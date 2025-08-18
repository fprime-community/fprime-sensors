// ======================================================================
// \title  ImuManager.cpp
// \author mstarch
// \brief  cpp file for ImuManager component implementation class
// ======================================================================

#include "fprime-sensors/MpuImu/Components/ImuManager/ImuManager.hpp"

namespace MpuImu {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuManager ::ImuManager(const char* const compName) : ImuManagerComponentBase(compName), m_state(RESET) {}

ImuManager ::~ImuManager() {}

void ImuManager ::parameterUpdated(FwPrmIdType id) {
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    switch (id) {
        case PARAMID_ACCELEROMETER_RANGE: {
            // Read back the parameter value
            const AccelerationRange range = this->paramGet_ACCELEROMETER_RANGE(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_AccelerometerRangeUpdated(range);
            this->m_state = CONFIGURE;
            break;
        }
        case PARAMID_GYROSCOPE_RANGE: {
            // Read back the parameter value
            const GyroscopeRange range = this->paramGet_GYROSCOPE_RANGE(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_GyroscopeRangeUpdated(range);
            this->m_state = CONFIGURE;
            break;
        }
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(id));
            break;
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuManager ::run_handler(FwIndexType portNum, U32 context) {
    switch (this->m_state) {
        case RESET:
            // If reset is successful, move to SETUP state
            if (this->reset()) {
                this->m_state = POWER_ON;
            }
            break;
        case POWER_ON: {
            U8 reset_val = 0;
            // When reset is complete, the reset bit will be 0
            if (not this->read_reset(reset_val)) {
                this->m_state = RESET;
            } else if ((reset_val & RESET_VALUE) == 0) {
                // If power on and configure are successful, move to RUNNING
                if (this->power_on()) {
                    this->m_state = CONFIGURE;
                } else {
                    this->m_state = RESET;
                }
            }
            break;
        }
        case CONFIGURE: {
            if (this->configure_device()) {
                this->m_state = RUNNING;
            } else {
                this->m_state = RESET;
            }
            break;
        }
        case RUNNING: {
            U8 data[DATA_LENGTH];
            U8 registerAddress = DATA_BASE_REGISTER;

            Fw::Buffer writeBuffer(&registerAddress, 1);
            Fw::Buffer readBuffer(data, DATA_LENGTH);
            // If bus write fails, state machine is reset, so just return
            if (this->bus_write(writeBuffer, readBuffer)) {
                RawImuData raw = this->deserialize_raw_data(readBuffer);

                // This code will read the currently scaled parameters
                Fw::ParamValid paramValid;
                const AccelerationRange accelerationRange = this->paramGet_ACCELEROMETER_RANGE(paramValid);
                FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
                const GyroscopeRange gyroscopeRange = this->paramGet_GYROSCOPE_RANGE(paramValid);
                FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));

                const ImuData imuData = this->convert_raw_data(raw, accelerationRange, gyroscopeRange);
                this->tlmWrite_Reading(imuData);
            } else {
                this->m_state = RESET;
            }
            break;
        }
        default:
            FW_ASSERT(0, this->m_state);
            break;
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

bool ImuManager ::reset() {
    U8 reset_sequence[] = {POWER_MGMT_REGISTER, RESET_VALUE};
    Fw::Buffer writeBuffer(reset_sequence, sizeof(reset_sequence));
    Fw::Buffer readBuffer;
    return this->bus_write(writeBuffer, readBuffer);
}

bool ImuManager ::power_on() {
    U8 power_on_sequence[] = {POWER_MGMT_REGISTER, POWER_ON_VALUE};
    Fw::Buffer writeBuffer(power_on_sequence, sizeof(power_on_sequence));
    Fw::Buffer readBuffer;
    return this->bus_write(writeBuffer, readBuffer);
}

bool ImuManager ::bus_write(Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    Drv::I2cStatus status;
    FW_ASSERT(writeBuffer.isValid());
    if (readBuffer.isValid()) {
	status = this->busWriteRead_out(0, DEVICE_ADDRESS, writeBuffer, readBuffer);
    } else {
	status = this->busWrite_out(0, DEVICE_ADDRESS, writeBuffer);
    }
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(DEVICE_ADDRESS, status);
        return false;
    }
    return true;
}

bool ImuManager ::read_reset(U8& value) {
    U8 registerAddress = POWER_MGMT_REGISTER;
    Fw::Buffer writeBuffer(&registerAddress, sizeof(registerAddress));
    Fw::Buffer readBuffer(&value, sizeof(value));
    return this->bus_write(writeBuffer, readBuffer);
}

bool ImuManager ::configure_device() {
    Fw::ParamValid paramValid;
    // Read accelerometer parameter and configure
    {
        const AccelerationRange accelerationRange = this->paramGet_ACCELEROMETER_RANGE(paramValid);
        FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));

        U8 accel_config_sequence[] = {ACCEL_CONFIG_REGISTER, this->accelerometer_range_to_register(accelerationRange)};
        Fw::Buffer writeBuffer(accel_config_sequence, sizeof(accel_config_sequence));
        Fw::Buffer readBuffer;
        if (not this->bus_write(writeBuffer, readBuffer)) {
            return false;
        }
    }
    // Read gyroscope parameter and configure
    {
        const GyroscopeRange gyroscopeRange = this->paramGet_GYROSCOPE_RANGE(paramValid);
        FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
        U8 gyro_config_sequence[] = {GYRO_CONFIG_REGISTER, this->gyroscope_range_to_register(gyroscopeRange)};
        Fw::Buffer writeBuffer(gyro_config_sequence, sizeof(gyro_config_sequence));
        Fw::Buffer readBuffer;
        if (not this->bus_write(writeBuffer, readBuffer)) {
            return false;
        }
    }
    return true;
}

ImuManager::RawImuData ImuManager ::deserialize_raw_data(Fw::Buffer& buffer) {
    auto deserializer = buffer.getDeserializer();
    RawImuData raw;
    deserializer.deserialize(raw.acceleration[0]);
    deserializer.deserialize(raw.acceleration[1]);
    deserializer.deserialize(raw.acceleration[2]);
    deserializer.deserialize(raw.temperature);
    deserializer.deserialize(raw.gyroscope[0]);
    deserializer.deserialize(raw.gyroscope[1]);
    deserializer.deserialize(raw.gyroscope[2]);
    return raw;
}

ImuData ImuManager ::convert_raw_data(const RawImuData& raw,
                                      const AccelerationRange& accelerationRange,
                                      const GyroscopeRange& gyroscopeRange) {
    // Set the values of the IMU data by multiplying by conversion factors
    MpuImu::ImuData imuData;
    imuData.get_acceleration().set_x(static_cast<F32>(raw.acceleration[0]) * 1.0f / static_cast<F32>(accelerationRange));
    imuData.get_acceleration().set_y(static_cast<F32>(raw.acceleration[1]) * 1.0f / static_cast<F32>(accelerationRange));
    imuData.get_acceleration().set_z(static_cast<F32>(raw.acceleration[2]) * 1.0f / static_cast<F32>(accelerationRange));
    imuData.set_temperature((static_cast<F32>(raw.temperature) / TEMPERATURE_SCALAR) + TEMPERATURE_OFFSET);
    imuData.get_rotation().set_x(static_cast<F32>(raw.gyroscope[0]) * 10.0f / static_cast<F32>(gyroscopeRange));
    imuData.get_rotation().set_y(static_cast<F32>(raw.gyroscope[1]) * 10.0f / static_cast<F32>(gyroscopeRange));
    imuData.get_rotation().set_z(static_cast<F32>(raw.gyroscope[2]) * 10.0f / static_cast<F32>(gyroscopeRange));
    return imuData;
}

U8 ImuManager ::accelerometer_range_to_register(AccelerationRange range) {
    U8 registerValue = 0;
    switch (range.e) {
        case AccelerationRange::RANGE_2G:
            registerValue = ACCEL_CONFIG_2G;
            break;
        case AccelerationRange::RANGE_4G:
            registerValue = ACCEL_CONFIG_4G;
            break;
        case AccelerationRange::RANGE_8G:
            registerValue = ACCEL_CONFIG_8G;
            break;
        case AccelerationRange::RANGE_16G:
            registerValue = ACCEL_CONFIG_16G;
            break;
        default:
            FW_ASSERT(0, range.e);
            break;
    }
    return registerValue;
}

U8 ImuManager ::gyroscope_range_to_register(GyroscopeRange range) {
    U8 registerValue = 0;
    switch (range.e) {
        case GyroscopeRange::RANGE_250DEG:
            registerValue = GYRO_CONFIG_250DEG;
            break;
        case GyroscopeRange::RANGE_500DEG:
            registerValue = GYRO_CONFIG_500DEG;
            break;
        case GyroscopeRange::RANGE_1000DEG:
            registerValue = GYRO_CONFIG_1000DEG;
            break;
        case GyroscopeRange::RANGE_2000DEG:
            registerValue = GYRO_CONFIG_2000DEG;
            break;
        default:
            FW_ASSERT(0, range.e);
            break;
    }
    return registerValue;
}
}  // namespace MpuImu
