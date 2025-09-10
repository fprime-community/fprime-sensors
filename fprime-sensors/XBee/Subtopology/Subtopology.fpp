module XBee {

    # XBee Radio Integration
    instance comMgr: XBee.XBeeManager base id XBee.BASE_ID + 0x1000 \
        queue size Components.QUEUE_SIZE \
        stack size Components.STACK_SIZE \
        priority 140


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
        }
    }
}
