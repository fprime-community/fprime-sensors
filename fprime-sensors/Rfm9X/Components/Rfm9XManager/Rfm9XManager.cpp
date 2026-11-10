// ======================================================================
// \title  Rfm9XManager.cpp
// \brief  cpp file for Rfm9XManager component implementation class
//
// F Prime handler implementations. Radio-specific register access and
// packet handling helpers live in Rfm9XHelpers.cpp.
// ======================================================================

#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManager.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManagerCfg/FppConstantsAc.hpp"
#include <Os/Mutex.hpp>
#include <cstring>

namespace Rfm9X {

// Out-of-class definitions for static constexpr members whose address is
// taken (e.g. by gtest's ASSERT_EQ in the unit tests), required pre-C++17.
constexpr U8 Rfm9XManager::FIFO_BASE_ADDR;
constexpr U32 Rfm9XManager::PENDING_TX_DEPTH;
constexpr FwSizeType Rfm9XManager::CONFIG_TABLE_SIZE;

namespace {
//! Round ms up to the nearest whole tick at `tickPeriodMs` per tick
U32 msToTicks(U32 ms, U32 tickPeriodMs) {
    return (ms + tickPeriodMs - 1) / tickPeriodMs;
}
}  // namespace

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

Rfm9XManager ::Rfm9XManager(const char* const compName)
    : Rfm9XManagerComponentBase(compName),
      m_spiOpOrigin(SPI_OP_NONE),
      m_configureStep(0),
      m_rxState(RX_IDLE),
      m_lastChannelBusy(false),
      m_rxIrqFlags(0),
      m_rxPacketLength(0),
      m_txActive(false),
      m_txState(TX_IDLE),
      m_txBuffer(),
      m_txContext(),
      m_txWaitTicks(0),
      m_txOrigin(TX_ORIGIN_COM),
      m_cmdPayload{},
      m_pendingTxBuffers{},
      m_pendingTxContexts{},
      m_pendingTxCount(0),
      m_rxDeliverSize(0),
      m_rxPayload{},
      m_rxTxHoldoffTicks(0),
      m_txTxHoldoffTicks(0),
      m_txStarvedTicks(0),
      m_txDoneTimeoutTicks(msToTicks(Rfm9X::ManagerCfg::TX_DONE_TIMEOUT_MS, 1)),
      m_rxTxHoldoffBoundTicks(msToTicks(Rfm9X::ManagerCfg::RX_TX_HOLDOFF_MS, 1)),
      m_txTxHoldoffBoundTicks(msToTicks(Rfm9X::ManagerCfg::TX_TX_HOLDOFF_MS, 1)),
      m_rxHoldoffStarvationTicks(msToTicks(Rfm9X::ManagerCfg::RX_HOLDOFF_STARVATION_MS, 1)),
      m_state(RESET_ASSERT),
      m_resetTicks(0),
      m_configured(false),
      m_packetsTransmitted(0),
      m_packetsReceived(0),
      m_rxCrcErrors(0),
      m_transmitEnabled(TransmitState::ENABLED),
      m_comStatusAnnounced(false),
      m_comResumeNeeded(false),
      m_mosi{},
      m_miso{},
      m_configTable{} {}

Rfm9XManager ::~Rfm9XManager() {}

void Rfm9XManager ::configure(U32 tickPeriodMs) {
    FW_ASSERT(tickPeriodMs > 0, static_cast<FwAssertArgType>(tickPeriodMs));
    this->m_txDoneTimeoutTicks = msToTicks(Rfm9X::ManagerCfg::TX_DONE_TIMEOUT_MS, tickPeriodMs);
    this->m_rxTxHoldoffBoundTicks = msToTicks(Rfm9X::ManagerCfg::RX_TX_HOLDOFF_MS, tickPeriodMs);
    this->m_txTxHoldoffBoundTicks = msToTicks(Rfm9X::ManagerCfg::TX_TX_HOLDOFF_MS, tickPeriodMs);
    this->m_rxHoldoffStarvationTicks = msToTicks(Rfm9X::ManagerCfg::RX_HOLDOFF_STARVATION_MS, tickPeriodMs);
}

void Rfm9XManager ::parameterUpdated(FwPrmIdType id) {
    // F´ already stored the new value; defer register rewrite to the next
    // run tick, which re-reads params during CONFIGURE.
    (void)id;
    Os::ScopeLock lock(this->m_lock);
    if (this->m_state == READY) {
        this->m_state = CONFIGURE;
        this->m_configureStep = 0;
    }
}

void Rfm9XManager ::parametersLoaded() {
    // No SPI or GPIO work here: bring-up (reset pulse, detect, configure) is
    // advanced one bounded step at a time by run() ticks.
    Os::ScopeLock lock(this->m_lock);
    this->m_configured = true;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Rfm9XManager ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Accepting a buffer only stages it. RF airtime is advanced by run(), so
    // this handler remains bounded even when a Com SUCCESS callback reaches it
    // synchronously from the 1 kHz rate-group thread.
    bool accepted = false;
    bool held = false;
    {
        Os::ScopeLock lock(this->m_lock);
        if (this->m_transmitEnabled == TransmitState::DISABLED) {
            // Keep a local hold on GS (no ComQueue); flight reports FAILURE so
            // ComQueue retries after TRANSMIT is re-enabled.
            if (this->usesLocalTxHold()) {
                held = this->enqueuePendingTransmit(data, context);
            } else {
                this->m_comResumeNeeded = true;
            }
        } else if ((this->m_state != READY) || this->m_txActive || this->channelBusy() ||
                   (this->usesLocalTxHold() && (this->m_txTxHoldoffTicks > 0))) {
            // Half-duplex / not ready: ComQueue gets FAILURE; GS holds & retries.
            // GS ignores RX_TX_HOLDOFF (continuous TM would starve uplink) but
            // honors TX_TX_HOLDOFF so bursts cannot send back-to-back packets.
            if (this->usesLocalTxHold()) {
                held = this->enqueuePendingTransmit(data, context);
            } else {
                // Expected while busy: no SendFailed event (a WARNING_HI here
                // re-enters ComQueue and can reorder ground DATA chunks).
                this->m_comResumeNeeded = true;
            }
        } else if (this->downlinkBlocked() && !this->usesLocalTxHold()) {
            // Flight ComQueue path still respects holdoff via FAILURE/resume.
            this->m_comResumeNeeded = true;
        } else if (this->usesLocalTxHold() && (this->m_pendingTxCount > 0)) {
            // Keep FIFO order: a new frame must not jump ahead of held frames.
            held = this->enqueuePendingTransmit(data, context);
        } else {
            accepted = this->startTransmit(data, context, TX_ORIGIN_COM);
            if (accepted) {
                this->m_comResumeNeeded = false;
            } else {
                // startTransmit only fails on empty/oversized payloads — never hold.
                this->m_comResumeNeeded = true;
            }
        }
    }
    if (!accepted && !held) {
        this->dataReturnOut_out(0, data, context);
        this->emitComStatus(Fw::Success::FAILURE);
    }
}

void Rfm9XManager ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->deallocate_out(0, data);
}

