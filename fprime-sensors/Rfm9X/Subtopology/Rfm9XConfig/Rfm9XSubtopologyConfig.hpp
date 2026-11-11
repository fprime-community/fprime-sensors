// ======================================================================
// \title  Rfm9XSubtopologyConfig.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
//
// ======================================================================
#ifndef Rfm9X_Rfm9XSubtopologyConfig_hpp
#define Rfm9X_Rfm9XSubtopologyConfig_hpp

struct Rfm9XDevice {
    // SPI bus number and chip select for the radio. Defaults match the
    // reference Raspberry Pi deployment (SPI0, CE1).
    int device = 0;  // SPI bus number (e.g., 0 for SPI bus 0)
    int select = 1;  // SPI chip select pin (e.g., 1 for CS1)
    // Optional reset/power-enable line (either a dedicated active-low NRESET
    // or a power-enable GPIO work; see Rfm9XManager.fpp's resetGpio port doc
    // comment). Defaults match the reference wiring: BCM GPIO26 (physical
    // pin 37).
    const char* resetGpioChip = "/dev/gpiochip0";
    int resetGpioPin = 26;
};

#endif  // Rfm9X_Rfm9XSubtopologyConfig_hpp
