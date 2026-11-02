module Rfm9X {
    @ LoRa spreading factor (RegModemConfig2.SpreadingFactor, datasheet Table 59).
    @ Expressed as chips/symbol base-2 logarithm; SF6 requires the special
    @ implicit-header configuration noted in datasheet section 4.1.1.2.
    enum SpreadingFactor : U8 {
        SF6 = 6
        SF7 = 7
        SF8 = 8
        SF9 = 9
        SF10 = 10
        SF11 = 11
        SF12 = 12
    }

    @ LoRa signal bandwidth (RegModemConfig1.Bw, datasheet section 6.4). Values
    @ are the full 10-entry Bw field range; 250/500 kHz are unsupported on the
    @ low-frequency (169 MHz) band per datasheet, not applicable at 434 MHz.
    enum Bandwidth : U32 {
        BW_7_8_KHZ = 7800
        BW_10_4_KHZ = 10400
        BW_15_6_KHZ = 15600
        BW_20_8_KHZ = 20800
        BW_31_25_KHZ = 31250
        BW_41_7_KHZ = 41700
        BW_62_5_KHZ = 62500
        BW_125_KHZ = 125000
        BW_250_KHZ = 250000
        BW_500_KHZ = 500000
    }

    @ LoRa error coding rate (RegModemConfig1.CodingRate, datasheet Table 60).
    enum CodingRate : U8 {
        CR_4_5 = 1
        CR_4_6 = 2
        CR_4_7 = 3
        CR_4_8 = 4
    }

    @ Nominal RFM9X transmitter output power in dBm, on the PA_BOOST pin
    @ (the Adafruit RFM9X breakout's antenna path per RFM9XDatasheet.md).
    @ PA_BOOST's floor is +2 dBm (datasheet RegPaConfig), so DBM_0 is omitted.
    @ The helper controls RegPaConfig, RegOcp, and RegPaDac (+20 dBm) together.
    enum Rfm9XTxPower : I8 {
        DBM_5 = 5
        DBM_10 = 10
        DBM_13 = 13
        DBM_17 = 17
        DBM_20 = 20
    }

    @ Radio mode associated with a configuration failure, mirroring LoRa.
    enum Rfm9XMode : U8 {
        Transmit
        Receive
    }

    @ Reason a staged downlink packet was not sent.
    enum Rfm9XSendFailure : U8 {
        @ Payload was empty or exceeded the 255-byte RFM9X packet limit
        INVALID_SIZE
        @ SPI error or mode/PacketSent timeout; the radio was recovered to RX
        RADIO_FAULT
        @ Local pending-transmit hold was full (ground station only)
        QUEUE_FULL
    }

    @ Radio bring-up progress, reported as telemetry.
    enum Rfm9XRadioState : U8 {
        @ Waiting for FPP parameters to load
        NOT_STARTED
        @ Driving / settling the optional hardware reset line
        RESETTING
        @ Probing RegVersion over SPI
        DETECT
        @ Writing the modem profile and entering receive
        CONFIGURE
        @ Receiving; downlink packets accepted
        READY
    }

    @ Controls whether the radio may leave receive mode to downlink.
    @ DISABLED: receive-only; downlink dataIn returns Com FAILURE (ComQueue pauses).
    @ ENABLED: normal TX; re-enable emits Com SUCCESS to resume the queue.
    enum TransmitState : U8 {
        ENABLED
        DISABLED
    }

    @ Communication adapter (Svc.Com interface) for an RFM9XHCW radio on SPI.
    passive component Rfm9XManager {
        @ Import the communication interface
        import Svc.Com

        # ----------------------------------------------------------------------
        # Implementation ports
        # ----------------------------------------------------------------------

        @ SPI bus transactions with the radio
        output port spiWriteRead: Drv.SpiWriteRead

        @ Reset requests through a platform-provided GPIO driver
        output port resetGpio: Drv.GpioWrite

        @ Allocation of buffers for received packets
        output port allocate: Fw.BufferGet

        @ Deallocation of buffers returned on dataReturnIn
        output port deallocate: Fw.BufferSend

        @ Rate-group tick: detection, configuration, and receive polling
        sync input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Special ports
        # ----------------------------------------------------------------------

        time get port timeCaller
        telemetry port tlmOut
        event port logOut
        text event port logTextOut
        command reg port CmdReg
        command recv port CmdDisp
        command resp port CmdStatus
        param get port prmGet
        param set port prmSet

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ LoRa spreading factor. Default SF7 matches the project's 434 MHz
        @ operating point. Ground Station must match.
        param SPREADING_FACTOR: SpreadingFactor default SpreadingFactor.SF7 id 0

        @ LoRa signal bandwidth. Default 125 kHz matches the ground-station image.
        param BANDWIDTH: Bandwidth default Bandwidth.BW_125_KHZ id 1

        @ LoRa error coding rate. Default 4/5 matches the ground-station image.
        param CODING_RATE: CodingRate default CodingRate.CR_4_5 id 2

        @ RF carrier frequency in Hz. Default 434.0 MHz matches the project's
        @ stated operating band (datasheet section 6.4, RegFrLsb).
        param FREQUENCY_HZ: U32 default 434000000 id 3

        @ HCW transmitter power configuration, including PA boost when needed.
        param TX_POWER: Rfm9XTxPower default Rfm9XTxPower.DBM_13 id 4

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Enable or disable downlink. Disabled keeps the radio in receive mode.
        sync command TRANSMIT(enabled: TransmitState)

        @ Pulse hardware RST and then run normal detection/configuration.
        sync command RESET

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Event to indicate configuration failure
        event ConfigurationFailed(mode: Rfm9XMode) severity warning high \
            format "Failed to configure RFM9X into mode: {}" throttle 2

        @ Event to indicate send failure
        event SendFailed(reason: Rfm9XSendFailure) severity warning high \
            format "Failed to send RFM9X message: {}" throttle 2

        @ Radio detected, configured, and receiving
        event RadioReady severity activity high \
            format "RFM9X radio configured and ready"

        @ Radio reset and re-initialization has begun
        event ResetInitiated severity activity high \
            format "RFM9X reset initiated"

        @ Event to indicate allocation failure
        event AllocationFailed(allocation_size: FwSizeType) severity warning high \
            format "Failed to allocate buffer of: {} bytes" throttle 2

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Successful RF packet transmissions
        telemetry PacketsTransmitted: U32

        @ RF packets delivered on dataOut
        telemetry PacketsReceived: U32

        @ RX frames dropped for failing hardware CRC (noise / corrupted uplink).
        @ A counter, not an event: corrupted frames are expected on a lossy link
        @ and must not spam WARNING events onto the downlink or the soak gate.
        telemetry RxCrcErrors: U32

        @ RSSI of last received packet (dBm): -137 + RegPktRssiValue (datasheet section 6.4)
        telemetry LastPacketRssi: F32

        @ SNR of last received packet (dB): RegPktSnrValue (two's complement) / 4 (datasheet section 6.4)
        telemetry LastPacketSnr: F32

        @ Radio bring-up state
        telemetry RadioState: Rfm9XRadioState
    }
}
