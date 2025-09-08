// ======================================================================
// \title  XBee/test/ut/Tester.hpp
// \author mstarch
// \brief  hpp file for XBee test harness implementation class
// ======================================================================

#ifndef TESTER_HPP
#define TESTER_HPP

#include "XBeeManagerGTestBase.hpp"
#include "fprime-sensors/XBee/Components/XBeeManager/XBeeManager.hpp"

namespace XBee {

class XBeeManagerTester : public XBeeManagerGTestBase {
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Queue depth supplied to the component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

  public:
    //! Construct object XBeeManagerTester
    //!
    XBeeManagerTester();

    //! Destroy object XBeeManagerTester
    //!
    ~XBeeManagerTester();

  public:
    //! Buffer to fill with data
    //!
    void fill_buffer(Fw::Buffer& buffer_to_fill);
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test initial READY setup
    //!
    void test_initial();

    //! Tests the basic input and output of the component
    //!
    void test_basic_io();

    //! Tests the basic failure case for the component
    //!
    void test_fail();

    //! Tests the basic failure retry component
    //!
    void test_retry();

  private:
    // ----------------------------------------------------------------------
    // Handlers for output ports (test harness)
    // ----------------------------------------------------------------------

    //! Handler for from_dataOut
    //!
    void from_dataOut_handler(const FwIndexType portNum, /*!< The port number*/
                              Fw::Buffer& recvBuffer,
                              const ComCfg::FrameContext& context) override;

    //! Handler for from_comStatusOut
    //!
    void from_comStatusOut_handler(const FwIndexType portNum,  //!< The port number
                                   Fw::Success& status         //!< Status of communication state
                                   ) override;

    //! Handler for from_drvSendOut
    //!
    Drv::ByteStreamStatus from_drvSendOut_handler(const FwIndexType portNum, /*!< The port number*/
                                                  Fw::Buffer& sendBuffer) override;

  private:
    // ----------------------------------------------------------------------
    // Helper methods (autocoded)
    // ----------------------------------------------------------------------

    //! Connect ports
    //!
    void connectPorts();

    //! Initialize components
    //!
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Variables
    // ----------------------------------------------------------------------

    //! The component under test
    //!
    XBeeManager component;
    Drv::ByteStreamStatus m_send_mode;  //! Send mode
    U32 m_retries;
};

}  // namespace XBee

#endif
