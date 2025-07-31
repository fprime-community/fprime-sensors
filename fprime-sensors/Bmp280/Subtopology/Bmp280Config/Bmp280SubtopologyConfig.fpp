module Bmp280 {
    module SubtopologyConfig {
        constant BASE_ID = 0xD0000000
    }

    instance bmpDriver: Drv.LinuxSpiDriver base id Bmp280.SubtopologyConfig.BASE_ID + 0x00002000 {
        phase Fpp.ToCpp.Phases.configComponents """
        if (not Bmp280::bmpDriver.open(state.bmp.device.device, state.bmp.device.select, Drv::SPI_FREQUENCY_5MHZ)) {
            Fw::Logger::log("[ERROR] BMP280 SPI open failed\\n");
        }
        else {
            Fw::Logger::log("[INFO] BMP280 SPI open successful\\n");
        }
        """
    }
} 