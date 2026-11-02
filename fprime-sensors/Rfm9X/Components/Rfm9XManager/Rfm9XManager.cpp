// ======================================================================
// \title  Rfm9XManager.cpp
// \brief  cpp file for Rfm9XManager component implementation class
//
// F Prime handler implementations. Radio-specific register access and
// packet handling helpers live in Rfm9XHelpers.cpp.
// ======================================================================

#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManager.hpp"
#include <Os/Mutex.hpp>
#include <cstring>

namespace Rfm9X {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

Rfm9XManager ::Rfm9XManager(const char* const compName)
    : Rfm9XManagerComponentBase(compName),
      m_txActive(false),
      m_txState(TX_IDLE),
      m_txBuffer(),
      m_txContext(),
      m_txWaitTicks(0),
      m_pendingTxBuffers{},
      m_pendingTxContexts{},
      m_pendingTxCount(0),
      m_rxDeliverSize(0),
      m_rxPayload{},
      m_rxTxHoldoffTicks(0),
      m_txTxHoldoffTicks(0),
      m_txStarvedTicks(0),
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
      m_miso{} {}

Rfm9XManager ::~Rfm9XManager() {}

void Rfm9XManager ::parameterUpdated(FwPrmIdType id) {
    // F´ already stored the new value; defer register rewrite to the next
    // run tick, which re-reads params during CONFIGURE.
    (void)id;
    Os::ScopeLock lock(this->m_lock);
    if (this->m_state == READY) {
        this->m_state = CONFIGURE;
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
            accepted = this->startTransmit(data, context);
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
    TxProgress txProgress = TX_IN_PROGRESS;
    Fw::Buffer completedBuffer;
    ComCfg::FrameContext completedContext;
    bool txCompleted = false;
    Fw::Buffer dropBuffer;
    ComCfg::FrameContext dropContext;
    bool dropPending = false;
    bool resume = false;
    U8 rxPacket[MAX_PACKET_PAYLOAD];
    FwSizeType rxPacketSize = 0;
    {
    Os::ScopeLock lock(this->m_lock);
    if (this->m_txActive) {
        txProgress = this->advanceTransmit();
        if (txProgress != TX_IN_PROGRESS) {
            txCompleted = true;
            completedBuffer = this->m_txBuffer;
            completedContext = this->m_txContext;
            this->m_txBuffer = Fw::Buffer();
            this->m_txState = TX_IDLE;
            this->m_txActive = false;
            if (txProgress == TX_SUCCEEDED) {
                // Do not immediately re-enter dataIn through the synchronous
                // Com SUCCESS callback. Give the peer time to switch to RX
                // and reacquire the preamble.
                this->m_txTxHoldoffTicks = TX_TX_HOLDOFF_TICKS;
                this->m_comResumeNeeded = false;
            } else {
                this->m_comResumeNeeded = true;
            }
        }
    } else if (this->m_state == READY) {
        // Drain local GS hold before RX poll so uplink is not starved by TM.
        dropPending = this->tryStartPendingTransmit(dropBuffer, dropContext);
        if (!this->m_txActive) {
            this->pollReceive();
        }
    } else {
        // Bring-up advances here one bounded step per tick; also covers
        // RESET, parameter reconfigure, and failed-detect retries.
        this->initializeRadio();
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
        if (++this->m_txStarvedTicks >= RX_HOLDOFF_STARVATION_TICKS) {
            this->m_txStarvedTicks = 0;
            this->m_rxTxHoldoffTicks = 0;
        }
    } else {
        this->m_txStarvedTicks = 0;
    }
    // After holdoff decays, try a held GS uplink before emitting Com SUCCESS.
    if (!dropPending && !this->m_txActive && (this->m_state == READY)) {
        dropPending = this->tryStartPendingTransmit(dropBuffer, dropContext);
    }
    // Resume ComQueue after holdoff/TX FAILURE once the radio can accept TX.
    // A failed staged TX first reports FAILURE. Resume is deliberately deferred
    // to a later tick so ComRetry observes an ordered state transition rather
    // than FAILURE and SUCCESS in one synchronous callback chain.
    resume = txCompleted ? false : this->maybeResumeComStatus();
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
    if (txCompleted) {
        this->dataReturnOut_out(0, completedBuffer, completedContext);
        this->emitComStatus(txProgress == TX_SUCCEEDED ? Fw::Success::SUCCESS : Fw::Success::FAILURE);
    }
    if (dropPending) {
        this->dataReturnOut_out(0, dropBuffer, dropContext);
    }
    if (resume) {
        this->emitComStatus(Fw::Success::SUCCESS);
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
            if (this->isConnected_resetGpio_OutputPort(0) &&
                (this->resetGpio_out(0, Fw::Logic::HIGH) == Drv::GpioStatus::OP_OK)) {
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
                if (this->resetGpio_out(0, Fw::Logic::LOW) == Drv::GpioStatus::OP_OK) {
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
            if (this->detectRadio()) {
                this->m_state = CONFIGURE;
                this->reportRadioState();
            } else {
                this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            }
            break;
        case CONFIGURE:
            if (this->configureRadio() && this->requestMode(Mode::RX_CONTINUOUS)) {
                this->m_state = READY;
                this->reportRadioState();
                this->log_ACTIVITY_HI_RadioReady();
                // Com status is a one-time startup handshake. Sending an extra
                // SUCCESS after a hardware reset is invalid while the downstream
                // aggregator is already READY and causes it to assert.
                if (!this->m_comStatusAnnounced && this->isConnected_comStatusOut_OutputPort(0)) {
                    Fw::Success ready = Fw::Success::SUCCESS;
                    this->comStatusOut_out(0, ready);
                    this->m_comStatusAnnounced = true;
                }
            } else {
                this->log_WARNING_HI_ConfigurationFailed(Rfm9XMode::Receive);
            }
            break;
        case READY:
        default:
            break;
    }
}

void Rfm9XManager ::beginResetSequence() {
    this->m_state = RESET_ASSERT;
    this->m_resetTicks = 0;
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

bool Rfm9XManager ::startTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // One Com buffer → one RF packet (1..255). No radio-layer split/reassembly.
    const FwSizeType size = data.getSize();
    if ((data.getData() == nullptr) || (size == 0) || (size > MAX_PACKET_PAYLOAD)) {
        this->log_WARNING_HI_SendFailed(Rfm9XSendFailure::INVALID_SIZE);
        return false;
    }
    this->m_txBuffer = data;
    this->m_txContext = context;
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
    if (this->startTransmit(data, context)) {
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
    {
        Os::ScopeLock lock(this->m_lock);
        // Fail an in-flight TX immediately: the radio is about to be held in
        // hardware reset, so further TX SPI traffic is pointless.
        abortPending = this->m_txActive;
        if (abortPending) {
            abortBuffer = this->m_txBuffer;
            abortContext = this->m_txContext;
            this->m_txBuffer = Fw::Buffer();
            this->m_txState = TX_IDLE;
            this->m_txActive = false;
            this->m_comResumeNeeded = true;
        }
        // Tick-driven: the optional RST pulse and re-detect are advanced one
        // bounded step per run() tick, so no command-context blocking occurs.
        this->beginResetSequence();
    }
    if (abortPending) {
        this->dataReturnOut_out(0, abortBuffer, abortContext);
        this->emitComStatus(Fw::Success::FAILURE);
    }
    this->log_ACTIVITY_HI_ResetInitiated();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Rfm9XManager ::pollReceive() {
    U8 flags = 0;
    if (this->readRegister(Reg::IRQ_FLAGS, flags) != Drv::SpiStatus::SPI_OK) {
        return;
    }
    if ((flags & IrqFlags::RX_DONE) == 0) {
        return;
    }
    this->tryDeliverReceivedPacket();
}

void Rfm9XManager ::tryDeliverReceivedPacket() {
    // Re-read the flags byte that gated this call (pollReceive already
    // confirmed RxDone); PayloadCrcError is only meaningful alongside it.
    U8 flags = 0;
    if (this->readRegister(Reg::IRQ_FLAGS, flags) != Drv::SpiStatus::SPI_OK) {
        return;
    }
    // Clear RxDone (and any CRC error) before leaving: write 1 to clear
    // (datasheet section 6.4). Done regardless of outcome so RX_CONTINUOUS
    // is ready for the next packet.
    (void)this->writeRegister(Reg::IRQ_FLAGS, static_cast<U8>(IrqFlags::RX_DONE | IrqFlags::PAYLOAD_CRC_ERROR));

    if ((flags & IrqFlags::PAYLOAD_CRC_ERROR) != 0) {
        this->m_rxCrcErrors++;
        this->tlmWrite_RxCrcErrors(this->m_rxCrcErrors);
        return;
    }

    this->updateRssiSnr();

    U8 length = 0;
    U8 currentAddr = 0;
    if ((this->readRegister(Reg::RX_NB_BYTES, length) != Drv::SpiStatus::SPI_OK) ||
        (this->readRegister(Reg::FIFO_RX_CURRENT_ADDR, currentAddr) != Drv::SpiStatus::SPI_OK)) {
        return;
    }
    if ((length == 0) || (static_cast<FwSizeType>(length) > MAX_PACKET_PAYLOAD)) {
        return;
    }
    // Data Reception: move the SPI FIFO pointer to the start of the packet
    // just received before reading it out (datasheet section 4.1.5,
    // "Payload Data Extraction from FIFO").
    if (this->writeRegister(Reg::FIFO_ADDR_PTR, currentAddr) != Drv::SpiStatus::SPI_OK) {
        return;
    }
    if (!this->readFifo(this->m_rxPayload, length)) {
        return;
    }

    this->m_rxDeliverSize = length;
    this->m_packetsReceived++;
    this->tlmWrite_PacketsReceived(this->m_packetsReceived);
    // Keep the half-duplex channel clear for the next ground chunk.
    this->m_rxTxHoldoffTicks = RX_TX_HOLDOFF_TICKS;
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