void Rfm9XManager ::run_handler(FwIndexType portNum, U32 context) {
    bool resume = false;
    U8 rxPacket[MAX_PACKET_PAYLOAD];
    FwSizeType rxPacketSize = 0;
    {
    Os::ScopeLock lock(this->m_lock);
    if (this->m_spiOpOrigin == SPI_OP_NONE) {
        // Only start a *new* bounded step when no SPI request is already
        // outstanding; progress on an outstanding one happens from
        // spiWriteReadComplete_handler() as soon as the driver's completion
        // callback fires, not from this tick.
        if (this->m_txActive) {
            (void)this->advanceTransmit();
        } else if (this->m_state == READY) {
            // Drain local GS hold before RX poll so uplink is not starved by TM.
            Fw::Buffer dropBuffer;
            ComCfg::FrameContext dropContext;
            if (this->tryStartPendingTransmit(dropBuffer, dropContext)) {
                this->dataReturnOut_out(0, dropBuffer, dropContext);
            } else if (!this->m_txActive) {
                this->pollReceive();
            }
        } else {
            // Bring-up advances here one bounded step per tick; also covers
            // RESET, parameter reconfigure, and failed-detect retries.
            this->initializeRadio();
        }
    }
    if (this->m_rxTxHoldoffTicks > 0) {
        this->m_rxTxHoldoffTicks--;
    }
    if (this->m_txTxHoldoffTicks > 0) {
        this->m_txTxHoldoffTicks--;
    }
    // Downlink fairness: each received packet re-arms the RX lease, so
    // periodic uplink (GDS keepalives, paced file chunks) can otherwise defer
    // a pending downlink forever. Bound the total wait and drop the lease.
    if (this->m_comResumeNeeded && (this->m_state == READY) &&
        (this->m_transmitEnabled == TransmitState::ENABLED) && (this->m_rxTxHoldoffTicks > 0)) {
        if (++this->m_txStarvedTicks >= this->m_rxHoldoffStarvationTicks) {
            this->m_txStarvedTicks = 0;
            this->m_rxTxHoldoffTicks = 0;
        }
    } else {
        this->m_txStarvedTicks = 0;
    }
    // Resume ComQueue after holdoff/TX FAILURE once the radio can accept TX.
    resume = this->maybeResumeComStatus();
    // Copy a completed RX packet out under the lock; allocation and delivery
    // to downstream components happen after the lock is released.
    if (this->m_rxDeliverSize > 0) {
        rxPacketSize = this->m_rxDeliverSize;
        this->m_rxDeliverSize = 0;
        (void)::memcpy(rxPacket, this->m_rxPayload, rxPacketSize);
    }
    }
    if (rxPacketSize > 0) {
        this->deliverReceivedPacket(rxPacket, rxPacketSize);
    }
    if (resume) {
        this->emitComStatus(Fw::Success::SUCCESS);
    }
}

