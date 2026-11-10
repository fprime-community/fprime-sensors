// ======================================================================
// \title  Rfm9XManagerTester.cpp
// \brief  cpp file for Rfm9XManager component test harness implementation class
// ======================================================================

#include "Rfm9XManagerTester.hpp"

namespace Rfm9X {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

Rfm9XManagerTester ::Rfm9XManagerTester(bool localHold)
    : Rfm9XManagerGTestBase("Rfm9XManagerTester", Rfm9XManagerTester::MAX_HISTORY_SIZE),
      component("Rfm9XManager"),
      m_model(),
      m_spiFail(false),
      m_allocFail(false),
      m_allocation{},
      m_pendingCompletionValid(false),
      m_pendingPortNum(0),
      m_pendingWriteBuffer(),
      m_pendingReadBuffer(),
      m_pendingStatus(Drv::SpiStatus::SPI_OK) {
    this->initComponents();
    if (localHold) {
        this->connectPortsLocalHold();
    } else {
        this->connectPorts();
    }
}

void Rfm9XManagerTester ::connectPortsLocalHold() {
    // Mirrors the autocoded connectPorts() minus comStatusOut: the component
    // detects the unconnected port and selects local pending-TX hold mode.
    this->connect_to_CmdDisp(0, this->component.get_CmdDisp_InputPort(0));
    this->component.set_CmdReg_OutputPort(0, this->get_from_CmdReg(0));
    this->component.set_CmdStatus_OutputPort(0, this->get_from_CmdStatus(0));
    this->component.set_logOut_OutputPort(0, this->get_from_logOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
    this->component.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
#endif
    this->component.set_prmGet_OutputPort(0, this->get_from_prmGet(0));
    this->component.set_prmSet_OutputPort(0, this->get_from_prmSet(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    this->connect_to_dataIn(0, this->component.get_dataIn_InputPort(0));
    this->connect_to_dataReturnIn(0, this->component.get_dataReturnIn_InputPort(0));
    this->connect_to_run(0, this->component.get_run_InputPort(0));
    this->component.set_allocate_OutputPort(0, this->get_from_allocate(0));
    this->component.set_dataOut_OutputPort(0, this->get_from_dataOut(0));
    this->component.set_dataReturnOut_OutputPort(0, this->get_from_dataReturnOut(0));
    this->component.set_deallocate_OutputPort(0, this->get_from_deallocate(0));
    this->component.set_resetGpio_OutputPort(0, this->get_from_resetGpio(0));
    this->component.set_spiWriteRead_OutputPort(0, this->get_from_spiWriteRead(0));
    this->connect_to_spiWriteReadComplete(0, this->component.get_spiWriteReadComplete_InputPort(0));
}

Rfm9XManagerTester ::~Rfm9XManagerTester() {}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void Rfm9XManagerTester ::from_spiWriteRead_handler(FwIndexType portNum,
                                                    Fw::Buffer& writeBuffer,
                                                    Fw::Buffer& readBuffer) {
    this->pushFromPortEntry_spiWriteRead(writeBuffer, readBuffer);
    Drv::SpiStatus status = Drv::SpiStatus::SPI_OK;
    if (this->m_spiFail) {
        status = Drv::SpiStatus::SPI_WRITE_ERR;
    } else {
        EXPECT_EQ(writeBuffer.getSize(), readBuffer.getSize());
        this->m_model.spiTransaction(writeBuffer.getData(), readBuffer.getData(), writeBuffer.getSize());
    }
    // Must NOT call invoke_to_spiWriteReadComplete() here: this handler can
    // run while the component already holds its own m_lock (e.g. issued
    // from inside spiWriteReadComplete_handler's own continueXxx() chaining
    // the next step) -- calling back into spiWriteReadComplete_handler
    // synchronously would re-lock that same non-recursive mutex from the
    // same call stack. A real async driver never has this problem (the
    // completion always arrives from a separate, later call via poll()),
    // so the harness defers delivery the same way: stash it here, and
    // runOnce()'s drain delivers it afterward, outside any component lock.
    FW_ASSERT(!this->m_pendingCompletionValid);
    this->m_pendingCompletionValid = true;
    this->m_pendingPortNum = portNum;
    this->m_pendingWriteBuffer = writeBuffer;
    this->m_pendingReadBuffer = readBuffer;
    this->m_pendingStatus = status;
}

void Rfm9XManagerTester ::invoke_to_run(FwIndexType portNum, U32 context) {
    // Shadows (hides, via same-name lookup) Rfm9XManagerTesterBase's own
    // invoke_to_run(): every one of this file's many run-tick loops already
    // calls `this->invoke_to_run(0, 0)`, so routing all of them through here
    // -- tick, then deliver exactly one pending SPI completion -- requires
    // no changes at those call sites. See from_spiWriteRead_handler()'s
    // comment for why delivery must be deferred to here instead of
    // happening inline inside that handler.
    this->Rfm9XManagerTesterBase::invoke_to_run(portNum, context);
    this->drainOnePendingCompletion();
}

void Rfm9XManagerTester ::drainOnePendingCompletion() {
    if (!this->m_pendingCompletionValid) {
        return;
    }
    this->m_pendingCompletionValid = false;
    const FwIndexType portNum = this->m_pendingPortNum;
    Fw::Buffer writeBuffer = this->m_pendingWriteBuffer;
    Fw::Buffer readBuffer = this->m_pendingReadBuffer;
    const Drv::SpiStatus status = this->m_pendingStatus;
    // Delivering this may itself chain into issuing the *next* request
    // (continueXxx() calling another issueYyy()), which lands back in
    // from_spiWriteRead_handler() and sets a new pending completion -- but
    // that one is intentionally left undelivered until the next runOnce(),
    // so each external tick advances the sequence by exactly one SPI op,
    // matching real hardware's one-op-per-callback pacing.
    this->invoke_to_spiWriteReadComplete(portNum, writeBuffer, readBuffer, status);
}

Fw::Buffer Rfm9XManagerTester ::from_allocate_handler(FwIndexType portNum, FwSizeType size) {
    this->pushFromPortEntry_allocate(size);
    if (this->m_allocFail || (size > sizeof this->m_allocation)) {
        return Fw::Buffer();
    }
    return Fw::Buffer(this->m_allocation, size);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void Rfm9XManagerTester ::setParameters(const SpreadingFactor& sf,
                                        const Bandwidth& bw,
                                        const CodingRate& cr,
                                        const Rfm9XTxPower& txPower) {
    this->paramSet_SPREADING_FACTOR(sf, Fw::ParamValid::VALID);
    this->paramSet_BANDWIDTH(bw, Fw::ParamValid::VALID);
    this->paramSet_CODING_RATE(cr, Fw::ParamValid::VALID);
    this->paramSet_FREQUENCY_HZ(434000000U, Fw::ParamValid::VALID);
    this->paramSet_TX_POWER(txPower, Fw::ParamValid::VALID);
}

void Rfm9XManagerTester ::setDefaultParameters() {
    // Match FPP defaults: SF7 / BW_125_KHZ / CR_4_5 / 434 MHz / DBM_13.
    this->setParameters(SpreadingFactor::SF7, Bandwidth::BW_125_KHZ, CodingRate::CR_4_5, Rfm9XTxPower::DBM_13);
}

void Rfm9XManagerTester ::makeReady() {
    this->setDefaultParameters();
    this->component.loadParameters();
    this->runUntilReady();
    ASSERT_EVENTS_ConfigurationFailed_SIZE(0);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    this->clearHistory();
}

void Rfm9XManagerTester ::runTicks(U32 count) {
    for (U32 tick = 0; tick < count; tick++) {
        this->invoke_to_run(0, 0);
    }
}

void Rfm9XManagerTester ::runUntilReady(U32 maxTicks) {
    // Bring-up is tick-driven: reset pulse, settle, detect, then configure.
    for (U32 tick = 0; tick < maxTicks; tick++) {
        this->invoke_to_run(0, 0);
        if (this->eventsSize_RadioReady > 0) {
            return;
        }
    }
    FAIL() << "RFM9X bring-up did not reach READY within " << maxTicks << " run ticks";
}

void Rfm9XManagerTester ::runUntilTransmitCompletes(U32 maxTicks) {
    for (U32 tick = 0; tick < maxTicks; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_comStatusOut->size() > 0) {
            return;
        }
    }
    FAIL() << "staged RFM9X transmit did not complete within " << maxTicks << " run ticks";
}

void Rfm9XManagerTester ::runUntilChannelBusyRefreshed() {
    // channelBusy() is now a cached accessor (m_lastChannelBusy), refreshed
    // only by the RX sequence's RX_IDLE->RX_CHECK_BUSY->RX_CHECK_FLAGS step
    // pair -- a MODEM_STAT read followed by its completion. Two ticks is
    // always enough while idle (not mid-TX): one issues the read, the next
    // delivers its completion and updates the cache.
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void Rfm9XManagerTester ::test_initialization() {
    // Before FPP parameters are loaded, run must not touch the radio.
    this->invoke_to_run(0, 0);
    ASSERT_from_spiWriteRead_SIZE(0);
    ASSERT_from_resetGpio_SIZE(0);
    ASSERT_EVENTS_SIZE(0);

    // Loading parameters performs no bus work in command/registration context;
    // the tick-driven bring-up sequence then reaches READY.
    this->setDefaultParameters();
    this->component.loadParameters();
    ASSERT_from_spiWriteRead_SIZE(0);
    ASSERT_from_resetGpio_SIZE(0);
    this->runUntilReady();
    ASSERT_EVENTS_ConfigurationFailed_SIZE(0);
    ASSERT_EVENTS_RadioReady_SIZE(1);
    // Optional RST GPIO is connected in the harness: expect one LOW (assert) then HIGH (release) pulse.
    ASSERT_from_resetGpio_SIZE(2);
    ASSERT_from_resetGpio(0, Fw::Logic::LOW);
    ASSERT_from_resetGpio(1, Fw::Logic::HIGH);
    // Initial com status is SUCCESS (ready for the first frame)
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_TLM_RadioState(this->tlmHistory_RadioState->size() - 1, Rfm9XRadioState::READY);
}

void Rfm9XManagerTester ::test_detection_retry() {
    this->setDefaultParameters();
    // SPI failure during bring-up: radio not detected
    this->m_spiFail = true;
    this->component.loadParameters();
    this->runTicks(16);
    ASSERT_GT(this->eventHistory_ConfigurationFailed->size(), 0u);
    ASSERT_EVENTS_ConfigurationFailed(0, Rfm9XMode::Receive);
    ASSERT_from_comStatusOut_SIZE(0);
    // Radio (SPI) recovers: detection retries on run and succeeds
    this->m_spiFail = false;
    this->clearHistory();
    this->runUntilReady();
    ASSERT_from_comStatusOut_SIZE(1);
}

void Rfm9XManagerTester ::test_detection_radio_absent() {
    this->setDefaultParameters();
    // An unpowered/absent radio answers every SPI read with zeros
    this->m_model.setRadioPresent(false);
    this->component.loadParameters();
    this->runTicks(16);
    ASSERT_GT(this->eventHistory_ConfigurationFailed->size(), 0u);
    ASSERT_from_comStatusOut_SIZE(0);
    // Radio powered up: detection retries on later ticks and succeeds
    this->m_model.setRadioPresent(true);
    this->clearHistory();
    this->runUntilReady();
    ASSERT_from_comStatusOut_SIZE(1);
}

void Rfm9XManagerTester ::test_transmit_channel_busy() {
    this->makeReady();
    // A carrier on the channel asserts RegModemStat activity in RX:
    // listen-before-talk must defer the downlink with FAILURE.
    this->m_model.setChannelBusy(true);
    this->runUntilChannelBusyRefreshed();
    this->clearHistory();
    U8 data[16] = {0x5A};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);

    // Channel clears: the same frame now transmits
    this->m_model.setChannelBusy(false);
    this->runUntilChannelBusyRefreshed();
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_TLM_PacketsTransmitted(0, 1);
}

void Rfm9XManagerTester ::test_transmit_mode_settle() {
    // Model oscillator/PLL settling: packet-engine activity lags each mode change.
    this->m_model.setModeSettleByteTimes(24);
    this->makeReady();
    U8 data[32];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i ^ 0x3C);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_EVENTS_SendFailed_SIZE(0);
    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof data);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], data[i]);
    }
}

