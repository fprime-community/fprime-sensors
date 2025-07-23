module NmeaGps {
    @ Converter from GPS messages to GPS data
    passive component GpsManager {

        @ Channel for publishing GPS readings
        telemetry Reading: GpsData

        @ Report for malformed message
        event MalformedMessage(message_type: string, successful_fields: U8) severity warning low format "Malformed {} message after {} fields" throttle 5 

        @ Report for invalid message
        event InvalidData(message_type: string) severity warning low format "{} data  marked invalid" throttle 5 

        ###############################################################################
        # Deframer "In" Ports: Mascarades as a deframer to use the FrameAccumulator   #
        ###############################################################################

        @ Port to receive framed data, with optional context
        guarded input port dataIn: Fw.BufferSend

        @ Port for returning ownership of received buffers to deframe
        output port dataReturnOut: Fw.BufferSend

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}