void Rfm9XManager ::spiWriteReadComplete_handler(FwIndexType portNum, Fw::Buffer& writeBuffer,
                                                  Fw::Buffer& readBuffer, const Drv::SpiStatus& status) {
    (void)writeBuffer;
    (void)readBuffer;
    TxProgress txProgress = TX_IN_PROGRESS;
    Fw::Buffer completedBuffer;
    ComCfg::FrameContext completedContext;
    bool txCompleted = false;
    bool txCompletedIsCom = false;
    Fw::Buffer dropBuffer;
    ComCfg::FrameContext dropContext;
    bool dropPending = false;
    U8 rxPacket[MAX_PACKET_PAYLOAD];
    FwSizeType rxPacketSize = 0;
    {
    Os::ScopeLock lock(this->m_lock);
    // A RESET (or any other event that calls beginResetSequence()) may have
    // discarded the sequence that issued this request before the driver's
    // completion arrived; the request was still answered (every request
    // is), but there is nothing to resume.
    const SpiOpOrigin origin = this->m_spiOpOrigin;
    this->m_spiOpOrigin = SPI_OP_NONE;
    if (origin != SPI_OP_NONE) {
        switch (origin) {
            case SPI_OP_INIT:
                this->continueInitializeRadio(status);
                break;
            case SPI_OP_TX:
                txProgress = this->continueAdvanceTransmit(status);
                if (txProgress != TX_IN_PROGRESS) {
                    txCompleted = true;
                    txCompletedIsCom = (this->m_txOrigin == TX_ORIGIN_COM);
                    completedBuffer = this->m_txBuffer;
                    completedContext = this->m_txContext;
                    this->m_txBuffer = Fw::Buffer();
                    this->m_txState = TX_IDLE;
                    this->m_txActive = false;
                    if (txProgress == TX_SUCCEEDED) {
                        // Do not immediately re-enter dataIn through the synchronous
                        // Com SUCCESS callback. Give the peer time to switch to RX
                        // and reacquire the preamble. RF-level pacing applies
                        // regardless of origin: it protects physical radio turnaround,
                        // not the Com protocol.
                        this->m_txTxHoldoffTicks = this->m_txTxHoldoffBoundTicks;
                    }
                    // m_comResumeNeeded tracks an outstanding Com FAILURE/SUCCESS
                    // obligation; a command-originated buffer was never reported to
                    // Com in the first place (no dataReturnOut/comStatusOut below),
                    // so it must not perturb that bookkeeping either way.
                    if (txCompletedIsCom) {
                        this->m_comResumeNeeded = (txProgress != TX_SUCCEEDED);
                    }
                }
                break;
            case SPI_OP_RX:
                this->continueReceivePoll(status);
                break;
            case SPI_OP_NONE:
            default:
                break;
        }

        // After holdoff decays, try a held GS uplink before emitting Com SUCCESS.
        if (!dropPending && !this->m_txActive && (this->m_state == READY)) {
            dropPending = this->tryStartPendingTransmit(dropBuffer, dropContext);
        }
        if (this->m_rxDeliverSize > 0) {
            rxPacketSize = this->m_rxDeliverSize;
            this->m_rxDeliverSize = 0;
            (void)::memcpy(rxPacket, this->m_rxPayload, rxPacketSize);
        }
    }
    }
    if (rxPacketSize > 0) {
        this->deliverReceivedPacket(rxPacket, rxPacketSize);
    }
    if (txCompleted && txCompletedIsCom) {
        this->dataReturnOut_out(0, completedBuffer, completedContext);
        this->emitComStatus(txProgress == TX_SUCCEEDED ? Fw::Success::SUCCESS : Fw::Success::FAILURE);
    }
    if (dropPending) {
        this->dataReturnOut_out(0, dropBuffer, dropContext);
    }
}