void Rfm9XManagerTester ::test_transmit() {
    this->makeReady();
    U8 data[32];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();

    // Transmission succeeded and the buffer was returned
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_TLM_PacketsTransmitted(0, 1);

    // The model "transmitted" exactly the payload
    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof data);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], data[i]);
    }
    // No second packet pending
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);
}

void Rfm9XManagerTester ::test_transmit_zero_size() {
    this->makeReady();
    U8 data[1] = {0};
    Fw::Buffer buffer(data, 0);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);

    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::INVALID_SIZE);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);
}

void Rfm9XManagerTester ::test_transmit_oversize() {
    this->makeReady();
    U8 data[MAX_PACKET_PAYLOAD + 1];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);

    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::INVALID_SIZE);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);
}

void Rfm9XManagerTester ::test_transmit_large() {
    this->makeReady();
    // A full 255-byte packet: fits the 256-byte LoRa FIFO in one standby fill.
    U8 data[MAX_PACKET_PAYLOAD];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>((i * 7) & 0xFF);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();

    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_TLM_PacketsTransmitted(0, 1);

    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof data);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], data[i]);
    }
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);
}

void Rfm9XManagerTester ::test_receive_large() {
    this->makeReady();
    // A full 255-byte packet is delivered in one FIFO burst read.
    U8 data[MAX_PACKET_PAYLOAD];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>((i * 3) & 0xFF);
    }
    this->m_model.injectAirData(data, sizeof data);
    for (U32 tick = 0; tick < 512; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_TLM_PacketsReceived(0, 1);
    const Fw::Buffer& received = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(received.getSize(), sizeof data);
    for (FwSizeType i = 0; i < received.getSize(); i++) {
        ASSERT_EQ(received.getData()[i], data[i]);
    }
}

