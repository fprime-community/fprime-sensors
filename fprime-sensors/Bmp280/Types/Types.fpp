module Bmp280 {

    @ Oversampling setting for pressure measurement
    enum PressureOversampling : U8 {
        SKIP = 0x00
        OVERSAMPLE_1X = 0x04
        OVERSAMPLE_2X = 0x08
        OVERSAMPLE_4X = 0x0C
        OVERSAMPLE_8X = 0x10
        OVERSAMPLE_16X = 0x14
    }

    @ Oversampling setting for temperature measurement
    enum TemperatureOversampling : U8 {
        SKIP = 0x00
        OVERSAMPLE_1X = 0x20
        OVERSAMPLE_2X = 0x40
        OVERSAMPLE_4X = 0x60
        OVERSAMPLE_8X = 0x80
        OVERSAMPLE_16X = 0xA0
    }

    @ Struct representing Bmp280 sensor data
    struct Bmp280Data {
        @ Pressure in Pascals (Pa)
        pressure: F32
        
        @ Temperature in degrees Celsius (°C)
        temperature: F32
        
        @ Altitude in meters (m)
        altitude: F32
    }
} 