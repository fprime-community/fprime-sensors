module Bmp280 {
    @ Manager overseeing the BMP280
    instance bmpManager: Bmp280.BmpManager base id Bmp280.SubtopologyConfig.BASE_ID + 0x00001000

    topology Subtopology {
        instance bmpManager
        instance bmpDriver

        connections Bmp280 {
            bmpManager.spiReadWrite -> bmpDriver.SpiReadWrite
        }
    }
} 