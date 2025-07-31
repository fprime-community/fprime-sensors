# Bmp280::BmpManager

Component emitting telemetry read from a BMP280 digital pressure sensor. This component communicates with the BMP280 sensor via SPI interface and provides barometric pressure, temperature, and calculated altitude data. All calculations follow the compensation formulas specified in the BMP280 datasheet: https://cdn-shop.adafruit.com/datasheets/BST-BMP280-DS001-11.pdf

## Requirements

| Name | Description | Validation |
|---|---|---|
| BMP280-001 | The BmpManager shall emit barometric pressure data in units of Pa (Pascals) | Unit-Test |
| BMP280-002 | The BmpManager shall emit temperature data in units of °C (degrees Celsius) | Unit-Test |
| BMP280-003 | The BmpManager shall emit altitude data in units of m (meters) | Unit-Test |
| BMP280-004 | The BmpManager shall communicate with the BMP280 sensor using SPI interface | Unit-Test |
| BMP280-005 | The BmpManager shall use compensation formulas from the BMP280 datasheet for accurate readings | Unit-Test |
| BMP280-006 | The BmpManager shall read calibration data from the sensor for temperature and pressure compensation | Unit-Test |
| BMP280-007 | The BmpManager shall verify sensor chip ID matches expected BMP280 value (0x58) | Unit-Test |

**SPI Configuration Note**: If you wish to change the SPI connection settings, modify the device and chip select values in `fprime-sensors-reference/ReferenceDeployment/Main.cpp` at lines 96-97:
```cpp
inputs.bmp.device.device = 0; // SPI bus 0
inputs.bmp.device.select = 0; // SPI chip select 0
```

## Port Descriptions

| Name | Description |
|---|---|
| run | Scheduling input port for periodic sensor reading and telemetry emission |
| spiReadWrite | Output port for SPI bus communication with the BMP280 sensor |
| timeCaller | Port for requesting current time for telemetry timestamps |
| tlmOut | Port for sending telemetry channels to downlink |
| CmdDisp | Command receive port for handling component commands |
| CmdReg | Command registration port |
| CmdStatus | Command response port |
| LogText | Text event port for logging messages |
| PrmGet | Parameter get port |
| PrmSet | Parameter set port |

## Component States

The BmpManager operates as a finite state machine with the following states:

| Name | Description |
|---|---|
| RESET | Initial state where the component sends a reset command to the BMP280 sensor. Retries up to 5 times if reset fails. |
| STARTUP_DELAY | Waits for the sensor to complete its startup sequence after reset (approximately 2ms delay). |
| CHIP_ID_CHECK | Reads and verifies the sensor's chip ID register (0xD0) matches the expected BMP280 value (0x58). |
| CALIBRATION_READ | Reads 24 bytes of calibration data from registers starting at 0x88, used for temperature and pressure compensation. |
| CONFIGURE | Configures the sensor with pressure and temperature oversampling settings based on component parameters. |
| RUNNING | Normal operation state where the component triggers measurements in forced mode and reads sensor data for telemetry. |

State transitions occur based on successful completion of operations. Any failure in states CHIP_ID_CHECK through RUNNING will cause the component to return to RESET state.

## Parameters

| Name | Description |
|---|---|
| PRESSURE_OVERSAMPLING | Controls pressure measurement oversampling (SKIP, 1X, 2X, 4X, 8X, 16X). Default: OVERSAMPLE_1X |
| TEMPERATURE_OVERSAMPLING | Controls temperature measurement oversampling (SKIP, 1X, 2X, 4X, 8X, 16X). Default: OVERSAMPLE_1X |
| SEA_LEVEL_PRESSURE | Sea-level pressure in Pa used for altitude calculation. Default: 101325.0 Pa |

**SEA_LEVEL_PRESSURE Configuration Note**: Can change in GDS as command or change the default in `BmpManager.fpp` line 34
``` 
param SEA_LEVEL_PRESSURE: F32 default 101325.0
```


## Events

| Name | Description |
|---|---|
| PressureOversamplingUpdated | Emitted when pressure oversampling parameter is updated |
| TemperatureOversamplingUpdated | Emitted when temperature oversampling parameter is updated |
| SpiError | Warning event emitted when SPI communication fails (throttled to prevent spam) |

## Telemetry

| Name | Description |
|---|---|
| Reading | BMP280 sensor data containing pressure (Pa), temperature (°C), and calculated altitude (m) |

The telemetry structure (`Bmp280Data`) contains:
- **pressure**: Barometric pressure in Pascals, compensated using BMP280 datasheet formulas
- **temperature**: Temperature in degrees Celsius, compensated using BMP280 datasheet formulas  
- **altitude**: Calculated altitude in meters using the barometric formula and sea-level pressure parameter

## Unit Tests

| Name | Description | Output | Coverage |
|---|---|---|---|
| TestInitialState | Verify component starts in RESET state | Component initializes properly | State machine initialization |
| TestChipIdValidation | Test chip ID reading and validation | Correct chip ID verification | Chip ID check functionality |
| TestCalibrationDataRead | Verify calibration data is read correctly | Calibration coefficients populated | Calibration reading |
| TestDataConversion | Test raw sensor data conversion to engineering units | Accurate pressure, temperature, altitude | Data compensation algorithms |
| TestSpiCommunication | Verify SPI read/write operations | Successful sensor communication | SPI interface |
| TestStateTransitions | Verify proper state machine transitions | Correct state progression | State machine logic |

## Debugging 
**Debugging Note**: 
In `BmpManager.cpp`, there are several commented `printf` statements used for debugging. Uncomment for debugging help. 
