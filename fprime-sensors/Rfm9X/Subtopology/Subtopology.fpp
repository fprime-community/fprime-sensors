module Rfm9X {

    @ Manager overseeing the RFM9X radio as a com interface
    @ Configuration is loaded solely through Rfm9XManager's FPP parameters.
    instance rfm9XManager: Rfm9X.Rfm9XManager base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00001000

    @ RFM9X subtopology. Its named hardware instances are configuration-layer
    @ implementations; the manager communicates with them only through F' ports.
    topology Subtopology {
        instance rfm9XManager
        instance spiDriver
        instance resetGpio

        connections Rfm9X {
            rfm9XManager.spiWriteRead -> spiDriver.SpiWriteRead
            rfm9XManager.resetGpio -> resetGpio.gpioWrite
        }
    }

    @ Simulation subtopology: RFM9X manager attached to a register-level simulation
    topology SimSubtopology {
        instance rfm9XManager
        instance rfm9XSim

        connections Rfm9XSim {
            rfm9XManager.spiWriteRead -> rfm9XSim.SpiWriteRead
        }
    }
}