void Rfm9XManagerTester ::test_default_register_image() {
    this->makeReady();
    // This is the canonical default enum image mirrored by the ground station:
    // SF7 / BW_125_KHZ / CR_4_5 / 434 MHz / DBM_13.
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::FRF_MSB), 0x6C);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::FRF_MID), 0x80);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::FRF_LSB), 0x00);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::PA_CONFIG), 0xCB);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::OCP), 0x2B);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::PA_DAC), 0x84);
    // Bw=0111 (125kHz) | CodingRate=001 (4/5) << 1 | ImplicitHeaderModeOn=0
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::MODEM_CONFIG_1), 0x72);
    // SpreadingFactor=7 << 4 | TxContinuousMode=0 | RxPayloadCrcOn=1 | SymbTimeout(9:8)=0
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::MODEM_CONFIG_2), 0x74);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::PREAMBLE_MSB), 0x00);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::PREAMBLE_LSB), 0x08);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::MAX_PAYLOAD_LENGTH), 0xFF);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::FIFO_TX_BASE_ADDR), Rfm9XManager::FIFO_BASE_ADDR);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::FIFO_RX_BASE_ADDR), Rfm9XManager::FIFO_BASE_ADDR);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::OP_MODE) & Mode::LONG_RANGE_MODE, Mode::LONG_RANGE_MODE);
}

