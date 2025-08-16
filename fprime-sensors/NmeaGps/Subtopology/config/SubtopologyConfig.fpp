module NmeaGps {
    module SubtopologyConfig {
        constant BASE_ID = 0xF0000000
    }

    module BuffMgr {
        constant gpsAccumulatorSize    = 1024      
        constant gpsBufferSize         = 1024      
        constant gpsAccumulatorCount   = 1      
        constant gpsBufferCount        = 4       
    }

    instance bufferManager: Svc.BufferManager base id SubtopologyConfig.BASE_ID + 0x110000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::ComCcsds_commsBufferManager::bins, 0, sizeof(ConfigObjects::ComCcsds_commsBufferManager::bins));
        ConfigObjects::NmeaGps_bufferManager::bins.bins[0].bufferSize = NmeaGps::BuffMgr::gpsAccumulatorSize;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[0].numBuffers = NmeaGps::BuffMgr::gpsAccumulatorCount;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[1].bufferSize = NmeaGps::BuffMgr::gpsBufferSize;
        ConfigObjects::NmeaGps_bufferManager::bins.bins[1].numBuffers = NmeaGps::BuffMgr::gpsBufferCount;
        NmeaGps::bufferManager.setup(
            NmeaGps::BuffMgr::commsBuffMgrId,
            0,
            NmeaGps::Allocation::memAllocator,
            NmeaGps::NmeaGps_bufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        NmeaGps::bufferManager.cleanup();
        """
    }

    instance driver: Drv.LinuxUartDriver base id SubtopologyConfig.BASE_ID + 0x111000
}
