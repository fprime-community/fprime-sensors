// ======================================================================
// \title  BmpManager.cpp
// \author aborjigin & Generated
// \brief  cpp file for BmpManager component implementation class
// ======================================================================

#include "fprime-sensors/Bmp280/Components/BmpManager/BmpManager.hpp"
#include <cmath>
#include <cstdio>  // For printf debugging

namespace Bmp280 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BmpManager ::BmpManager(const char* const compName) : BmpManagerComponentBase(compName), m_state(RESET), m_resetAttempts(0) {
    // printf("[BmpManager] Constructor: Starting in RESET state\n");
}

BmpManager ::~BmpManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void BmpManager ::parameterUpdated(FwPrmIdType id) {
    printf("[BmpManager] Parameter updated: ID=%d\n", static_cast<int>(id));
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    switch (id) {
        case PARAMID_PRESSURE_OVERSAMPLING: {
            // Read back the parameter value
            const PressureOversampling oversampling = this->paramGet_PRESSURE_OVERSAMPLING(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_PressureOversamplingUpdated(oversampling);
            // printf("[BmpManager] Pressure oversampling updated to %d, transitioning to CONFIGURE\n", static_cast<int>(oversampling.e));
            this->m_state = CONFIGURE;
            break;
        }
        case PARAMID_TEMPERATURE_OVERSAMPLING: {
            // Read back the parameter value
            const TemperatureOversampling oversampling = this->paramGet_TEMPERATURE_OVERSAMPLING(isValid);
            // NOTE: isValid is always VALID in parameterUpdated as it was just properly set
            FW_ASSERT(isValid == Fw::ParamValid::VALID, static_cast<FwAssertArgType>(isValid));
            this->log_ACTIVITY_HI_TemperatureOversamplingUpdated(oversampling);
            // printf("[BmpManager] Temperature oversampling updated to %d, transitioning to CONFIGURE\n", static_cast<int>(oversampling.e));
            this->m_state = CONFIGURE;
            break;
        }
        case PARAMID_SEA_LEVEL_PRESSURE: {
            // printf("[BmpManager] Sea level pressure parameter updated - no reconfiguration needed\n");
            break;
        }
        default:
            // printf("[BmpManager] ERROR: Unknown parameter ID %d\n", static_cast<int>(id));
            FW_ASSERT(0, static_cast<FwAssertArgType>(id));
            break;
    }
}

void BmpManager ::run_handler(FwIndexType portNum, U32 context) {
    // printf("[BmpManager] run_handler called, current state: %d\n", static_cast<int>(m_state));
    
    switch (this->m_state) {
        case RESET:
            // printf("[BmpManager] RESET state: attempt %d\n", m_resetAttempts);
            // If reset is successful, move to STARTUP_DELAY state
            if (this->reset()) {
                // printf("[BmpManager] Reset command sent successfully, waiting for startup delay\n");
                this->m_state = STARTUP_DELAY;
                this->m_startupCounter = STARTUP_DELAY_CYCLES;
            } else {
                // printf("[BmpManager] Reset command failed\n");
                m_resetAttempts++;
                if (m_resetAttempts > MAX_RESET_ATTEMPTS) {
                    // printf("[BmpManager] ERROR: Maximum reset attempts exceeded\n");
                    // Stay in RESET state but log error
                    m_resetAttempts = 0;
                }
            }
            break;
            
        case STARTUP_DELAY:
            // printf("[BmpManager] STARTUP_DELAY state: %d cycles remaining\n", m_startupCounter);
            m_startupCounter--;
            if (m_startupCounter <= 0) {
                // printf("[BmpManager] Startup delay complete, transitioning to CHIP_ID_CHECK\n");
                this->m_state = CHIP_ID_CHECK;
            }
            break;
            
        case CHIP_ID_CHECK: {
            // printf("[BmpManager] CHIP_ID_CHECK state\n");
            U8 chip_id = 0;
            if (this->read_chip_id(chip_id)) {
                // printf("[BmpManager] Chip ID read successfully: 0x%02X\n", chip_id);
                if (chip_id == CHIP_ID_VALUE) {
                    // printf("[BmpManager] Chip ID matches expected value (0x%02X), transitioning to CALIBRATION_READ\n", CHIP_ID_VALUE);
                    this->m_state = CALIBRATION_READ;
                } else {
                    // printf("[BmpManager] ERROR: Chip ID mismatch. Expected 0x%02X, got 0x%02X. Resetting.\n", CHIP_ID_VALUE, chip_id);
                    this->m_state = RESET;
                    m_resetAttempts = 0;
                }
            } else {
                // printf("[BmpManager] ERROR: Failed to read chip ID. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
            }
            break;
        }
        case CALIBRATION_READ:
            //  printf("[BmpManager] CALIBRATION_READ state\n");
            if (this->read_calibration_data()) {
                // printf("[BmpManager] Calibration data read successfully, transitioning to CONFIGURE\n");
                this->m_state = CONFIGURE;
            } else {
                // printf("[BmpManager] ERROR: Failed to read calibration data. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
            }
            break;
        case CONFIGURE:
            // printf("[BmpManager] CONFIGURE state\n");
            if (this->configure_device()) {
                // printf("[BmpManager] Device configured successfully, transitioning to RUNNING\n");
                this->m_state = RUNNING;
            } else {
                // printf("[BmpManager] ERROR: Failed to configure device. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
            }
            break;
        case RUNNING: {
            // printf("[BmpManager] RUNNING state\n");
            
            // Step 1: Check if measurement is ready
            U8 status = 0;
            if (!this->read_status(status)) {
                // printf("[BmpManager] ERROR: Failed to read status register. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
                break;
            }
            
            // Check if measurement is in progress (bit 3) or if data is being updated (bit 0)
            if ((status & 0x08) || (status & 0x01)) {
                // printf("[BmpManager] Measurement in progress (status=0x%02X), waiting...\n", status);
                break; // Wait for next cycle
            }
            
            // Step 2: Trigger a new measurement in forced mode
            if (!this->trigger_measurement()) {
                // printf("[BmpManager] ERROR: Failed to trigger measurement. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
                break;
            }
            
            // Step 3: Read measurement data
            // BMP280 SPI protocol: read 6 bytes starting from PRESSURE_MSB_REGISTER
            U8 spiData[MEASUREMENT_DATA_LENGTH + 1];
            spiData[0] = PRESSURE_MSB_REGISTER | 0x80; // Register address with MSB=1 for read
            for (int i = 1; i <= MEASUREMENT_DATA_LENGTH; i++) {
                spiData[i] = 0x00; // Dummy bytes
            }

            Fw::Buffer writeBuffer(spiData, MEASUREMENT_DATA_LENGTH + 1);
            Fw::Buffer readBuffer(spiData, MEASUREMENT_DATA_LENGTH + 1);
            
            if (this->spi_transfer(writeBuffer, readBuffer)) {
                // printf("[BmpManager] Raw measurement data read successfully\n");
                
                // Create buffer pointing to actual data (skip first byte which is register echo)
                Fw::Buffer dataBuffer(&readBuffer.getData()[1], MEASUREMENT_DATA_LENGTH);
                RawBmpData raw = this->deserialize_raw_data(dataBuffer);
                printf("[BmpManager] Raw pressure: %u, Raw temperature: %u\n", raw.pressure, raw.temperature);
                
                // Get sea level pressure parameter
                Fw::ParamValid paramValid;
                F32 seaLevelPressure = this->paramGet_SEA_LEVEL_PRESSURE(paramValid);
                FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
                
                const Bmp280Data bmpData = this->convert_raw_data(raw, this->m_calibration, seaLevelPressure);
                printf("[BmpManager] Converted data - Pressure: %.2f Pa, Temperature: %.2f C, Altitude: %.2f m\n", 
                       bmpData.getpressure(), bmpData.gettemperature(), bmpData.getaltitude());
                this->tlmWrite_Reading(bmpData);
            } else {
                printf("[BmpManager] ERROR: Failed to read measurement data. Resetting.\n");
                this->m_state = RESET;
                m_resetAttempts = 0;
            }
            break;
        }
        default:
            // printf("[BmpManager] ERROR: Unknown state %d\n", static_cast<int>(m_state));
            FW_ASSERT(0, this->m_state);
            break;
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

bool BmpManager ::reset() {
    // printf("[BmpManager] Sending reset command (0x%02X -> 0x%02X)\n", RESET_REGISTER, RESET_VALUE);
    // For SPI writes, register address MSB must be 0
    U8 reset_sequence[] = {RESET_REGISTER & 0x7F, RESET_VALUE}; // Clear MSB for write
    Fw::Buffer writeBuffer(reset_sequence, sizeof(reset_sequence));
    Fw::Buffer readBuffer;
    bool success = this->spi_transfer(writeBuffer, readBuffer);
    // printf("[BmpManager] Reset command %s\n", success ? "succeeded" : "failed");
    return success;
}

bool BmpManager ::read_chip_id(U8& id) {
    // printf("[BmpManager] Reading chip ID from register 0x%02X\n", CHIP_ID_REGISTER);
    
    // BMP280 SPI protocol: for reads, MSB must be 1, and we need a 2-byte transaction
    // Byte 0: Register address with MSB=1 (0xD0 | 0x80 = 0xD0, already has MSB set)
    // Byte 1: Dummy byte (will be replaced with register value)
    U8 spiData[2] = {CHIP_ID_REGISTER | 0x80, 0x00}; // Ensure MSB is set for read
    
    Fw::Buffer writeBuffer(spiData, 2);
    Fw::Buffer readBuffer(spiData, 2); // Reuse same buffer for read
    
    bool success = this->spi_transfer(writeBuffer, readBuffer);
    
    if (success) {
        // The chip ID will be in the second byte of the response
        id = readBuffer.getData()[1];
        // printf("[BmpManager] Chip ID read: 0x%02X\n", id);
    } else {
        // printf("[BmpManager] Failed to read chip ID\n");
        id = 0;
    }
    return success;
}

bool BmpManager ::read_status(U8& status) {
    // printf("[BmpManager] Reading status from register 0x%02X\n", STATUS_REGISTER);
    
    // BMP280 SPI protocol: 2-byte transaction for register read
    U8 spiData[2] = {STATUS_REGISTER | 0x80, 0x00}; // Ensure MSB is set for read
    
    Fw::Buffer writeBuffer(spiData, 2);
    Fw::Buffer readBuffer(spiData, 2);
    
    bool success = this->spi_transfer(writeBuffer, readBuffer);
    
    if (success) {
        status = readBuffer.getData()[1];
        // printf("[BmpManager] Status read: 0x%02X\n", status);
    } else {
        // printf("[BmpManager] Failed to read status\n");
        status = 0;
    }
    
    return success;
}

bool BmpManager ::read_calibration_data() {
    // printf("[BmpManager] Reading calibration data from register 0x%02X, length %d bytes\n", 
    //        CALIB_DATA_REGISTER, CALIB_DATA_LENGTH);
    
    // BMP280 SPI protocol: for multi-byte read, send register address + read 24 bytes
    // Total transaction: 1 byte register address + 24 bytes data = 25 bytes
    U8 spiData[CALIB_DATA_LENGTH + 1];
    spiData[0] = CALIB_DATA_REGISTER | 0x80; // Register address with MSB=1 for read
    for (int i = 1; i <= CALIB_DATA_LENGTH; i++) {
        spiData[i] = 0x00; // Dummy bytes that will be filled with calibration data
    }
    
    Fw::Buffer writeBuffer(spiData, CALIB_DATA_LENGTH + 1);
    Fw::Buffer readBuffer(spiData, CALIB_DATA_LENGTH + 1);
    
    if (this->spi_transfer(writeBuffer, readBuffer)) {
        // printf("[BmpManager] Calibration data read successfully, parsing coefficients\n");
        
        // Parse calibration data from the buffer (skip first byte which is the register echo)
        // Registers 0x88-0x9F contain 12 calibration coefficients (24 bytes)
        U8* data = &readBuffer.getData()[1]; // Skip the first byte (register address echo)
        
        m_calibration.dig_T1 = (static_cast<U16>(data[1]) << 8) | data[0];
        m_calibration.dig_T2 = (static_cast<I16>(data[3]) << 8) | data[2];
        m_calibration.dig_T3 = (static_cast<I16>(data[5]) << 8) | data[4];
        
        m_calibration.dig_P1 = (static_cast<U16>(data[7]) << 8) | data[6];
        m_calibration.dig_P2 = (static_cast<I16>(data[9]) << 8) | data[8];
        m_calibration.dig_P3 = (static_cast<I16>(data[11]) << 8) | data[10];
        m_calibration.dig_P4 = (static_cast<I16>(data[13]) << 8) | data[12];
        m_calibration.dig_P5 = (static_cast<I16>(data[15]) << 8) | data[14];
        m_calibration.dig_P6 = (static_cast<I16>(data[17]) << 8) | data[16];
        m_calibration.dig_P7 = (static_cast<I16>(data[19]) << 8) | data[18];
        m_calibration.dig_P8 = (static_cast<I16>(data[21]) << 8) | data[20];
        m_calibration.dig_P9 = (static_cast<I16>(data[23]) << 8) | data[22];
        
        // printf("[BmpManager] Calibration coefficients:\n");
        // printf("  dig_T1=%u, dig_T2=%d, dig_T3=%d\n", m_calibration.dig_T1, m_calibration.dig_T2, m_calibration.dig_T3);
        // printf("  dig_P1=%u, dig_P2=%d, dig_P3=%d\n", m_calibration.dig_P1, m_calibration.dig_P2, m_calibration.dig_P3);
        // printf("  dig_P4=%d, dig_P5=%d, dig_P6=%d\n", m_calibration.dig_P4, m_calibration.dig_P5, m_calibration.dig_P6);
        // printf("  dig_P7=%d, dig_P8=%d, dig_P9=%d\n", m_calibration.dig_P7, m_calibration.dig_P8, m_calibration.dig_P9);
        
        return true;
    }
    printf("[BmpManager] Failed to read calibration data\n");
    return false;
}

bool BmpManager ::configure_device() {
    // printf("[BmpManager] Configuring device\n");
    Fw::ParamValid paramValid;
    const PressureOversampling pressureOversampling = this->paramGet_PRESSURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
    const TemperatureOversampling temperatureOversampling = this->paramGet_TEMPERATURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));

    // According to datasheet, CTRL_MEAS register (0xF4) format:
    // bits 7:5 = osrs_t (temperature oversampling)  
    // bits 4:2 = osrs_p (pressure oversampling)
    // bits 1:0 = mode (00=sleep, 01=forced, 11=normal)
    U8 ctrl_meas_value = (static_cast<U8>(temperatureOversampling) << 5) | 
                         (static_cast<U8>(pressureOversampling) << 2) | 
                         0x00; // Start in sleep mode first
    
    //printf("[BmpManager] Setting CTRL_MEAS register to 0x%02X (temp_os=%d, press_os=%d, mode=sleep)\n", 
        //    ctrl_meas_value, static_cast<int>(temperatureOversampling.e), static_cast<int>(pressureOversampling.e));
    
    // For SPI writes, register address MSB must be 0
    U8 config_sequence[] = {CTRL_MEAS_REGISTER & 0x7F, ctrl_meas_value}; // Clear MSB for write
    Fw::Buffer writeBuffer(config_sequence, sizeof(config_sequence));
    Fw::Buffer readBuffer;
    
    bool success = this->spi_transfer(writeBuffer, readBuffer);
    
    if (success) {
        // Also configure the CONFIG register (0xF5) for IIR filter and standby time
        // bits 7:5 = t_sb (standby time in normal mode) - not used in forced mode
        // bits 4:2 = filter (IIR filter coefficient)
        // bit 0 = spi3w_en (3-wire SPI enable)
        U8 config_value = 0x00; // No IIR filter, 4-wire SPI
        U8 config_sequence[] = {CONFIG_REGISTER & 0x7F, config_value}; // Clear MSB for write
        Fw::Buffer configWriteBuffer(config_sequence, sizeof(config_sequence));
        Fw::Buffer configReadBuffer;
        
        // printf("[BmpManager] Setting CONFIG register to 0x%02X\n", config_value);
        success = this->spi_transfer(configWriteBuffer, configReadBuffer);
    }
    
    // printf("[BmpManager] Device configuration %s\n", success ? "succeeded" : "failed");
    return success;
}

bool BmpManager ::trigger_measurement() {
    // printf("[BmpManager] Triggering measurement in forced mode\n");
    Fw::ParamValid paramValid;
    const PressureOversampling pressureOversampling = this->paramGet_PRESSURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));
    const TemperatureOversampling temperatureOversampling = this->paramGet_TEMPERATURE_OVERSAMPLING(paramValid);
    FW_ASSERT(paramValid != Fw::ParamValid::INVALID, static_cast<FwAssertArgType>(paramValid));

    // Set device to forced mode to trigger a measurement
    U8 ctrl_meas_value = (static_cast<U8>(temperatureOversampling) << 5) | 
                         (static_cast<U8>(pressureOversampling) << 2) | 
                         FORCED_MODE;
    
    // For SPI writes, register address MSB must be 0
    U8 config_sequence[] = {CTRL_MEAS_REGISTER & 0x7F, ctrl_meas_value}; // Clear MSB for write
    Fw::Buffer writeBuffer(config_sequence, sizeof(config_sequence));
    Fw::Buffer readBuffer;
    return this->spi_transfer(writeBuffer, readBuffer);
}