void Rfm9XManagerTester ::test_long_range_mode_sleep_gated() {
    // datasheet section 6.4, RegOpMode bit 7: "This bit can be modified only
    // in Sleep mode." Bring-up must therefore pass through SLEEP with
    // LongRangeMode set before reaching STANDBY/configuration.
    this->m_model.clearRegisterWriteHistory();
    this->makeReady();
    ASSERT_TRUE(this->m_model.wasRegisterWritten(Reg::OP_MODE, Mode::SLEEP));
    ASSERT_TRUE(this->m_model.wasRegisterWritten(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::SLEEP)));
}

void Rfm9XManagerTester ::test_fifo_pointer_management() {
    this->makeReady();
    this->m_model.clearRegisterWriteHistory();
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    // TX stages FifoAddrPtr at the TX base address before writing the payload
    // (datasheet section 4.1.5: "Set FifoPtrAddr to FifoTxPtrBase").
    ASSERT_TRUE(this->m_model.wasRegisterWritten(Reg::FIFO_ADDR_PTR, Rfm9XManager::FIFO_BASE_ADDR));

    this->clearHistory();
    this->m_model.clearRegisterWriteHistory();
    U8 uplink[8] = {7, 6, 5, 4, 3, 2, 1, 0};
    this->m_model.injectAirData(uplink, sizeof uplink);
    for (U32 tick = 0; tick < 512; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
    // RX repoints FifoAddrPtr to FifoRxCurrentAddr before reading the packet
    // out (datasheet section 4.1.5, "Payload Data Extraction from FIFO").
    ASSERT_TRUE(this->m_model.wasRegisterWritten(Reg::FIFO_ADDR_PTR, Rfm9XManager::FIFO_BASE_ADDR));
}

void Rfm9XManagerTester ::test_rssi_snr_telemetry() {
    this->makeReady();
    this->m_model.setRssiReadback(0x61);  // -137 + 97 = -40 dBm
    this->m_model.setSnrReadback(0xF4);   // (I8)-12 / 4 = -3 dB
    U8 data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    this->m_model.injectAirData(data, sizeof data);
    for (U32 tick = 0; tick < 512; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_TLM_LastPacketRssi(0, -40.0f);
    ASSERT_TLM_LastPacketSnr(0, -3.0f);
}

void Rfm9XManagerTester ::test_dio0_mapping() {
    this->makeReady();
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::DIO_MAPPING_1), Dio0Mapping::RX_DONE);

    this->m_model.clearRegisterWriteHistory();
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_TRUE(this->m_model.wasRegisterWritten(Reg::DIO_MAPPING_1, Dio0Mapping::TX_DONE));
    // Back to RxDone mapping once TX completes and RX resumes.
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::DIO_MAPPING_1), Dio0Mapping::RX_DONE);
}

void Rfm9XManagerTester ::test_spreading_factor_register_map() {
    const struct {
        SpreadingFactor sf;
        U8 registerValue;
    } expected[] = {
        {SpreadingFactor::SF6, 6},
        {SpreadingFactor::SF7, 7},
        {SpreadingFactor::SF8, 8},
        {SpreadingFactor::SF9, 9},
        {SpreadingFactor::SF10, 10},
        {SpreadingFactor::SF11, 11},
        {SpreadingFactor::SF12, 12},
    };
    for (const auto& item : expected) {
        U8 reg = 0;
        ASSERT_TRUE(getSpreadingFactorReg(item.sf, reg));
        ASSERT_EQ(reg, item.registerValue);
    }
}

void Rfm9XManagerTester ::test_bandwidth_update_reconfigure() {
    this->makeReady();

    // Parameter-set commands call parameterUpdated(), which schedules
    // CONFIGURE for the next rate-group tick (no SPI in command context).
    this->paramSet_BANDWIDTH(Bandwidth::BW_250_KHZ, Fw::ParamValid::VALID);
    this->paramSend_BANDWIDTH(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_BANDWIDTH_SET, 0, Fw::CmdResponse::OK);
    ASSERT_from_spiWriteRead_SIZE(0);

    // CONFIGURE now takes many ticks (one SPI op per callback); re-run it
    // to completion rather than assuming one tick finishes the whole table.
    this->runUntilReady();
    ASSERT_EVENTS_ConfigurationFailed_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(0);
    // Bw=1000 (250kHz) | CodingRate=001 (4/5) << 1
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::MODEM_CONFIG_1), 0x82);

    this->clearHistory();
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), sizeof data);
}

