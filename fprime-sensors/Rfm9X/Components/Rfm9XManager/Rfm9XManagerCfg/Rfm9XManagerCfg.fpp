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

        @ Time to hold resetGpio asserted (LOW) before releasing it, in
        @ milliseconds. Default covers the SX127x NRESET pulse requirement
        @ (>=100us, datasheet section 7.2.2) with margin. A deployment that
        @ wires resetGpio to a power-enable/load-switch GPIO instead of a
        @ dedicated NRESET line (same LOW=off/HIGH=on drive sequence either
        @ way) should override this to whatever that switch's own hold
        @ requirement is, if any.
        constant RESET_HOLD_MS = 1

        @ Time to wait after releasing resetGpio before assuming the radio is
        @ ready to talk to, in milliseconds. Default covers the SX127x's own
        @ post-reset settle time (datasheet section 7.2.2). A power-enable
        @ deployment should override this to cover its own worst-case
        @ power-on time instead -- load-switch output capacitor charge time,
        @ any onboard regulator rise time, and the radio's crystal
        @ oscillator startup can all be meaningfully longer than a simple
        @ reset release.
        constant RESET_SETTLE_MS = 6

        @ Maximum LoRa packet payload, in bytes. The SX127x's RegPayloadLength/
        @ RX_NB_BYTES registers are one byte wide, so this can never exceed
        @ 255 regardless of the override. Sizes every fixed payload/scratch
        @ buffer in Rfm9XManager and Rfm9XSim (m_mosi/m_miso/m_rxPayload/
        @ m_cmdPayload each scale with this) -- a project that only ever
        @ sends short packets can shrink this to cut RAM; 255 keeps full
        @ single-packet headroom.
        constant MAX_PAYLOAD_LENGTH = 255

        @ Depth of the local ground-station pending-TX hold queue (used only
        @ when comStatusOut is unwired, i.e. no ComQueue retry path -- see
        @ Rfm9XManager::usesLocalTxHold()). Each slot costs one Fw::Buffer +
        @ one ComCfg::FrameContext (roughly 168 bytes on a 32-bit target),
        @ the single largest RAM cost in this component at the default depth
        @ of 8. A flight deployment with a real ComQueue wired up never
        @ fills this queue and can set it to 0 or 1.
        constant PENDING_TX_DEPTH = 8
    }
}