bool BmpManager ::spi_transfer(Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    // printf("[BmpManager] SPI transfer: writing %d bytes, reading %d bytes\n", 
    //        writeBuffer.getSize(), readBuffer.getSize());
    
    // Print write data for debugging
    if (writeBuffer.getSize() > 0) {
        // printf("[BmpManager] Write data: ");
        for (U32 i = 0; i < writeBuffer.getSize(); i++) {
            // printf("0x%02X ", writeBuffer.getData()[i]);
        }
        // printf("\n");
    }
    
    // Perform the SPI transfer
    this->spiReadWrite_out(0, writeBuffer, readBuffer);
    
    // Print read data for debugging
    // if (readBuffer.getSize() > 0) {
    //     printf("[BmpManager] Read data: ");
    //     for (U32 i = 0; i < readBuffer.getSize(); i++) {
    //         printf("0x%02X ", readBuffer.getData()[i]);
    //     }
    //     printf("\n");
    // }
    
    // TODO: In a real implementation, we should check for SPI errors
    // For now, we assume success but this is where error detection should be added
    // The Linux SPI driver logs warnings internally if ioctl fails
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
    
    // BMP280 Temperature Compensation (from datasheet)
    // Returns temperature in DegC, resolution is 0.01 DegC
    I32 adc_T = static_cast<I32>(raw.temperature);
    I32 var1, var2, t_fine;
    var1 = ((((adc_T >> 3) - (static_cast<I32>(calib.dig_T1) << 1))) * static_cast<I32>(calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - static_cast<I32>(calib.dig_T1)) * ((adc_T >> 4) - static_cast<I32>(calib.dig_T1))) >> 12) * static_cast<I32>(calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    F32 temperature = static_cast<F32>((t_fine * 5 + 128) >> 8) / 100.0f;
    
    // BMP280 Pressure Compensation (from datasheet)
    // Returns pressure in Pa as unsigned 32 bit integer
    I32 adc_P = static_cast<I32>(raw.pressure);
    I64 var1_64, var2_64, p_64;
    var1_64 = static_cast<I64>(t_fine) - 128000;
    var2_64 = var1_64 * var1_64 * static_cast<I64>(calib.dig_P6);
    var2_64 = var2_64 + ((var1_64 * static_cast<I64>(calib.dig_P5)) << 17);
    var2_64 = var2_64 + (static_cast<I64>(calib.dig_P4) << 35);
    var1_64 = ((var1_64 * var1_64 * static_cast<I64>(calib.dig_P3)) >> 8) + ((var1_64 * static_cast<I64>(calib.dig_P2)) << 12);
    var1_64 = (((static_cast<I64>(1) << 47) + var1_64)) * static_cast<I64>(calib.dig_P1) >> 33;
    
    F32 pressure = 0.0f;
    if (var1_64 != 0) {
        p_64 = 1048576 - adc_P;
        p_64 = (((p_64 << 31) - var2_64) * 3125) / var1_64;
        var1_64 = (static_cast<I64>(calib.dig_P9) * (p_64 >> 13) * (p_64 >> 13)) >> 25;
        var2_64 = (static_cast<I64>(calib.dig_P8) * p_64) >> 19;
        p_64 = ((p_64 + var1_64 + var2_64) >> 8) + (static_cast<I64>(calib.dig_P7) << 4);
        pressure = static_cast<F32>(p_64) / 256.0f;
    }
    
    // Calculate altitude using barometric formula
    F32 altitude = calculate_altitude(pressure, seaLevelPressure);
    
    // printf("[BmpManager] Temperature compensation: raw=%u -> %.2f°C (t_fine=%d)\n", raw.temperature, temperature, t_fine);
    // printf("[BmpManager] Pressure compensation: raw=%u -> %.2f Pa\n", raw.pressure, pressure);
    
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
    
    // printf("[BmpManager] Calculating altitude: pressure=%.2f Pa, sea_level=%.2f Pa\n", pressure, seaLevelPressure);
    
    if (seaLevelPressure <= 0.0f || pressure <= 0.0f) {
        // printf("[BmpManager] ERROR: Invalid pressure values for altitude calculation\n");
        return 0.0f;  // Return 0 for invalid inputs
    }
    
    F32 ratio = pressure / seaLevelPressure;
    if (ratio <= 0.0f) {
        // printf("[BmpManager] ERROR: Invalid pressure ratio for altitude calculation\n");
        return 0.0f;
    }
    
    // Using standard atmosphere model constants
    static const F32 STANDARD_ALTITUDE_CONSTANT = 44330.0f;
    static const F32 PRESSURE_EXPONENT = 1.0f / 5.255f;
    
    F32 altitude = STANDARD_ALTITUDE_CONSTANT * (1.0f - pow(ratio, PRESSURE_EXPONENT));
    
    // printf("[BmpManager] Altitude calculated: %.2f m (ratio=%.4f)\n", altitude, ratio);
    
    return altitude;
}

}  // namespace Bmp280 