// ======================================================================
// \title  ImuManager.hpp
// \author mstarch
// \brief  hpp file for ImuManager component implementation class
// ======================================================================

#ifndef MpuImu_ImuManager_HPP
#define MpuImu_ImuManager_HPP

#include "fprime-sensors/MpuImu/Components/ImuManager/ImuManagerComponentAc.hpp"
#include "fprime-sensors/MpuImu/Components/ImuManager/ImuTypes.hpp"

namespace MpuImu {

class ImuManager final : public ImuManagerComponentBase {
    friend class ImuManagerTester;
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuManager object
    ImuManager(const char* const compName  //!< The component name
    );

    //! Destroy ImuManager object
    ~ImuManager();

    //! Configure the device address
    void configure(U8 device_address=DEVICE_DEFAULT_ADDRESS);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Emit parameter updated EVR
    //!
    void parameterUpdated(FwPrmIdType id  //!< The parameter ID
                          ) override;

    //! Handler implementation for run
    //!
    //! Scheduling port for reading from IMU and writing to telemetry
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command RESET
    //!
    //! Command to force a RESET
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    // ----------------------------------------------------------------------
    // Implementations for internal state machine actions
    // ----------------------------------------------------------------------

    //! Implementation for action doReset of state machine MpuImu_ImuStateMachine
    //!
    //! Perform reset commands
    void MpuImu_ImuStateMachine_action_doReset(SmId smId,                             //!< The state machine id
                                               MpuImu_ImuStateMachine::Signal signal  //!< The signal
                                               ) override;

    //! Implementation for action checkReset of state machine MpuImu_ImuStateMachine
    //!
    //! Perform checkReset commands
    void MpuImu_ImuStateMachine_action_checkReset(SmId smId,                             //!< The state machine id
                                                  MpuImu_ImuStateMachine::Signal signal  //!< The signal
                                                 ) override;

    //! Implementation for action doEnable of state machine MpuImu_ImuStateMachine
    //!
    //! Perform enable commands
    void MpuImu_ImuStateMachine_action_doEnable(SmId smId,                             //!< The state machine id
                                                MpuImu_ImuStateMachine::Signal signal  //!< The signal
                                                ) override;

    //! Implementation for action doConfigure of state machine MpuImu_ImuStateMachine
    //!
    //! Perform configure commands
    void MpuImu_ImuStateMachine_action_doConfigure(SmId smId,                             //!< The state machine id
                                                   MpuImu_ImuStateMachine::Signal signal  //!< The signal
                                                   ) override;

    //! Implementation for action doRead of state machine MpuImu_ImuStateMachine
    //!
    //! Read the IMU
    void MpuImu_ImuStateMachine_action_doRead(SmId smId,                             //!< The state machine id
                                              MpuImu_ImuStateMachine::Signal signal  //!< The signal
                                              ) override;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

        //! Converts raw IMU data to the telemetry structure
    static ImuData convert_raw_data(const RawImuData& raw,
                                    const AccelerationRange& accelerationRange,
                                    const GyroscopeRange& gyroscopeRange);

    //! Acceleration range to register value
    static U8 accelerometer_range_to_register(AccelerationRange range);

    //! Gyroscope range to register value
    static U8 gyroscope_range_to_register(GyroscopeRange range);

    //! Resets the IMU
    Drv::I2cStatus reset();

    //! Read the reset register value
    Drv::I2cStatus read_reset(U8& value);

    //! Enable on the IMU
    Drv::I2cStatus enable();

    //! Configure the IMU's accelerometer and gyroscope
    Drv::I2cStatus configure_device();

    //! Read IMU data
    Drv::I2cStatus read(ImuData& imuData);

    //! Write to the I2C bus and handle errors
    Drv::I2cStatus bus_write(Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer);

    //! Deserializes raw data from the bus
    RawImuData deserialize_raw_data(Fw::Buffer& buffer);

  private:
    U8 m_address;
};

}  // namespace MpuImu

#endif
