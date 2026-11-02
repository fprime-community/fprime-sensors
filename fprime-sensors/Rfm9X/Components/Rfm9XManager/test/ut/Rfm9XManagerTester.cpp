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
      m_allocation{} {
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
}

Rfm9XManagerTester ::~Rfm9XManagerTester() {}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

Drv::SpiStatus Rfm9XManagerTester ::from_spiWriteRead_handler(FwIndexType portNum,
                                                              Fw::Buffer& writeBuffer,
                                                              Fw::Buffer& readBuffer) {
    this->pushFromPortEntry_spiWriteRead(writeBuffer, readBuffer);
    if (this->m_spiFail) {
        return Drv::SpiStatus::SPI_WRITE_ERR;
    }
    EXPECT_EQ(writeBuffer.getSize(), readBuffer.getSize());
    this->m_model.spiTransaction(writeBuffer.getData(), readBuffer.getData(), writeBuffer.getSize());
    return Drv::SpiStatus::SPI_OK;
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
    // Optional RST GPIO is connected in the harness: expect one HIGH then LOW pulse.
    ASSERT_from_resetGpio_SIZE(2);
    ASSERT_from_resetGpio(0, Fw::Logic::HIGH);
    ASSERT_from_resetGpio(1, Fw::Logic::LOW);
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

    this->invoke_to_run(0, 0);
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

    this->invoke_to_run(0, 0);
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
    ASSERT_from_resetGpio(0, Fw::Logic::HIGH);
    ASSERT_from_resetGpio(1, Fw::Logic::LOW);
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
    const U32 keepaliveTicks = Rfm9XManager::RX_TX_HOLDOFF_TICKS - 100;
    const U32 bound = 3 * Rfm9XManager::RX_HOLDOFF_STARVATION_TICKS;
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
    this->runUntilTransmitCompletes(Rfm9XManager::TX_DONE_TIMEOUT_TICKS + 16);

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
    for (U32 tick = 0; tick < (4 * Rfm9XManager::TX_TX_HOLDOFF_TICKS); tick++) {
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

}  // namespace Rfm9X