module Rfm9X {
    module SubtopologyConfig {
        constant BASE_ID = 0xC0000000
    }

    @ Linux platform implementations are intentionally isolated in the
    @ configuration layer. The topology and manager only use F' ports.
    instance spiDriver: Drv.LinuxSpiDriver base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00002000 {
        phase Fpp.ToCpp.Phases.configComponents """
        // The Pi/reference wiring shows intermittent MISO bit-7 sampling errors
        // at 5 MHz after the RFM9X has already accepted the RF CRC. One MHz is
        // comfortably inside the 1 kHz manager budget and is byte-exact in HIL.
        if (not Rfm9X::spiDriver.open(state.rfm9X.device.device, state.rfm9X.device.select,
                                      Drv::SPI_FREQUENCY_1MHZ,
                                      Drv::SPI_MODE_CPOL_LOW_CPHA_LOW)) {
            Fw::Logger::log("[ERROR] RFM9X SPI open failed\\n");
        }
        """
    }

    @ RFM9X reset/power-enable GPIO (active-low NRESET or a power-enable
    @ load switch -- see Rfm9XManager.fpp's resetGpio port doc comment);
    @ pin assignment comes from topology state.
    instance resetGpio: Drv.LinuxGpioDriver base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00005000 {
        phase Fpp.ToCpp.Phases.configComponents """
        if (Rfm9X::resetGpio.open(state.rfm9X.device.resetGpioChip, state.rfm9X.device.resetGpioPin,
                                  Drv::LinuxGpioDriver::GPIO_OUTPUT, Fw::Logic::LOW) != Os::File::OP_OK) {
            Fw::Logger::log("[ERROR] RFM9X reset GPIO open failed\\n");
        }
        """
    }

    instance rfm9XSim: Rfm9X.Rfm9XSim base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00003000
}