void Rfm9XManagerTester ::test_bandwidth_register_map() {
    const struct {
        Bandwidth bandwidth;
        U8 registerValue;
    } expected[] = {
        {Bandwidth::BW_7_8_KHZ, 0x00},
        {Bandwidth::BW_10_4_KHZ, 0x10},
        {Bandwidth::BW_15_6_KHZ, 0x20},
        {Bandwidth::BW_20_8_KHZ, 0x30},
        {Bandwidth::BW_31_25_KHZ, 0x40},
        {Bandwidth::BW_41_7_KHZ, 0x50},
        {Bandwidth::BW_62_5_KHZ, 0x60},
        {Bandwidth::BW_125_KHZ, 0x70},
        {Bandwidth::BW_250_KHZ, 0x80},
        {Bandwidth::BW_500_KHZ, 0x90},
    };

    for (const auto& entry : expected) {
        U8 reg = 0;
        ASSERT_TRUE(getBandwidthReg(entry.bandwidth, reg));
        ASSERT_EQ(reg, entry.registerValue);
    }
}

void Rfm9XManagerTester ::test_coding_rate_register_map() {
    const struct {
        CodingRate cr;
        U8 registerValue;
    } expected[] = {
        {CodingRate::CR_4_5, 0x02},
        {CodingRate::CR_4_6, 0x04},
        {CodingRate::CR_4_7, 0x06},
        {CodingRate::CR_4_8, 0x08},
    };
    for (const auto& item : expected) {
        U8 reg = 0;
        ASSERT_TRUE(getCodingRateReg(item.cr, reg));
        ASSERT_EQ(reg, item.registerValue);
    }
}

void Rfm9XManagerTester ::test_parameter_update_reconfigure() {
    this->makeReady();

    // parameterUpdated() defers reconfiguration to the next rate-group tick
    // rather than touching SPI in command-dispatch context.
    this->paramSet_SPREADING_FACTOR(SpreadingFactor::SF9, Fw::ParamValid::VALID);
    this->paramSend_SPREADING_FACTOR(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SPREADING_FACTOR_SET, 0, Fw::CmdResponse::OK);
    ASSERT_from_spiWriteRead_SIZE(0);

    // CONFIGURE now takes many ticks (one SPI op per callback); re-run it
    // to completion rather than assuming one tick finishes the whole table.
    this->runUntilReady();
    ASSERT_EVENTS_ConfigurationFailed_SIZE(0);
    // SpreadingFactor=9 << 4 | RxPayloadCrcOn=1
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::MODEM_CONFIG_2), 0x94);
    ASSERT_from_comStatusOut_SIZE(0);

    this->clearHistory();
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), sizeof data);
}

void Rfm9XManagerTester ::test_reset_recovery() {
    this->makeReady();

    this->sendCmd_RESET(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_RESET, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_ResetInitiated_SIZE(1);
    // The command itself performs no GPIO work; the pulse is tick-driven.
    ASSERT_from_resetGpio_SIZE(0);

    // Subsequent scheduler ticks pulse RST, redetect, and configure the model.
    // The startup Com SUCCESS is intentionally not repeated after a reset.
    this->runUntilReady();
    ASSERT_EVENTS_ConfigurationFailed_SIZE(0);
    ASSERT_from_resetGpio_SIZE(2);
    ASSERT_from_resetGpio(0, Fw::Logic::LOW);
    ASSERT_from_resetGpio(1, Fw::Logic::HIGH);
    ASSERT_from_comStatusOut_SIZE(0);

    this->clearHistory();
    U8 data[8] = {7, 6, 5, 4, 3, 2, 1, 0};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), sizeof data);
}

void Rfm9XManagerTester ::test_transmit_dropped_when_busy() {
    this->makeReady();
    // A reception in progress: RegModemStat reports RX ongoing
    U8 uplink[40];
    for (FwSizeType i = 0; i < sizeof uplink; i++) {
        uplink[i] = static_cast<U8>(i);
    }
    this->m_model.injectAirData(uplink, sizeof uplink);
    this->runUntilChannelBusyRefreshed();
    this->clearHistory();

    // Half-duplex: defer downlink with FAILURE so ComQueue can pause
    U8 data[32] = {0xA5};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_EVENTS_SendFailed_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);

    this->clearHistory();
    for (U32 tick = 0; tick < 1024; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(0);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);
}

void Rfm9XManagerTester ::test_downlink_starvation_recovery() {
    this->makeReady();

    // Periodic uplink shorter than the RX lease keeps re-arming it forever.
    U8 uplink[16];
    for (FwSizeType i = 0; i < sizeof uplink; i++) {
        uplink[i] = static_cast<U8>(i);
    }
    this->m_model.injectAirData(uplink, sizeof uplink);
    for (U32 tick = 0; (tick < 1024) && (this->fromPortHistory_dataOut->size() == 0); tick++) {
        this->invoke_to_run(0, 0);
    }
    ASSERT_from_dataOut_SIZE(1);

    // A downlink attempt during the lease is deferred with FAILURE.
    U8 data[32] = {0x5A};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);

    // Re-arm the lease more often than it can decay. The starvation cap must
    // still open a downlink window and resume ComQueue with SUCCESS.
    this->clearHistory();
    bool resumed = false;
    const U32 keepaliveTicks = this->component.getRxTxHoldoffBoundTicks() - 100;
    const U32 bound = 3 * this->component.getRxHoldoffStarvationTicks();
    for (U32 tick = 0; tick < bound; tick++) {
        if ((tick % keepaliveTicks) == (keepaliveTicks - 1)) {
            this->m_model.injectAirData(uplink, sizeof uplink);
        }
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_comStatusOut->size() > 0) {
            resumed = true;
            break;
        }
    }
    ASSERT_TRUE(resumed);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
}

