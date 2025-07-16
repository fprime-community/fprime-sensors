// ======================================================================
// \title  BmpManager.cpp
// \author Generated
// \brief  cpp file for BmpManager component implementation class
// ======================================================================

#include "fprime-sensors/Bmp280/Components/BmpManager/BmpManager.hpp"
#include <cmath>

namespace Bmp280 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BmpManager ::BmpManager(const char* const compName) : BmpManagerComponentBase(compName), m_state(RESET) {}

BmpManager ::~BmpManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void BmpManager ::parameterUpdated(FwPrmIdType id) {
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    switch (id) {
        case PARAMID_PRESSURE_OVERSAMPLING: {
            // Read back the parameter value
            const PressureOversampling oversampling = this->paramGet_PRESSURE_OVERSAMPLING(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_PressureOversamplingUpdated(oversampling);
            this->m_state = CONFIGURE;
            break;
        }
        case PARAMID_TEMPERATURE_OVERSAMPLING: {
            // Read back the parameter value
            const TemperatureOversampling oversampling = this->paramGet_TEMPERATURE_OVERSAMPLING(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_TemperatureOversamplingUpdated(oversampling);
            this->m_state = CONFIGURE;
            break;
        }
        case PARAMID_SEA_LEVEL_PRESSURE: {
            // Sea level pressure parameter updated - no need to reconfigure device
            break;
        }
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(id));
            break;
    }
}

void BmpManager ::run_handler(FwIndexType portNum, U32 context) {
    switch (this->m_state) {
        case RESET:
            // If reset is successful, move to CHIP_ID_CHECK state
            if (this->reset()) {
                this->m_state = CHIP_ID_CHECK;
            }
            break;
        case CHIP_ID_CHECK: {
            U8 chip_id = 0;
            if (this->read_chip_id(chip_id)) {
                if (chip_id == CHIP_ID_VALUE) {
                    this->m_state = CALIBRATION_READ;
                } else {
                    this->m_state = RESET;
                }
            } else {
                this->m_state = RESET;
            }
            break;
        }
        case CALIBRATION_READ:
            if (this->read_calibration_data()) {
                this->m_state = CONFIGURE;
            } else {
                this->m_state = RESET;
            }
            break;
        case CONFIGURE:
            if (this->configure_device()) {
                this->m_state = RUNNING;
            } else {
                this->m_state = RESET;
            }
            break;
        case RUNNING: {
            U8 data[MEASUREMENT_DATA_LENGTH];
            U8 registerAddress = PRESSURE_MSB_REGISTER;

            Fw::Buffer writeBuffer(&registerAddress, 1);
            Fw::Buffer readBuffer(data, MEASUREMENT_DATA_LENGTH);
            
            if (this->spi_transfer(writeBuffer, readBuffer)) {
                RawBmpData raw = this->deserialize_raw_data(readBuffer);
                
                // Get sea level pressure parameter
                Fw::ParamValid paramValid;
                F32 seaLevelPressure = this->paramGet_SEA_LEVEL_PRESSURE(paramValid);
                FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
                
                const Bmp280Data bmpData = this->convert_raw_data(raw, this->m_calibration, seaLevelPressure);
                this->tlmWrite_Reading(bmpData);
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

bool BmpManager ::reset() {
    U8 reset_sequence[] = {RESET_REGISTER, RESET_VALUE};
    Fw::Buffer writeBuffer(reset_sequence, sizeof(reset_sequence));
    Fw::Buffer readBuffer;
    return this->spi_transfer(writeBuffer, readBuffer);
}

bool BmpManager ::read_chip_id(U8& id) {
    U8 registerAddress = CHIP_ID_REGISTER;
    Fw::Buffer writeBuffer(&registerAddress, 1);
    Fw::Buffer readBuffer(&id, 1);
    return this->spi_transfer(writeBuffer, readBuffer);
}

bool BmpManager ::read_calibration_data() {
    U8 data[CALIB_DATA_LENGTH];
    U8 registerAddress = CALIB_DATA_REGISTER;
    Fw::Buffer writeBuffer(&registerAddress, 1);
    Fw::Buffer readBuffer(data, CALIB_DATA_LENGTH);
    
    if (this->spi_transfer(writeBuffer, readBuffer)) {
        // Parse calibration data from the buffer
        auto deserializer = readBuffer.getDeserializer();
        // Implementation would parse calibration coefficients
        return true;
    }
    return false;
}

bool BmpManager ::configure_device() {
    Fw::ParamValid paramValid;
    const PressureOversampling pressureOversampling = this->paramGet_PRESSURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
    const TemperatureOversampling temperatureOversampling = this->paramGet_TEMPERATURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));

    U8 ctrl_meas_value = static_cast<U8>(temperatureOversampling) | 
                         static_cast<U8>(pressureOversampling) | 
                         FORCED_MODE;
    
    U8 config_sequence[] = {CTRL_MEAS_REGISTER, ctrl_meas_value};
    Fw::Buffer writeBuffer(config_sequence, sizeof(config_sequence));
    Fw::Buffer readBuffer;
    return this->spi_transfer(writeBuffer, readBuffer);
}

bool BmpManager ::spi_transfer(Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    // For SPI, we need to handle the transaction differently
    // SPI typically requires a single transfer with combined write/read buffer
    this->spiReadWrite_out(0, writeBuffer, readBuffer);
    return true;
}

BmpManager::RawBmpData BmpManager ::deserialize_raw_data(Fw::Buffer& buffer) {
    auto deserializer = buffer.getDeserializer();
    RawBmpData raw;
    U8 pressure_msb, pressure_lsb, pressure_xlsb;
    U8 temperature_msb, temperature_lsb, temperature_xlsb;
    
    deserializer.deserialize(pressure_msb);
    deserializer.deserialize(pressure_lsb);
    deserializer.deserialize(pressure_xlsb);
    deserializer.deserialize(temperature_msb);
    deserializer.deserialize(temperature_lsb);
    deserializer.deserialize(temperature_xlsb);
    
    raw.pressure = (static_cast<U32>(pressure_msb) << 12) | 
                   (static_cast<U32>(pressure_lsb) << 4) | 
                   (static_cast<U32>(pressure_xlsb) >> 4);
    
    raw.temperature = (static_cast<U32>(temperature_msb) << 12) | 
                      (static_cast<U32>(temperature_lsb) << 4) | 
                      (static_cast<U32>(temperature_xlsb) >> 4);
    
    return raw;
}

Bmp280Data BmpManager ::convert_raw_data(const RawBmpData& raw, const CalibrationData& calib, F32 seaLevelPressure) {
    Bmp280Data bmpData;
    // Implementation would use calibration coefficients to convert raw data
    // This is a placeholder implementation - actual BMP280 requires complex calibration
    
    // Convert to proper units
    F32 pressure = static_cast<F32>(raw.pressure) / 256.0f;  // Convert to Pascals
    F32 temperature = static_cast<F32>(raw.temperature) / 5120.0f;  // Convert to Celsius
    
    // Calculate altitude using barometric formula
    F32 altitude = calculate_altitude(pressure, seaLevelPressure);
    
    bmpData.setpressure(pressure);
    bmpData.settemperature(temperature);
    bmpData.setaltitude(altitude);
    
    return bmpData;
}

U8 BmpManager ::pressure_oversampling_to_register(PressureOversampling oversampling) {
    return static_cast<U8>(oversampling);
}

U8 BmpManager ::temperature_oversampling_to_register(TemperatureOversampling oversampling) {
    return static_cast<U8>(oversampling);
}

F32 BmpManager ::calculate_altitude(F32 pressure, F32 seaLevelPressure) {
    // Barometric formula for altitude calculation
    // altitude = 44330 * (1 - (pressure / seaLevelPressure)^(1/5.255))
    
    if (seaLevelPressure <= 0.0f || pressure <= 0.0f) {
        return 0.0f;  // Return 0 for invalid inputs
    }
    
    F32 ratio = pressure / seaLevelPressure;
    F32 altitude = 44330.0f * (1.0f - pow(ratio, 1.0f / 5.255f));
    
    return altitude;
}

}  // namespace Bmp280 