module XBee {

    module BuffMgr {
        constant gpsAccumulatorSize    = 1024      
        constant gpsBufferSize         = 1024      
        constant gpsAccumulatorCount   = 1      
        constant gpsBufferCount        = 4
        constant buffMgrId           = 0xA000
    }

    module Components {
        constant QUEUE_SIZE = 10
        constant STACK_SIZE = 64 * 1024
    }


    constant BASE_ID = 0xF0000000

    # XBee Radio Integration
    instance comMgr: XBee.XBeeManager base id XBee.BASE_ID + 0x1000 \
        queue size Components.QUEUE_SIZE \
        stack size Components.STACK_SIZE \
        priority 140


    instance comDriver: Drv.LinuxUartDriver base id XBee.BASE_ID + 0x2000 {
        phase Fpp.ToCpp.Phases.configComponents """

        if (state.xbee.device != nullptr && state.xbee.baudRate != 0) {
            // Uplink is configured for receive so a socket task is started
            if (comDriver.open(state.xbee.device, static_cast<Drv::LinuxUartDriver::UartBaudRate>(state.xbee.baudRate),
                            Drv::LinuxUartDriver::NO_FLOW, Drv::LinuxUartDriver::PARITY_NONE, 1024)) {
                comDriver.start(100, XBee::Components::STACK_SIZE);
            } else {
                Fw::Logger::log("[ERROR] Failed to open UART device %s at baud rate %" PRIu32 "\n", state.xbee.device, state.xbee.baudRate);
            }
        }
        """
    }
}