void Rfm9XManager ::emitComStatus(Fw::Success status) {
    if (this->isConnected_comStatusOut_OutputPort(0)) {
        this->comStatusOut_out(0, status);
    }
}

bool Rfm9XManager ::maybeResumeComStatus() {
    if (!this->m_comResumeNeeded) {
        return false;
    }
    if (this->m_transmitEnabled != TransmitState::ENABLED) {
        return false;
    }
    if (this->m_state != READY || this->downlinkBlocked()) {
        return false;
    }
    this->m_comResumeNeeded = false;
    return true;
}

// ----------------------------------------------------------------------
// Helper functions: radio state management
// ----------------------------------------------------------------------

void Rfm9XManager ::initializeRadio() {
    // Do not touch the bus before the deployment has loaded its parameters.
    if (!this->m_configured) {
        return;
    }
    switch (this->m_state) {
        case RESET_ASSERT:
            // Optional hardware RST (recommended when a reset GPIO is wired —
            // cold power-up / shared-SPI glitches can leave the radio in a bad
            // state). Never required: skip if unconnected or the write fails.
            // NRESET is active-low (datasheet 7.2.2): drive LOW to assert.
            if (this->isConnected_resetGpio_OutputPort(0) &&
                (this->resetGpio_out(0, Fw::Logic::LOW) == Drv::GpioStatus::OP_OK)) {
                this->m_resetTicks = 0;
                this->m_state = RESET_HOLD;
            } else {
                this->m_state = DETECT;
            }
            this->reportRadioState();
            break;
        case RESET_HOLD:
            if (++this->m_resetTicks >= RESET_HOLD_TICKS) {
                // Retry the release next tick on failure: advancing with RST
                // still asserted would leave the radio in reset and mask the
                // GPIO fault as a generic detection failure.
                if (this->resetGpio_out(0, Fw::Logic::HIGH) == Drv::GpioStatus::OP_OK) {
                    this->m_resetTicks = 0;
                    this->m_state = RESET_SETTLE;
                }
            }
            break;
        case RESET_SETTLE:
            if (++this->m_resetTicks >= RESET_SETTLE_TICKS) {
                this->m_state = DETECT;
                this->reportRadioState();
            }
            break;
        case DETECT:
            this->issueDetectRadio();
            break;
        case CONFIGURE:
            this->continueInitializeRadio(Drv::SpiStatus::SPI_OK);  // first step: no prior op to check
            break;
        case READY:
        default:
            break;
    }
}