void Rfm9XManagerTester ::test_transmit_not_ready() {
    // No initialization: transmission must be refused with FAILURE status.
    // The refusal is deliberate flow control, not a fault: FAILURE pauses
    // ComQueue, and bring-up problems are already reported through
    // ConfigurationFailed, so no SendFailed event is emitted here.
    U8 data[8] = {0};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_EVENTS_SendFailed_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_spiWriteRead_SIZE(0);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);
}

void Rfm9XManagerTester ::test_transmit_spi_failure() {
    this->makeReady();
    this->m_spiFail = true;
    U8 data[8] = {0};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::RADIO_FAULT);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
}

void Rfm9XManagerTester ::test_transmit_timeout_recovery() {
    this->makeReady();
    this->m_model.setTxDoneStall(true);
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes(this->component.getTxDoneTimeoutTicks() + 16);

    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::RADIO_FAULT);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->m_model.readRegisterValue(Reg::OP_MODE) & Mode::MASK, Mode::RX_CONTINUOUS);
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), 0);

    // Remove the fault and prove the same radio model can transmit again.
    this->m_model.setTxDoneStall(false);
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_from_dataReturnOut_SIZE(1);
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof data);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], data[i]);
    }
}

void Rfm9XManagerTester ::test_receive() {
    this->makeReady();
    U8 data[64];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(0xFF - (i & 0xFF));
    }
    this->m_model.injectAirData(data, sizeof data);
    for (U32 tick = 0; tick < 1024; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() >= 1) {
            break;
        }
    }

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_TLM_PacketsReceived(0, 1);
    const Fw::Buffer& received = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(received.getSize(), sizeof data);
    for (FwSizeType i = 0; i < received.getSize(); i++) {
        ASSERT_EQ(received.getData()[i], data[i]);
    }
}

void Rfm9XManagerTester ::test_receive_allocation_failure() {
    this->makeReady();
    U8 data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    this->m_model.injectAirData(data, sizeof data);
    this->m_allocFail = true;
    for (U32 tick = 0; tick < 512; tick++) {
        this->invoke_to_run(0, 0);
    }
    // Packet dropped with an event; nothing delivered downstream
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    ASSERT_EVENTS_AllocationFailed(0, sizeof data);
    ASSERT_from_dataOut_SIZE(0);
    // Link stays alive: a later packet is delivered once allocation recovers
    this->m_allocFail = false;
    this->clearHistory();
    // Acknowledge RxDone so the model can accept the next reception.
    this->invoke_to_run(0, 0);
    this->m_model.injectAirData(data, sizeof data);
    for (U32 tick = 0; tick < 512; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
}

void Rfm9XManagerTester ::test_receive_crc_drop() {
    this->makeReady();
    // RxDone asserts alongside PayloadCrcError on a corrupted frame
    // (datasheet section 4.1.5): the manager must drop it, not forward garbage.
    this->m_model.injectCorruptFrame(40);
    for (U32 tick = 0; tick < 1024; tick++) {
        this->invoke_to_run(0, 0);
    }
    // Nothing delivered to the deframer; the drop is counted in telemetry.
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_TLM_RxCrcErrors_SIZE(1);
    ASSERT_TLM_RxCrcErrors(0, 1);

    // The link still works afterward: a clean frame is delivered normally.
    this->clearHistory();
    U8 good[32];
    for (FwSizeType i = 0; i < sizeof good; i++) {
        good[i] = static_cast<U8>(i + 1);
    }
    this->m_model.injectAirData(good, sizeof good);
    for (U32 tick = 0; tick < 1024; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getSize(), sizeof good);
}

void Rfm9XManagerTester ::test_data_return() {
    U8 data[8] = {0};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataReturnIn(0, buffer, context);
    ASSERT_from_deallocate_SIZE(1);
}

void Rfm9XManagerTester ::test_transmit_disabled() {
    this->makeReady();
    this->sendCmd_TRANSMIT(0, 0, Rfm9X::TransmitState::DISABLED);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_TRANSMIT, 0, Fw::CmdResponse::OK);
    this->clearHistory();

    // Mute: no SPI TX; FAILURE pauses ComQueue; buffer returned.
    U8 data[32];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_from_spiWriteRead_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);

    // Re-enable emits SUCCESS to resume Com, then the next frame transmits
    this->clearHistory();
    this->sendCmd_TRANSMIT(0, 1, Rfm9X::TransmitState::ENABLED);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    ASSERT_TLM_PacketsTransmitted(0, 1);
}

