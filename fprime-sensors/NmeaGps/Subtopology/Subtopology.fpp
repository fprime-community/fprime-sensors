module NmeaGps {
    @ Manager overseeing the GPS system
    instance gpsManager: NmeaGps.GpsManager base id NmeaGps.SubtopologyConfig.BASE_ID + 0x00001000

    @ Adapter for GPS to received data from the FrameAccumulator
    instance gpsAdapter: FprimeSensors.AccumulatorAdapter base id NmeaGps.SubtopologyConfig.BASE_ID + 0x00002000

    @ FrameAccumulator collecting NMEA messages
    instance frameAccumulator: Svc.FrameAccumulator base id NmeaGps.SubtopologyConfig.BASE_ID + 0x00003000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        NmeaGps::NmeaDetector nmeaDetector;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        NmeaGps::frameAccumulator.configure(
            ConfigObjects::NmeaGps_frameAccumulator::nmeaDetector, 1, mallocator, 2048);
        """
    }

    @ Adapter from driver to FrameAccumulator
    instance driverInterface: FprimeSensors.AccumulatorAdapter base id NmeaGps.SubtopologyConfig.BASE_ID + 0x00004000


    topology Subtopology {
        instance gpsManager
        instance gpsAdapter
        instance frameAccumulator
        instance driverInterface
        instance driver
        instance bufferManager

        connections NmeaGps {
            # Connect the GPS Manager to the adapter for the frame accumulator
            gpsAdapter.bufferLikeOut -> gpsManager.dataIn
            gpsManager.dataReturnOut -> gpsAdapter.bufferLikeIn

            # Connect the FrameAccumulator to the GPS adapter and buffer manager
            frameAccumulator.dataOut -> gpsAdapter.commLikeIn
            frameAccumulator.bufferAllocate -> bufferManager.bufferGetCallee
            frameAccumulator.bufferDeallocate -> bufferManager.bufferSendIn
            gpsAdapter.commLikeOut -> frameAccumulator.dataReturnIn

            # Connect the driver adapter to the accumulator
            driverInterface.commLikeOut -> frameAccumulator.dataIn
            frameAccumulator.dataReturnOut -> driverInterface.commLikeIn

            # Connect the driver to the driver interface
            driver.$recv -> driverInterface.byteStreamLikeIn
            driverInterface.bufferLikeOut -> driver.recvReturnIn
        }
    }
}