void Rfm9XManager ::continueInitializeRadio(const Drv::SpiStatus& status) {
    // DETECT's single read completes here.
    if (this->m_state == DETECT) {
        const bool ok = (status == Drv::SpiStatus::SPI_OK) && (this->m_miso[1] == VERSION_VALUE);
        if (ok) {
            this->m_state = CONFIGURE;
            this->m_configureStep = 0;
            this->reportRadioState();
        } else {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
        }
        return;
    }

    FW_ASSERT(this->m_state == CONFIGURE, static_cast<FwAssertArgType>(this->m_state));

    // Step 0 is special-cased: initializeRadio() calls in here with a
    // placeholder SPI_OK status (there is no prior op on the very first
    // entry), so step 0 must not interpret that as "the previous write
    // succeeded." Steps 1+ always come from a real completion.
    const bool ok = (this->m_configureStep == 0) || (status == Drv::SpiStatus::SPI_OK);
    if (!ok) {
        this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
        this->m_configureStep = 0;
        return;
    }

    // enterLoRaMode()'s three writes (datasheet section 6.4, RegOpMode note):
    // SLEEP -> LongRangeMode|SLEEP -> LongRangeMode|STANDBY.
    if (this->m_configureStep == 0) {
        this->issueWriteRegister(Reg::OP_MODE, Mode::SLEEP, SPI_OP_INIT);
        this->m_configureStep = 1;
        return;
    }
    if (this->m_configureStep == 1) {
        this->issueWriteRegister(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::SLEEP), SPI_OP_INIT);
        this->m_configureStep = 2;
        return;
    }
    if (this->m_configureStep == 2) {
        this->issueWriteRegister(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::STANDBY), SPI_OP_INIT);
        this->m_configureStep = 3;
        return;
    }
    if (this->m_configureStep == 3) {
        // enterLoRaMode() just completed. Build the modem-profile table from
        // current FPP parameters now, once, before walking it.
        Fw::ParamValid isValid = Fw::ParamValid::INVALID;
        const SpreadingFactor sfParam = this->paramGet_SPREADING_FACTOR(isValid);
        if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }
        const Bandwidth bwParam = this->paramGet_BANDWIDTH(isValid);
        if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }
        const CodingRate crParam = this->paramGet_CODING_RATE(isValid);
        if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }
        const U32 freqParam = this->paramGet_FREQUENCY_HZ(isValid);
        if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }
        const Rfm9XTxPower txPowerParam = this->paramGet_TX_POWER(isValid);
        if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }

        U8 sfReg = 0;
        U8 bwReg = 0;
        U8 crReg = 0;
        TxPowerSetting power{};
        if (!getSpreadingFactorReg(sfParam, sfReg) || !getBandwidthReg(bwParam, bwReg) ||
            !getCodingRateReg(crParam, crReg) || !getTxPowerSetting(txPowerParam, power)) {
            this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            this->m_configureStep = 0;
            return;
        }

        const U32 frf = frequencyToFrf(freqParam);
        const ConfigEntry table[CONFIG_TABLE_SIZE] = {
            {Reg::FRF_MSB, static_cast<U8>(frf >> 16)},
            {Reg::FRF_MID, static_cast<U8>(frf >> 8)},
            {Reg::FRF_LSB, static_cast<U8>(frf & 0xFF)},
            {Reg::PA_CONFIG, power.paConfig},
            {Reg::OCP, power.ocp},
            {Reg::PA_DAC, power.paDac},
            // RegModemConfig1: Bw | CodingRate | ImplicitHeaderModeOn=0 (explicit header)
            {Reg::MODEM_CONFIG_1, static_cast<U8>(bwReg | crReg)},
            // RegModemConfig2: SpreadingFactor | TxContinuousMode=0 | RxPayloadCrcOn=1 | SymbTimeout(9:8)=0
            {Reg::MODEM_CONFIG_2, static_cast<U8>(static_cast<U8>(sfReg << 4) | 0x04)},
            {Reg::SYMB_TIMEOUT_LSB, 0x64},  // datasheet reset default (100 symbols)
            {Reg::PREAMBLE_MSB, static_cast<U8>(FIXED_PREAMBLE_LENGTH >> 8)},
            {Reg::PREAMBLE_LSB, static_cast<U8>(FIXED_PREAMBLE_LENGTH & 0xFF)},
            {Reg::MAX_PAYLOAD_LENGTH, static_cast<U8>(MAX_PACKET_PAYLOAD)},
            {Reg::FIFO_TX_BASE_ADDR, FIFO_BASE_ADDR},
            {Reg::FIFO_RX_BASE_ADDR, FIFO_BASE_ADDR},
            {Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE},  // Default to RX mapping; TX flips this before each send
        };
        (void)::memcpy(this->m_configTable, table, sizeof(table));

        this->issueWriteRegister(this->m_configTable[0].address, this->m_configTable[0].value, SPI_OP_INIT);
        this->m_configureStep = 4;
        return;
    }

    // Steps 4..4+CONFIG_TABLE_SIZE-2 walk m_configTable[1..CONFIG_TABLE_SIZE-1]
    // (index 0 was already issued by step 3 above).
    const U32 tableIndex = this->m_configureStep - 3;
    if (tableIndex < CONFIG_TABLE_SIZE) {
        this->issueWriteRegister(this->m_configTable[tableIndex].address, this->m_configTable[tableIndex].value,
                                 SPI_OP_INIT);
        this->m_configureStep++;
        return;
    }

    const U32 irqClearStep = CONFIG_TABLE_SIZE + 3;
    if (this->m_configureStep == irqClearStep) {
        // Clear any IRQ residue from before configuration (write 1 to clear, datasheet section 6.4)
        this->issueWriteRegister(Reg::IRQ_FLAGS, 0xFF, SPI_OP_INIT);
        this->m_configureStep++;
        return;
    }

    const U32 finalModeStep = irqClearStep + 1;
    if (this->m_configureStep == finalModeStep) {
        this->issueRequestMode(Mode::RX_CONTINUOUS, SPI_OP_INIT);
        this->m_configureStep++;
        return;
    }

    // Last step's completion: RX_CONTINUOUS was requested successfully.
    this->m_state = READY;
    this->m_configureStep = 0;
    this->reportRadioState();
    this->log_ACTIVITY_HI_RadioReady();
    // Com status is a one-time startup handshake. Sending an extra SUCCESS
    // after a hardware reset is invalid while the downstream aggregator is
    // already READY and causes it to assert.
    if (!this->m_comStatusAnnounced && this->isConnected_comStatusOut_OutputPort(0)) {
        Fw::Success ready = Fw::Success::SUCCESS;
        this->comStatusOut_out(0, ready);
        this->m_comStatusAnnounced = true;
    }
}

