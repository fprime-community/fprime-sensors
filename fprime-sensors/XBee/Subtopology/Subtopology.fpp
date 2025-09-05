module XBee {

    instance bufferManager: Svc.BufferManager base id XBee.BASE_ID + 0x03000 {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::XBee_bufferManager::bins, 0, sizeof(ConfigObjects::XBee_bufferManager::bins));
        ConfigObjects::XBee_bufferManager::bins.bins[0].bufferSize = 2000;
        ConfigObjects::XBee_bufferManager::bins.bins[0].numBuffers = 5;
        XBee::bufferManager.setup(
            XBee::BuffMgr::buffMgrId,
            0,
            XBee::Allocation::memAllocator,
            ConfigObjects::XBee_bufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        XBee::bufferManager.cleanup();
        """
    }

    topology Subtopology {
        instance comMgr
        instance comDriver
        instance bufferManager

        connections XBee {
            # ComDriver <-> ComAdapter (Uplink)
            comDriver.$recv                     -> comMgr.drvReceiveIn
            comMgr.drvReceiveReturnOut -> comDriver.recvReturnIn

            # ComAdapter <-> ComDriver (Downlink)
            comMgr.drvSendOut      -> comDriver.$send
            comDriver.ready         -> comMgr.drvConnected

            # Buffer allocations
            comDriver.allocate      -> bufferManager.bufferGetCallee
            comDriver.deallocate    -> bufferManager.bufferSendIn
            comMgr.allocate      -> bufferManager.bufferGetCallee
            comMgr.deallocate    -> bufferManager.bufferSendIn

        }
    }
}