void Rfm9XManagerTester ::test_transmit_enable_idempotent() {
    this->makeReady();
    // No frame was deferred while disabled: re-enabling must not emit an
    // unsolicited SUCCESS (ComQueue asserts on a resume while READY).
    this->sendCmd_TRANSMIT(0, 0, Rfm9X::TransmitState::DISABLED);
    this->sendCmd_TRANSMIT(0, 1, Rfm9X::TransmitState::ENABLED);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_from_comStatusOut_SIZE(0);

    // Enabling while already enabled is also silent.
    this->sendCmd_TRANSMIT(0, 2, Rfm9X::TransmitState::ENABLED);
    ASSERT_from_comStatusOut_SIZE(0);
}

void Rfm9XManagerTester ::test_reset_during_transmit() {
    this->makeReady();
    // Stall TxDone so the staged TX stays active across ticks.
    this->m_model.setTxDoneStall(true);
    U8 data[32];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runTicks(8);
    ASSERT_from_dataReturnOut_SIZE(0);

    // RESET while TX is active: the buffer is returned with FAILURE and no
    // further TX SPI traffic follows once the reset sequence begins.
    this->sendCmd_RESET(0, 0);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_RESET, 0, Fw::CmdResponse::OK);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::FAILURE));
    ASSERT_TLM_PacketsTransmitted_SIZE(0);

    // The radio recovers to READY and can transmit again.
    this->m_model.setTxDoneStall(false);
    this->clearHistory();
    this->runUntilReady();
    ASSERT_EVENTS_RadioReady_SIZE(1);
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
    U8 transmitted[MAX_PACKET_PAYLOAD];
    ASSERT_EQ(this->m_model.retrievePacket(transmitted, sizeof transmitted), sizeof data);
}

void Rfm9XManagerTester ::test_local_hold_ordering() {
    // Constructed with comStatusOut unconnected: local pending-TX hold mode.
    ASSERT_FALSE(this->component.isConnected_comStatusOut_OutputPort(0));
    this->setDefaultParameters();
    this->component.loadParameters();
    this->runUntilReady();
    ASSERT_EVENTS_RadioReady_SIZE(1);
    this->clearHistory();

    // While muted, frames are held locally (not dropped, no status).
    this->sendCmd_TRANSMIT(0, 0, Rfm9X::TransmitState::DISABLED);
    U8 first[16];
    U8 second[16];
    for (FwSizeType i = 0; i < sizeof first; i++) {
        first[i] = static_cast<U8>(0x10 + i);
        second[i] = static_cast<U8>(0x80 + i);
    }
    Fw::Buffer firstBuffer(first, sizeof first);
    Fw::Buffer secondBuffer(second, sizeof second);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, firstBuffer, context);
    this->invoke_to_dataIn(0, secondBuffer, context);
    ASSERT_from_dataReturnOut_SIZE(0);

    // Re-enable: held frames drain oldest-first, paced by the TX holdoff.
    this->sendCmd_TRANSMIT(0, 1, Rfm9X::TransmitState::ENABLED);
    for (U32 tick = 0; tick < (4 * this->component.getTxTxHoldoffBoundTicks()); tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataReturnOut->size() >= 2) {
            break;
        }
    }
    ASSERT_from_dataReturnOut_SIZE(2);
    ASSERT_TLM_PacketsTransmitted(1, 2);

    U8 transmitted[MAX_PACKET_PAYLOAD];
    FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof first);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], first[i]);
    }
    size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, sizeof second);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(transmitted[i], second[i]);
    }
}

void Rfm9XManagerTester ::test_send_string_command() {
    this->makeReady();
    // Fw::CmdStringArg is capped at FW_CMD_STRING_MAX_SIZE (16 bytes on this
    // STM32H7 target, including the null terminator): keep this under 15 chars.
    const Fw::CmdStringArg payload("HELLO LoRa");
    this->sendCmd_SEND_STRING(0, 0, payload);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SEND_STRING, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TestPayloadQueued_SIZE(1);
    ASSERT_EVENTS_TestPayloadQueued(0, payload.length());
    // A command-originated send never touches comStatusOut; watch the
    // PacketsTransmitted counter (origin-agnostic) for completion instead.
    for (U32 tick = 0; (tick < 4096) && (this->tlmHistory_PacketsTransmitted->size() == 0); tick++) {
        this->invoke_to_run(0, 0);
    }
    ASSERT_TLM_PacketsTransmitted_SIZE(1);
    ASSERT_TLM_PacketsTransmitted(0, 1);

    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, payload.length());
    EXPECT_EQ(::memcmp(transmitted, payload.toChar(), size), 0);
}

void Rfm9XManagerTester ::test_send_string_zero_size() {
    this->makeReady();
    const Fw::CmdStringArg payload("");
    this->sendCmd_SEND_STRING(0, 0, payload);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SEND_STRING, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::INVALID_SIZE);
    ASSERT_from_spiWriteRead_SIZE(0);
}

