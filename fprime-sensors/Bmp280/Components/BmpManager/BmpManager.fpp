module Bmp280 {
    @ Component emitting telemetry read from a Bmp280
    passive component BmpManager {

        @ Port for SPI bus communication
        output port spiReadWrite: Drv.SpiReadWrite

        @ Scheduling port for reading from BMP280 and writing to telemetry
        sync input port run: Svc.Sched

        @ Telemetry channel for BMP280 data
        telemetry Reading: Bmp280Data

        event PressureOversamplingUpdated(
            newOversampling: PressureOversampling
        ) severity activity high format "Pressure oversampling updated to {}"

        event TemperatureOversamplingUpdated(
            newOversampling: TemperatureOversampling
        ) severity activity high format "Temperature oversampling updated to {}"

        event DeviceFailure() severity warning high format "BMP280 Device failure" throttle 5

        event ChipIdCheckFailure() severity warning high format "BMP280 Chip ID check failure" throttle 5

        event CalibrationFailure() severity warning high format "BMP280 Calibration Read failure" throttle 5

        event DeviceConfigureFailure() severity warning high format "BMP280 Configure failure" throttle 5 

        event MeasurementTriggerFailure() severity warning high format "BMP280 Measurement Trigger failure" throttle 5

        event DeviceReadFailure() severity warning high format "BMP280 Device Measurement Read failure" throttle 5

        @ Parameter for setting the pressure oversampling
        param PRESSURE_OVERSAMPLING: PressureOversampling default PressureOversampling.OVERSAMPLE_1X

        @ Parameter for setting the temperature oversampling
        param TEMPERATURE_OVERSAMPLING: TemperatureOversampling default TemperatureOversampling.OVERSAMPLE_1X

        @ Parameter for setting the sea-level pressure for altitude calculation (Pa)
        param SEA_LEVEL_PRESSURE: F32 default 101325.0

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Command receive port
        command recv port CmdDisp

        @ Command registration port
        command reg port CmdReg

        @ Command response port
        command resp port CmdStatus

        @ Event port
        event port Log

        @ Text event port
        text event port LogText

        @ Parameter get port
        param get port PrmGet

        @ Parameter set port
        param set port PrmSet

    }
} 