// ======================================================================
// \title  Bmp280SubtopologyConfig.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
//
// ======================================================================
#ifndef Bmp280_Bmp280SubtopologyConfig_hpp
#define Bmp280_Bmp280SubtopologyConfig_hpp

struct BmpDevice {
    int device;  // SPI bus number (e.g., 0 for SPI bus 0)
    int select;  // SPI chip select pin (e.g., 0 for CS0)
};

#endif // Bmp280_Bmp280SubtopologyConfig_hpp 