void Rfm9XManager ::beginResetSequence() {
    this->m_state = RESET_ASSERT;
    this->m_resetTicks = 0;
    this->m_configureStep = 0;
    this->m_spiOpOrigin = SPI_OP_NONE;
    this->m_rxState = RX_IDLE;
    this->reportRadioState();
}

void Rfm9XManager ::reportRadioState() {
    Rfm9XRadioState state = Rfm9XRadioState::NOT_STARTED;
    switch (this->m_state) {
        case RESET_ASSERT:
        case RESET_HOLD:
        case RESET_SETTLE:
            state = Rfm9XRadioState::RESETTING;
            break;
        case DETECT:
            state = Rfm9XRadioState::DETECT;
            break;
        case CONFIGURE:
            state = Rfm9XRadioState::CONFIGURE;
            break;
        case READY:
            state = Rfm9XRadioState::READY;
            break;
        default:
            break;
    }
    this->tlmWrite_RadioState(state);
}

bool Rfm9XManager ::startTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context, TxOrigin origin) {
    // One Com buffer → one RF packet (1..255). No radio-layer split/reassembly.
    const FwSizeType size = data.getSize();
    if ((data.getData() == nullptr) || (size == 0) || (size > MAX_PACKET_PAYLOAD)) {
        this->log_WARNING_HI_SendFailed(Rfm9XSendFailure::INVALID_SIZE);
        return false;
    }
    this->m_txBuffer = data;
    this->m_txContext = context;
    this->m_txOrigin = origin;
    this->m_txWaitTicks = 0;
    this->m_txState = TX_REQUEST_STANDBY;
    this->m_txActive = true;
    return true;
}

bool Rfm9XManager ::usesLocalTxHold() const {
    // Flight wires comStatusOut into ComQueue/ComRetry. The Feather GS does not,
    // so a busy-radio reject would otherwise permanently drop uplink frames.
    return !this->isConnected_comStatusOut_OutputPort(0);
}

bool Rfm9XManager ::enqueuePendingTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    if (this->m_pendingTxCount >= PENDING_TX_DEPTH) {
        this->log_WARNING_HI_SendFailed(Rfm9XSendFailure::QUEUE_FULL);
        return false;
    }
    this->m_pendingTxBuffers[this->m_pendingTxCount] = data;
    this->m_pendingTxContexts[this->m_pendingTxCount] = context;
    this->m_pendingTxCount++;
    return true;
}

