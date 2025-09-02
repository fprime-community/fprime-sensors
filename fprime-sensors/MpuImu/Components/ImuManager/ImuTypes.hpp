// ======================================================================
// \title  ImuTypes.hpp
// \author mstarch
// \brief  hpp file for defining constants and types for ImuHelpers.cpp
// ======================================================================

#ifndef MpuImu_ImuTypes_HPP
#define MpuImu_ImuTypes_HPP
#include "Fw/FPrimeBasicTypes.hpp"

namespace MpuImu {
    static constexpr U8 DATA_LENGTH = (6 + 1) * sizeof(U16);  // 6 DoF + temperature
    static constexpr U8 DATA_BASE_REGISTER = 0x3B;
    static constexpr U8 DEVICE_DEFAULT_ADDRESS = 0x68;
    static constexpr U8 POWER_MGMT_REGISTER = 0x6B;
    static constexpr U8 RESET_VALUE = 0x80;
    static constexpr U8 POWER_ON_VALUE = 0x00;
    static constexpr U8 GYRO_CONFIG_REGISTER = 0x1B;
    static constexpr U8 ACCEL_CONFIG_REGISTER = 0x1C;

    // Configuration values for the accelerometer and gyroscope
    static constexpr U8 ACCEL_CONFIG_2G = 0x00;
    static constexpr U8 ACCEL_CONFIG_4G = 0x08;
    static constexpr U8 ACCEL_CONFIG_8G = 0x10;
    static constexpr U8 ACCEL_CONFIG_16G = 0x18;
    static constexpr U8 GYRO_CONFIG_250DEG = 0x00;
    static constexpr U8 GYRO_CONFIG_500DEG = 0x08;
    static constexpr U8 GYRO_CONFIG_1000DEG = 0x10;
    static constexpr U8 GYRO_CONFIG_2000DEG = 0x18;
    static constexpr F32 TEMPERATURE_SCALAR = 340.0f;
    static constexpr F32 TEMPERATURE_OFFSET = 36.53f;

    //! RawImuData: basic structure of imu data as read from the device
    struct RawImuData {
        U16 acceleration[3];
        U16 temperature;
        U16 gyroscope[3];
    };
}
#endif