module XBee {
    constant retryCount = 10
    array EnergyDensityType = [16] U8

    active component XBeeManager {

        import Svc.Com

        # ----------------------------------------------------------------------
        # Interacting with a ByteStreamDriver (UART)
        # ----------------------------------------------------------------------

        @ Ready signal when driver is connected
        sync input port drvConnected: Drv.ByteStreamReady

        @ Receive (read) data from the driver
        sync input port drvReceiveIn: Drv.ByteStreamData

        @ Return buffer ownership
        output port drvReceiveReturnOut: Fw.BufferSend

        @ Send (write) data to the driver
        output port drvSendOut: Drv.ByteStreamSend

        # ----------------------------------------------------------------------
        # Implementation ports
        # ----------------------------------------------------------------------

        @ Port carrying 1HZ tick for timeout tracking
        sync input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Special ports
        # ----------------------------------------------------------------------

        @ Time get port
        time get port timeCaller

        @ Command registration port
        command reg port cmdRegOut

        @ Command received port
        command recv port cmdIn

        @ Command response port
        command resp port cmdResponseOut

        @ Text event port
        text event port logTextOut

        @ Event port
        event port logOut

        @ Telemetry port
        telemetry port tlmOut

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Report the radio serial number
        async command ReportNodeIdentifier

        @ Perform energy density scan activity
        async command EnergyDensityScan

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Produces a node-identifier
        event RadioNodeIdentifier(
              identifier: string size 20 @< Radio identifier
        ) \
        severity activity high \
        id 0 \
        format "Radio identification: {}"

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------
        @ Energy density graph
        telemetry EnergyDensity: EnergyDensityType id 0 update always
    }
}
