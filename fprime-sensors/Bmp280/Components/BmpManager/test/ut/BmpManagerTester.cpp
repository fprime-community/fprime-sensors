// ======================================================================
// \title  BmpManagerTester.cpp
// \author Generated
// \brief  cpp file for BmpManager component test harness implementation class
// ======================================================================

#include "BmpManagerTester.hpp"

namespace Bmp280 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

BmpManagerTester ::BmpManagerTester()
    : BmpManagerGTestBase("BmpManagerTester", BmpManagerTester::MAX_HISTORY_SIZE), component("BmpManager") {
    this->initComponents();
    this->connectPorts();
}

BmpManagerTester ::~BmpManagerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void BmpManagerTester ::test_nominal() {
    // Test nominal operation
    this->invoke_to_run(0, 0);

    // Verify telemetry was sent
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_Reading_SIZE(1);

    // Verify no events were emitted
    ASSERT_EVENTS_SIZE(0);
}

void BmpManagerTester ::test_error() {
    // Test error cases
    this->invoke_to_run(0, 0);

    // Verify error event was emitted
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_I2cError_SIZE(1);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void BmpManagerTester ::from_busWriteRead_handler(FwIndexType portNum,
                                                  U32 addr,
                                                  Fw::Buffer& writeBuffer,
                                                  Fw::Buffer& readBuffer) {
    this->pushFromPortEntry_busWriteRead(addr, writeBuffer, readBuffer);
}

void BmpManagerTester ::from_busWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer) {
    this->pushFromPortEntry_busWrite(addr, writeBuffer);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void BmpManagerTester ::connectPorts() {
    // Connect ports
    this->connect_to_run(0, this->component.get_run_InputPort(0));
    this->connect_to_timeCaller(0, this->component.get_timeCaller_OutputPort(0));
    this->connect_to_CmdDisp(0, this->component.get_CmdDisp_OutputPort(0));
    this->connect_to_CmdReg(0, this->component.get_CmdReg_OutputPort(0));
    this->connect_to_CmdStatus(0, this->component.get_CmdStatus_OutputPort(0));
    this->connect_to_Log(0, this->component.get_Log_OutputPort(0));
    this->connect_to_LogText(0, this->component.get_LogText_OutputPort(0));
    this->connect_to_tlmOut(0, this->component.get_tlmOut_OutputPort(0));
    this->connect_to_PrmGet(0, this->component.get_PrmGet_OutputPort(0));
    this->connect_to_PrmSet(0, this->component.get_PrmSet_OutputPort(0));

    this->component.set_busWriteRead_OutputPort(0, this->get_from_busWriteRead(0));
    this->component.set_busWrite_OutputPort(0, this->get_from_busWrite(0));
}

void BmpManagerTester ::initComponents() {
    this->init();
    this->component.init(BmpManagerTester::TEST_INSTANCE_QUEUE_DEPTH, BmpManagerTester::TEST_INSTANCE_ID);
}

}  // namespace Bmp280