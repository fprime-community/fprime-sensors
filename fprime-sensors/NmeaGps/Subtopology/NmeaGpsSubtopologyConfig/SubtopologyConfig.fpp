module NmeaGps {
    module SubtopologyConfig {
        constant BASE_ID = 0xF0000000
    }

    module BuffMgr {
        constant gpsAccumulatorSize    = 1024      
        constant gpsBufferSize         = 1024      
        constant gpsAccumulatorCount   = 1      
        constant gpsBufferCount        = 4
        constant gpsBufferId           = 0xF000
    }

    instance driver: Drv.LinuxUartDriver base id SubtopologyConfig.BASE_ID + 0x111000 \
    {
        phase Fpp.ToCpp.Phases.configComponents """
            NmeaGps::driver.open(state.gps.device,
                         static_cast<Drv::LinuxUartDriver::UartBaudRate>(state.gps.baud),
                         Drv::LinuxUartDriver::UartFlowControl::NO_FLOW,
                         Drv::LinuxUartDriver::UartParity::PARITY_NONE,
                         1024);
        """

        phase Fpp.ToCpp.Phases.startTasks """
            NmeaGps::driver.start(90);
        """

        phase Fpp.ToCpp.Phases.stopTasks """
            NmeaGps::driver.quitReadThread();
            NmeaGps::driver.join();
        """
    }

    # The default driver (LinuxUartDriver) requires buffer management and as such the buffer management
    # instance must be configured to provide for the driver
    instance bufferManager: Svc.BufferManager base id SubtopologyConfig.BASE_ID + 0x110000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::NmeaGps_bufferManager::bins, 0, sizeof(ConfigObjects::NmeaGps_bufferManager::bins));
        ConfigObjects::NmeaGps_bufferManager::bins.bins[0].bufferSize = NmeaGps::BuffMgr::gpsAccumulatorSize;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[0].numBuffers = NmeaGps::BuffMgr::gpsAccumulatorCount;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[1].bufferSize = NmeaGps::BuffMgr::gpsBufferSize;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[1].numBuffers = NmeaGps::BuffMgr::gpsBufferCount;
        NmeaGps::bufferManager.setup(
            NmeaGps::BuffMgr::gpsBufferId,
            0,
            mallocator,
            ConfigObjects::NmeaGps_bufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        NmeaGps::bufferManager.cleanup();
        """
    }
}
