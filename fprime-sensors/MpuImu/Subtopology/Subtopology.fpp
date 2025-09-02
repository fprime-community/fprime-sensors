module MpuImu {
    @ Manager overseeing the IMU
    instance imuManager: MpuImu.ImuManager base id MpuImu.BASE_ID + 0x00001000 \
        queue size MpuImu.QueueSizes.imuManager

    topology Subtopology {
        instance imuManager
        instance imuDriver

        connections MpuImu {
            imuManager.busWriteRead -> imuDriver.writeRead
            imuManager.busWrite -> imuDriver.write
        }
    }
}
