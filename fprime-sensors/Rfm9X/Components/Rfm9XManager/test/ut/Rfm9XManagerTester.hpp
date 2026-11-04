// ======================================================================
// \title  Rfm9XManagerTester.hpp
// \brief  hpp file for Rfm9XManager component test harness implementation class
// ======================================================================

#ifndef Rfm9X_Rfm9XManagerTester_HPP
#define Rfm9X_Rfm9XManagerTester_HPP

#include "Rfm9XManagerGTestBase.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManager.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.hpp"

namespace Rfm9X {

class Rfm9XManagerTester : public Rfm9XManagerGTestBase {
  public:
    // Maximum size of histories storing events, telemetry, and port outputs.
    static const U32 MAX_HISTORY_SIZE = 50000;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Size of the buffer pool backing the allocate port
    static const FwSizeType ALLOCATION_SIZE = 1024;

    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object Rfm9XManagerTester
    //! \param localHold when true, comStatusOut is left unconnected so the
    //!        component selects the ground-station local pending-TX hold mode
    explicit Rfm9XManagerTester(bool localHold = false);

    //! Destroy object Rfm9XManagerTester
    ~Rfm9XManagerTester();

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Detection, configuration, and initial com status (REQ-001/002/003)
    void test_initialization();

    //! Detection retry when the radio is absent (REQ-009)
    void test_detection_retry();

    //! Single-packet transmission (REQ-004/005/007/010)
    void test_transmit();

    //! Zero-length frames are rejected: an RF packet must contain 1..255 bytes
    void test_transmit_zero_size();

    //! Frames above the native 255-byte RF payload limit are rejected
    void test_transmit_oversize();

    //! Maximum-size (255-byte) packet, filled in one standby FIFO burst (REQ-004)
    void test_transmit_large();

    //! Maximum-size (255-byte) packet received in one FIFO burst (REQ-006)
    void test_receive_large();

    //! Default enum values program the canonical flight/ground register image
    void test_default_register_image();

    //! LongRangeMode can only be set while RegOpMode.Mode == SLEEP
    void test_long_range_mode_sleep_gated();

    //! TX stages FifoAddrPtr at FifoTxBaseAddr; RX reads back from FifoRxCurrentAddr
    void test_fifo_pointer_management();

    //! RegPktRssiValue/RegPktSnrValue readbacks map to the documented dBm/dB formulas
    void test_rssi_snr_telemetry();

    //! DIO0 is mapped to TxDone while transmitting and back to RxDone afterward
    void test_dio0_mapping();

    //! Every exposed spreading-factor enum maps to the intended register value
    void test_spreading_factor_register_map();

    //! A live BANDWIDTH parameter update schedules and completes reconfiguration
    void test_bandwidth_update_reconfigure();

    //! Every exposed bandwidth enum (10-value range) maps to the intended register value
    void test_bandwidth_register_map();

    //! Every exposed coding-rate enum maps to the intended register value
    void test_coding_rate_register_map();

    //! A live SPREADING_FACTOR parameter update schedules and completes reconfiguration
    void test_parameter_update_reconfigure();

    //! RESET pulses RST and returns the radio to READY without a process restart
    void test_reset_recovery();

    //! Half-duplex: TX while RX is in progress is dropped immediately
    void test_transmit_dropped_when_busy();

    //! Downlink deferred by continual uplink resumes via the starvation cap
    void test_downlink_starvation_recovery();

    //! Transmission before the radio is ready (REQ-012)
    void test_transmit_not_ready();

    //! Transmission with SPI failures (REQ-008)
    void test_transmit_spi_failure();

    //! A missing TxDone response times out and returns the radio to RX
    void test_transmit_timeout_recovery();

    //! Packet reception and delivery (REQ-006/007/010)
    void test_receive();

    //! Reception with buffer allocation failure (REQ-011)
    void test_receive_allocation_failure();

    //! A CRC-failing frame is dropped, counted, and never forwarded downstream
    void test_receive_crc_drop();

    //! Uplink buffer return path deallocates buffers
    void test_data_return();

    //! TRANSMIT command gives a commandable receive-only window
    void test_transmit_disabled();

    //! Re-enabling TRANSMIT without a prior deferral emits no unsolicited SUCCESS
    void test_transmit_enable_idempotent();

    //! RESET during an active TX returns the buffer and recovers to READY
    void test_reset_during_transmit();

    //! Local hold mode (comStatusOut unconnected): frames queue in FIFO order
    void test_local_hold_ordering();

    //! An absent (unpowered) radio is not detected; recovery follows power-up
    void test_detection_radio_absent();

    //! A busy RF channel (RegModemStat) defers downlink until clear
    void test_transmit_channel_busy();

    //! Oscillator/PLL settling delays are tolerated
    void test_transmit_mode_settle();

    //! SEND_STRING stages a command payload through the same TX engine
    //! dataIn uses, with no dataReturnOut/comStatusOut side effects
    void test_send_string_command();

    //! SEND_STRING rejects a zero-length payload. (Oversize cannot be
    //! exercised through this command: Fw::CmdStringArg's fixed capacity
    //! is well under MAX_PACKET_PAYLOAD, so FPP's own deserialization
    //! bounds every command-supplied string before the handler runs.)
    void test_send_string_zero_size();

    //! SEND_STRING rejects when the radio is not READY yet
    void test_send_string_not_ready();

    //! SEND_STRING rejects while a transmit is already active
    void test_send_string_busy();

    //! A command-originated send has no Com side effects, and a later
    //! dataIn-originated send still produces the normal Com pair
    void test_send_string_no_com_side_effects();

    private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Handler for from_spiWriteRead: backed by the register-level sim model
    Drv::SpiStatus from_spiWriteRead_handler(FwIndexType portNum,
                                             Fw::Buffer& writeBuffer,
                                             Fw::Buffer& readBuffer) override;

    //! Handler for from_allocate
    Fw::Buffer from_allocate_handler(FwIndexType portNum, FwSizeType size) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Bring the component to the READY state
    void makeReady();

    //! Invoke the scheduler tick a fixed number of times
    void runTicks(U32 count);

    //! Tick until the radio bring-up sequence reports readiness
    void runUntilReady(U32 maxTicks = 64);

    //! Advance run ticks until a staged TX reports SUCCESS or FAILURE.
    void runUntilTransmitCompletes(U32 maxTicks = 4096);

    //! Seed the generated parameter source with the canonical default profile
    void setDefaultParameters();

    //! Seed the four FPP parameters before component.loadParameters()
    void setParameters(const SpreadingFactor& sf,
                       const Bandwidth& bw,
                       const CodingRate& cr,
                       const Rfm9XTxPower& txPower);

    //! Connect ports and initialize components (autocoded helpers)
    void connectPorts();
    void initComponents();

    //! connectPorts() minus comStatusOut: selects local pending-TX hold mode
    void connectPortsLocalHold();

    // ----------------------------------------------------------------------
    // Variables
    // ----------------------------------------------------------------------

    //! The component under test
    Rfm9XManager component;

    //! Register-level radio model behind the SPI port
    Rfm9XSimModel m_model;

    //! Force SPI transactions to fail
    bool m_spiFail;

    //! Force allocations to fail (return an empty buffer)
    bool m_allocFail;

    //! Backing memory for the allocate port
    U8 m_allocation[ALLOCATION_SIZE];
};

}  // namespace Rfm9X

#endif
