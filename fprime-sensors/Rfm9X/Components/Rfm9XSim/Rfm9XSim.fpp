module Rfm9X {
    @ Register-level RFM9XHCW simulation exposed over Rfm9XManager's async
    @ SPI contract (request/completion port pair matching Stm32SpiDriver's
    @ spiWriteReadAsync/spiWriteReadAsyncComplete one-for-one, so this
    @ simulation wires to Rfm9XManager with no adapter, same as real hardware).
    passive component Rfm9XSim {

        @ Asynchronous SPI request: the model runs the transaction immediately
        @ (it is plain simulated register logic, no real timing), but the
        @ completion is delivered later from run() rather than inline --
        @ delivering it synchronously here would re-enter Rfm9XManager's own
        @ (non-recursive) lock from the same call stack that is already
        @ holding it while issuing the request.
        guarded input port spiWriteReadAsync: [Drv.AsyncSpiCfg.SpiDriverPorts] Drv.SpiWriteReadRequest

        ###### Port below must be connected: every request is completed through it ######

        @ Invoked when an spiWriteReadAsync transaction completes (see above)
        output port spiWriteReadAsyncComplete: [Drv.AsyncSpiCfg.SpiDriverPorts] Drv.SpiWriteReadCallback

        @ Drains one pending SPI completion (if any) per tick -- see
        @ spiWriteReadAsync's doc comment for why delivery is deferred here.
        sync input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Air interface: tunnels simulated RF packets over a byte stream driver
        # ----------------------------------------------------------------------

        @ Ready signal from the air interface driver (ignored)
        sync input port airReady: Drv.ByteStreamReady

        @ Bytes arriving over the simulated air interface (e.g. from the GDS)
        sync input port airDataIn: Drv.ByteStreamData

        @ Return of buffers received on airDataIn
        output port airDataReturnOut: Fw.BufferSend

        @ Transmitted packet payloads sent out the simulated air interface
        output port airDataOut: Drv.ByteStreamSend

        @ Allocation of buffers for transmitted packets
        output port allocate: Fw.BufferGet

        @ Deallocation of buffers for transmitted packets
        output port deallocate: Fw.BufferSend

        # ----------------------------------------------------------------------
        # Special ports
        # ----------------------------------------------------------------------

        @ Port for requesting the current time
        time get port timeCaller

        @ Event port
        event port logOut

        @ Text event port
        text event port logTextOut

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Failed to allocate a buffer for a transmitted packet
        event SimAllocationFailed() severity warning high format "RFM9X sim buffer allocation failed; dropping transmitted packet" throttle 5
    }
}
