// ======================================================================
// \title  ImuManagerTester.hpp
// \author mstarch
// \brief  hpp file for ImuManager component test harness implementation class
// ======================================================================

#ifndef MpuImu_ImuManagerTester_HPP
#define MpuImu_ImuManagerTester_HPP

#include "fprime-sensors/MpuImu/Components/ImuManager/ImuManager.hpp"
#include "fprime-sensors/MpuImu/Components/ImuManager/ImuManagerGTestBase.hpp"

namespace MpuImu {

class ImuManagerTester : public ImuManagerGTestBase, public ::testing::Test {
  public:
    enum State { RESET, WAIT_RESET, WAIT_RESET_FINISH, POWER_ON, CONFIGURE_ACCELEROMETER, CONFIGURE_GYROSCOPE, RUN };

    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    static const U32 DEVICE_ADDRESS = 0x68;

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Instance Queue Depth
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ImuManagerTester
    ImuManagerTester();

    //! Destroy object ImuManagerTester
    ~ImuManagerTester();

    //! Nominal boot sequence
    void nominal_boot_sequence();

    //! Nominal reconfiguration sequence
    void reconfigure_sequence();

    //! Nominal run sequence
    void nominal_run_sequence();

    //! Ticks and dispatches the rate group
    void tick();

    //! Verifies that the component is in the RESET state
    void verify_reset();

    //! Verifies that a register write passed the correct values
    void verify_register_write(U8 registerAddress, U8 registerValue, Fw::Buffer& writeBuffer);

    //! Sets the state and clears the history
    void verify_state_and_clear(State state);

    //! Pick a valid acceleration range
    void pick_acceleration_range();

    //! Pick a valid gyroscope range
    void pick_gyroscope_range();

    //! Fill read data buffer
    void fill_read_data(Fw::Buffer& readBuffer);

    //! Handler implementation for from_bus
    Drv::I2cStatus from_busWriteRead_handler(FwIndexType portNum,      //!< The port number
                                             U32 addr,                 //!< I2C slave device address
                                             Fw::Buffer& writeBuffer,  //!< Buffer to write data to the i2c device
                                             Fw::Buffer& readBuffer    //!< Buffer to read back data from the i2c device, must set
                                                                       //!< size when passing in read buffer
                                            ) final;
    
    //! Handler implementation for from_bus
    Drv::I2cStatus from_busWrite_handler(FwIndexType portNum,      //!< The port number
                                         U32 addr,                 //!< I2C slave device address
                                         Fw::Buffer& writeBuffer   //!< Buffer to write data to the i2c device
                                        ) final;

  private:
    //! Handler implementation for from_bus
    Drv::I2cStatus bus_handler_helper(U32 addr,                 //!< I2C slave device address
                                      Fw::Buffer& writeBuffer,  //!< Buffer to write data to the i2c device
                                      Fw::Buffer& readBuffer    //!< Buffer to read back data from the i2c device, must set
                                                                //!< size when passing in read buffer
                                     );

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  protected:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    ImuManager component;

    //! Current state of the component
    State state;

    //! Current acceleration range
    AccelerationRange accelerationRange = AccelerationRange::RANGE_2G;

    //! Current gyroscope range
    GyroscopeRange gyroscopeRange = GyroscopeRange::RANGE_250DEG;

    //! IMU data recalculated for telemetry tests
    ImuData imuData;

    //! Failure rate to use on the I2C bus
    U8 failureRate = 0;

    //! Counter for I2C failures
    U32 i2cFailure = 0;
};

}  // namespace MpuImu

#endif
