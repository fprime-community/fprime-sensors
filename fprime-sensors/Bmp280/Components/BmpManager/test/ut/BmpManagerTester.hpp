// ======================================================================
// \title  BmpManagerTester.hpp
// \author Generated
// \brief  hpp file for BmpManager component test harness implementation class
// ======================================================================

#ifndef Bmp280_BmpManagerTester_HPP
#define Bmp280_BmpManagerTester_HPP

#include "fprime-sensors/Bmp280/Components/BmpManager/BmpManager.hpp"
#include "fprime-sensors/Bmp280/Components/BmpManager/BmpManagerGTestBase.hpp"

namespace Bmp280 {

class BmpManagerTester : public BmpManagerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object BmpManagerTester
    BmpManagerTester();

    //! Destroy object BmpManagerTester
    ~BmpManagerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test nominal operation
    void test_nominal();

    //! Test error cases
    void test_error();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Handler for from_busWriteRead
    void from_busWriteRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer);

    //! Handler for from_busWrite
    void from_busWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer);

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    BmpManager component;
};

}  // namespace Bmp280

#endif