void Rfm9XManagerTester ::test_send_string_not_ready() {
    // No initialization: the radio is not READY yet.
    const Fw::CmdStringArg payload("test");
    this->sendCmd_SEND_STRING(0, 0, payload);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SEND_STRING, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::NOT_READY);
    ASSERT_from_spiWriteRead_SIZE(0);
}

void Rfm9XManagerTester ::test_send_string_busy() {
    this->makeReady();
    // Stall TxDone so the first dataIn-originated transmit stays active.
    this->m_model.setTxDoneStall(true);
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runTicks(4);
    ASSERT_from_dataReturnOut_SIZE(0);

    this->clearHistory();
    const Fw::CmdStringArg payload("busy");
    this->sendCmd_SEND_STRING(0, 0, payload);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SEND_STRING, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Rfm9XSendFailure::NOT_READY);
}

void Rfm9XManagerTester ::test_send_string_no_com_side_effects() {
    this->makeReady();
    const Fw::CmdStringArg payload("no com effect");
    this->sendCmd_SEND_STRING(0, 0, payload);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_SEND_STRING, 0, Fw::CmdResponse::OK);
    // A command-originated send never touches comStatusOut; watch the
    // PacketsTransmitted counter (origin-agnostic) for completion instead.
    for (U32 tick = 0; (tick < 4096) && (this->tlmHistory_PacketsTransmitted->size() == 0); tick++) {
        this->invoke_to_run(0, 0);
    }
    ASSERT_TLM_PacketsTransmitted_SIZE(1);
    // A command-originated send never touches Com-side output ports.
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(0);
    U8 transmitted[MAX_PACKET_PAYLOAD + 1];
    const FwSizeType size = this->m_model.retrievePacket(transmitted, sizeof transmitted);
    ASSERT_EQ(size, payload.length());

    // A subsequent dataIn-originated send still produces the normal pair,
    // once the RF-level post-TX holdoff (applies regardless of origin) decays.
    this->clearHistory();
    this->runTicks(this->component.getTxTxHoldoffBoundTicks());
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));
}

void Rfm9XManagerTester ::test_single_spi_request_in_flight() {
    // from_spiWriteRead_handler() asserts if a second request arrives while
    // one is still undelivered (FW_ASSERT(!this->m_pendingCompletionValid)):
    // driving a full bring-up + TX + RX cycle through the normal tick/drain
    // pairing in invoke_to_run() is itself the test -- any violation of the
    // "exactly one SPI op in flight at a time" invariant aborts immediately
    // rather than silently passing.
    this->makeReady();
    U8 data[32];
    for (FwSizeType i = 0; i < sizeof data; i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    this->runUntilTransmitCompletes();
    ASSERT_from_comStatusOut(0, Fw::Success(Fw::Success::SUCCESS));

    U8 uplink[16];
    for (FwSizeType i = 0; i < sizeof uplink; i++) {
        uplink[i] = static_cast<U8>(0xA0 + i);
    }
    this->m_model.injectAirData(uplink, sizeof uplink);
    for (U32 tick = 0; tick < 1024; tick++) {
        this->invoke_to_run(0, 0);
        if (this->fromPortHistory_dataOut->size() > 0) {
            break;
        }
    }
    ASSERT_from_dataOut_SIZE(1);
}

void Rfm9XManagerTester ::test_stale_completion_after_reset_dropped() {
    this->makeReady();
    U8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(data, sizeof data);
    ComCfg::FrameContext context;
    this->invoke_to_dataIn(0, buffer, context);
    // One tick: TX_REQUEST_STANDBY issues its write and stashes the
    // completion via from_spiWriteRead_handler(), but invoke_to_run()'s
    // drain has not run yet inside this single call -- it runs at the end
    // of this same invoke_to_run(), so grab the still-unconsumed stash by
    // calling the component's run tick directly instead of through the
    // draining override, leaving the completion pending.
    this->Rfm9XManagerTesterBase::invoke_to_run(0, 0);
    ASSERT_TRUE(this->m_pendingCompletionValid);

    // RESET discards the in-flight sequence (clears m_spiOpOrigin) before
    // the stashed completion is ever delivered.
    this->sendCmd_RESET(0, 0);
    ASSERT_CMD_RESPONSE(0, Rfm9XManagerComponentBase::OPCODE_RESET, 0, Fw::CmdResponse::OK);
    this->clearHistory();

    // Deliver the stale completion now: it must be dropped silently (the
    // origin was SPI_OP_TX when issued, but m_spiOpOrigin is SPI_OP_NONE by
    // the time this arrives) rather than resuming TX state RESET discarded.
    this->drainOnePendingCompletion();
    ASSERT_FALSE(this->m_pendingCompletionValid);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(0);
    ASSERT_TLM_PacketsTransmitted_SIZE(0);

    // The radio still recovers normally afterward.
    this->runUntilReady();
    ASSERT_EVENTS_RadioReady_SIZE(1);
}

}  // namespace Rfm9X