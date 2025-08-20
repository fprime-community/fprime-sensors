// ======================================================================
// \title  BmpManager.hpp
// \author aborjigin & Generated
// \brief  hpp file for BmpManager component implementation class
// ======================================================================

#ifndef Bmp280_BmpManager_HPP
#define Bmp280_BmpManager_HPP

#include "fprime-sensors/Bmp280/Components/BmpManager/BmpManagerComponentAc.hpp"

namespace Bmp280 {

class BmpManager final : public BmpManagerComponentBase {
  public:
    static constexpr U8 CHIP_ID_REGISTER = 0xD0;
    static constexpr U8 CHIP_ID_VALUE = 0x58;
    static constexpr U8 CTRL_MEAS_REGISTER = 0xF4;
    static constexpr U8 CONFIG_REGISTER = 0xF5;
    static constexpr U8 PRESSURE_MSB_REGISTER = 0xF7;
    static constexpr U8 DEVICE_SELECT = 0;  // SPI chip select
    static constexpr U8 RESET_REGISTER = 0xE0;
    static constexpr U8 RESET_VALUE = 0xB6;
    static constexpr U8 STATUS_REGISTER = 0xF3;
    static constexpr U8 CALIB_DATA_REGISTER = 0x88;
    static constexpr U8 CALIB_DATA_LENGTH = 24;
    static constexpr U8 MEASUREMENT_DATA_LENGTH = 6;
    static constexpr U8 NORMAL_MODE = 0x03;
    static constexpr U8 FORCED_MODE = 0x01;

    struct CalibrationData {
        U16 dig_T1;
        I16 dig_T2;
        I16 dig_T3;
        U16 dig_P1;
        I16 dig_P2;
        I16 dig_P3;
        I16 dig_P4;
        I16 dig_P5;
        I16 dig_P6;
        I16 dig_P7;
        I16 dig_P8;
        I16 dig_P9;
    };

    struct RawBmpData {
        U32 pressure;
        U32 temperature;
    };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BmpManager object
    BmpManager(const char* const compName  //!< The component name
    );

    //! Destroy BmpManager object
    ~BmpManager();

    //! Converts raw BMP280 data to the telemetry structure
    static Bmp280Data convert_raw_data(const RawBmpData& raw, const CalibrationData& calib, F32 seaLevelPressure);

    //! Calculates altitude from pressure using barometric formula
    static F32 calculate_altitude(F32 pressure, F32 seaLevelPressure);

    //! Pressure oversampling to register value
    static U8 pressure_oversampling_to_register(PressureOversampling oversampling);

    //! Temperature oversampling to register value
    static U8 temperature_oversampling_to_register(TemperatureOversampling oversampling);

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
    //! Scheduling port for reading from BMP280 and writing to telemetry
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Resets the BMP280
    bool reset();

    //! Read the chip ID
    bool read_chip_id(U8& id);

    //! Read the status register
    bool read_status(U8& status);

    //! Read calibration data
    bool read_calibration_data();

    //! Configure the BMP280
    bool configure_device();

    //! Trigger a measurement in forced mode
    bool trigger_measurement();

    //! Write to the SPI bus and handle errors
    bool spi_transfer(Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer);

    //! Deserializes raw data from the bus
    RawBmpData deserialize_raw_data(Fw::Buffer& buffer);

    //! State of the BMP280 component
    enum BmpState { RESET, STARTUP_DELAY, CHIP_ID_CHECK, CALIBRATION_READ, CONFIGURE, RUNNING };

    //! Tracks the state of the BMP280
    BmpState m_state;

    //! Startup delay counter
    U32 m_startupCounter;

    //! Calibration data
    CalibrationData m_calibration;

    //! Maximum number of reset attempts before giving up
    static constexpr U32 MAX_RESET_ATTEMPTS = 5;

    //! Number of cycles to wait after reset for device startup (approximately 2ms at typical scheduling rates)
    static constexpr U32 STARTUP_DELAY_CYCLES = 2;
};

}  // namespace Bmp280

#endif