bool Rfm9XManager ::tryStartPendingTransmit(Fw::Buffer& dropBuffer, ComCfg::FrameContext& dropContext) {
    if ((this->m_pendingTxCount == 0) || this->m_txActive) {
        return false;
    }
    if (this->m_transmitEnabled != TransmitState::ENABLED) {
        return false;
    }
    // Pace pending GS uplinks with the post-TX quiet window. RX holdoff is
    // ignored here so continuous TM cannot pin the pending queue. TX holdoff
    // is required so a UART burst cannot drain back-to-back.
    if ((this->m_state != READY) || this->channelBusy() || (this->m_txTxHoldoffTicks > 0)) {
        return false;
    }
    Fw::Buffer data = this->m_pendingTxBuffers[0];
    ComCfg::FrameContext context = this->m_pendingTxContexts[0];
    for (U32 i = 1; i < this->m_pendingTxCount; i++) {
        this->m_pendingTxBuffers[i - 1] = this->m_pendingTxBuffers[i];
        this->m_pendingTxContexts[i - 1] = this->m_pendingTxContexts[i];
    }
    this->m_pendingTxCount--;
    if (this->startTransmit(data, context, TX_ORIGIN_COM)) {
        return false;
    }
    // Permanent reject (empty/oversized): caller returns ownership after unlock.
    dropBuffer = data;
    dropContext = context;
    return true;
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void Rfm9XManager ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Rfm9X::TransmitState& enabled) {
    bool resume = false;
    {
        Os::ScopeLock lock(this->m_lock);
        if (enabled == TransmitState::ENABLED) {
            // Only resume if a FAILURE was actually reported while disabled: an
            // unsolicited SUCCESS while ComQueue is READY asserts downstream.
            if ((this->m_transmitEnabled == TransmitState::DISABLED) && this->m_comResumeNeeded) {
                this->m_comResumeNeeded = false;
                resume = true;
            }
            this->m_transmitEnabled = TransmitState::ENABLED;
        } else {
            this->m_transmitEnabled = TransmitState::DISABLED;
        }
    }
    if (resume) {
        this->emitComStatus(Fw::Success::SUCCESS);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Rfm9XManager ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Buffer abortBuffer;
    ComCfg::FrameContext abortContext;
    bool abortPending = false;
    bool abortIsCom = false;
    {
        Os::ScopeLock lock(this->m_lock);
        // Fail an in-flight TX immediately: the radio is about to be held in
        // hardware reset, so further TX SPI traffic is pointless. Any SPI
        // request already issued for it may still complete later, but
        // beginResetSequence() clears m_spiOpOrigin so that completion is
        // dropped as stale (see spiWriteReadComplete_handler()).
        abortPending = this->m_txActive;
        if (abortPending) {
            abortIsCom = (this->m_txOrigin == TX_ORIGIN_COM);
            abortBuffer = this->m_txBuffer;
            abortContext = this->m_txContext;
            this->m_txBuffer = Fw::Buffer();
            this->m_txState = TX_IDLE;
            this->m_txActive = false;
            if (abortIsCom) {
                this->m_comResumeNeeded = true;
            }
        }
        // Tick-driven: the optional RST pulse and re-detect are advanced one
        // bounded step per run() tick, so no command-context blocking occurs.
        this->beginResetSequence();
    }
    if (abortPending && abortIsCom) {
        this->dataReturnOut_out(0, abortBuffer, abortContext);
        this->emitComStatus(Fw::Success::FAILURE);
    }
    this->log_ACTIVITY_HI_ResetInitiated();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Rfm9XManager ::SEND_STRING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& payload) {
    const FwSizeType size = payload.length();
    bool accepted = false;
    Rfm9XSendFailure failureReason = Rfm9XSendFailure::NOT_READY;
    {
        Os::ScopeLock lock(this->m_lock);
        if ((size == 0) || (size > MAX_PACKET_PAYLOAD)) {
            failureReason = Rfm9XSendFailure::INVALID_SIZE;
        } else if ((this->m_state != READY) || this->m_txActive ||
                   (this->m_transmitEnabled != TransmitState::ENABLED) || this->downlinkBlocked()) {
            // downlinkBlocked() already covers channelBusy()/holdoff checks.
            failureReason = Rfm9XSendFailure::NOT_READY;
        } else {
            (void)::memcpy(this->m_cmdPayload, payload.toChar(), size);
            Fw::Buffer buffer(this->m_cmdPayload, size);
            ComCfg::FrameContext cmdContext;
            accepted = this->startTransmit(buffer, cmdContext, TX_ORIGIN_COMMAND);
        }
    }
    if (accepted) {
        this->log_ACTIVITY_HI_TestPayloadQueued(size);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->log_WARNING_HI_SendFailed(failureReason);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Receive sequence: one SPI op per step, replacing the old synchronous
// pollReceive()/tryDeliverReceivedPacket() (which issued up to 8 sequential
// SPI transactions in a single call).
// ----------------------------------------------------------------------

void Rfm9XManager ::pollReceive() {
    FW_ASSERT(this->m_rxState == RX_IDLE, static_cast<FwAssertArgType>(this->m_rxState));
    this->issueReadRegister(Reg::MODEM_STAT, SPI_OP_RX);
    this->m_rxState = RX_CHECK_BUSY;
}

void Rfm9XManager ::continueReceivePoll(const Drv::SpiStatus& status) {
    const bool ok = (status == Drv::SpiStatus::SPI_OK);

    switch (this->m_rxState) {
        case RX_CHECK_BUSY: {
            // A bus fault reads as "clear" so the transmit path is taken and
            // its own SPI failure handling reports RADIO_FAULT instead of
            // silently deferring downlink forever on a dead bus.
            const U8 stat = ok ? this->m_miso[1] : 0;
            this->m_lastChannelBusy =
                ok && ((stat & static_cast<U8>(ModemStat::RX_ONGOING | ModemStat::SIGNAL_DETECTED)) != 0);
            this->issueReadRegister(Reg::IRQ_FLAGS, SPI_OP_RX);
            this->m_rxState = RX_CHECK_FLAGS;
            break;
        }

        case RX_CHECK_FLAGS: {
            if (!ok) {
                this->m_rxState = RX_IDLE;
                break;
            }
            const U8 flags = this->m_miso[1];
            if ((flags & IrqFlags::RX_DONE) == 0) {
                this->m_rxState = RX_IDLE;
                break;
            }
            this->m_rxIrqFlags = flags;
            // Clear RxDone (and any CRC error) before leaving: write 1 to
            // clear (datasheet section 6.4). Done regardless of outcome so
            // RX_CONTINUOUS is ready for the next packet.
            this->issueWriteRegister(Reg::IRQ_FLAGS,
                                     static_cast<U8>(IrqFlags::RX_DONE | IrqFlags::PAYLOAD_CRC_ERROR), SPI_OP_RX);
            this->m_rxState = RX_CLEAR_FLAGS;
            break;
        }

        case RX_CLEAR_FLAGS:
            if ((this->m_rxIrqFlags & IrqFlags::PAYLOAD_CRC_ERROR) != 0) {
                this->m_rxCrcErrors++;
                this->tlmWrite_RxCrcErrors(this->m_rxCrcErrors);
                this->m_rxState = RX_IDLE;
                break;
            }
            this->issueReadRegister(Reg::PKT_RSSI_VALUE, SPI_OP_RX);
            this->m_rxState = RX_READ_RSSI;
            break;

        case RX_READ_RSSI:
            if (ok) {
                this->applyRssiReading(this->m_miso[1]);
            }
            this->issueReadRegister(Reg::PKT_SNR_VALUE, SPI_OP_RX);
            this->m_rxState = RX_READ_SNR;
            break;

        case RX_READ_SNR:
            if (ok) {
                this->applySnrReading(this->m_miso[1]);
            }
            this->issueReadRegister(Reg::RX_NB_BYTES, SPI_OP_RX);
            this->m_rxState = RX_READ_LENGTH;
            break;

        case RX_READ_LENGTH: {
            if (!ok) {
                this->m_rxState = RX_IDLE;
                break;
            }
            const U8 length = this->m_miso[1];
            if ((length == 0) || (static_cast<FwSizeType>(length) > MAX_PACKET_PAYLOAD)) {
                this->m_rxState = RX_IDLE;
                break;
            }
            this->m_rxPacketLength = length;
            this->issueReadRegister(Reg::FIFO_RX_CURRENT_ADDR, SPI_OP_RX);
            this->m_rxState = RX_READ_CUR_ADDR;
            break;
        }

        case RX_READ_CUR_ADDR: {
            if (!ok) {
                this->m_rxState = RX_IDLE;
                break;
            }
            // Data Reception: move the SPI FIFO pointer to the start of the
            // packet just received before reading it out (datasheet section
            // 4.1.5, "Payload Data Extraction from FIFO").
            const U8 currentAddr = this->m_miso[1];
            this->issueWriteRegister(Reg::FIFO_ADDR_PTR, currentAddr, SPI_OP_RX);
            this->m_rxState = RX_SET_FIFO_PTR;
            break;
        }

        case RX_SET_FIFO_PTR:
            if (!ok) {
                this->m_rxState = RX_IDLE;
                break;
            }
            this->issueReadFifo(this->m_rxPacketLength, SPI_OP_RX);
            this->m_rxState = RX_READ_FIFO;
            break;

        case RX_READ_FIFO:
            if (ok) {
                (void)::memcpy(this->m_rxPayload, &this->m_miso[1], this->m_rxPacketLength);
                this->m_rxDeliverSize = this->m_rxPacketLength;
                this->m_packetsReceived++;
                this->tlmWrite_PacketsReceived(this->m_packetsReceived);
                // Keep the half-duplex channel clear for the next ground chunk.
                this->m_rxTxHoldoffTicks = this->m_rxTxHoldoffBoundTicks;
            }
            this->m_rxState = RX_IDLE;
            break;

        case RX_IDLE:
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_rxState));
            break;
    }
}

void Rfm9XManager ::deliverReceivedPacket(const U8* data, FwSizeType size) {
    Fw::Buffer buffer = this->allocate_out(0, size);
    if (buffer.getSize() < size) {
        this->log_WARNING_HI_AllocationFailed(size);
        this->deallocate_out(0, buffer);
        return;
    }
    (void)::memcpy(buffer.getData(), data, size);
    buffer.setSize(size);
    ComCfg::FrameContext context;
    this->dataOut_out(0, buffer, context);
}

}  // namespace Rfm9X
