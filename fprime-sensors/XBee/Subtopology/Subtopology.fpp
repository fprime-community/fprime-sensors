module XBee {



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
