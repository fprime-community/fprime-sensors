module Rfm9X {
    module ManagerCfg {
        @ Bound on time spent waiting for TxDone after requesting TX mode, in
        @ milliseconds. LoRa time-on-air grows sharply with spreading factor
        @ and shrinks with bandwidth (datasheet section 4.1.1); this default
        @ comfortably covers the project's SF7/BW125kHz profile for a
        @ 255-byte payload.
        constant TX_DONE_TIMEOUT_MS = 5000

        @ After a completed uplink packet, defer downlink this many
        @ milliseconds -- an RX-priority lease for the half-duplex link.
        constant RX_TX_HOLDOFF_MS = 500

        @ Quiet time between consecutive local transmits, in milliseconds.
        constant TX_TX_HOLDOFF_MS = 500

        @ Upper bound on time a pending downlink may remain blocked by the
        @ RX-priority lease, in milliseconds.
        constant RX_HOLDOFF_STARVATION_MS = 2 * RX_TX_HOLDOFF_MS
    }
}
