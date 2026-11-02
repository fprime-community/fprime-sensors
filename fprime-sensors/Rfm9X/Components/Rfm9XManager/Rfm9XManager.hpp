// ======================================================================
// \title  Rfm9XManager.hpp
// \brief  hpp file for Rfm9XManager component implementation class
// ======================================================================

#ifndef Rfm9X_Rfm9XManager_HPP
#define Rfm9X_Rfm9XManager_HPP

#include "Os/Mutex.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManagerComponentAc.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XRadio.hpp"

namespace Rfm9X {

class Rfm9XManager final : public Rfm9XManagerComponentBase {
  public:
    //! Conservative bound on ticks spent waiting for TxDone after requesting
    //! TX mode. LoRa time-on-air grows sharply with spreading factor and
    //! shrinks with bandwidth (datasheet section 4.1.1, Table showing nominal
    //! bitrate vs. SF/BW/CR); this fixed bound comfortably covers the
    //! project's SF7/BW125kHz default for a 255-byte payload and is a
    //! conservative placeholder pending HIL tuning for other SF/BW combinations.
    static constexpr U32 TX_DONE_TIMEOUT_TICKS = 5000;
    //! After a completed uplink packet, defer downlink this many 1 kHz ticks.
    //! This is an RX-priority lease for the half-duplex link: each packet in a
    //! file burst refreshes it, so flight cannot begin a queued downlink in the
    //! gap before the next ground chunk. Once the burst ends, TX resumes
    //! automatically and drains queued TM/events. Carried over from the FSK
    //! baseline pending HIL retuning for LoRa time-on-air.
    //! Ground-station local pending-TX hold ignores this timer (channelBusy only).
    static constexpr U32 RX_TX_HOLDOFF_TICKS = 500;
    //! Quiet time between consecutive local transmits (1 kHz ticks). Carried
    //! over from the FSK baseline pending HIL retuning for LoRa time-on-air.
    static constexpr U32 TX_TX_HOLDOFF_TICKS = 500;
    //! Upper bound on consecutive ticks a pending downlink may remain blocked
    //! by the RX-priority lease. Periodic uplink (e.g. GDS keepalives every
    //! 0.5 s) re-arms RX_TX_HOLDOFF_TICKS on every received packet, which
    //! would otherwise starve telemetry indefinitely. When the cap expires,
    //! the lease is dropped so one downlink window opens; an RX actively
    //! clocking in (channelBusy) is still honored.
    static constexpr U32 RX_HOLDOFF_STARVATION_TICKS = 2 * RX_TX_HOLDOFF_TICKS;

    //! Both FIFO base addresses are set to the start of the 256-byte LoRa
    //! FIFO (datasheet section 4.1.2) rather than the reset defaults
    //! (FifoTxBaseAddr=0x80, FifoRxBaseAddr=0x00), which would only leave
    //! 128 bytes for TX. This half-duplex single-antenna link never has TX
    //! and RX data resident at once, so both directions may use the full
    //! 256-byte buffer from address 0x00.
    static constexpr U8 FIFO_BASE_ADDR = 0x00;

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Rfm9XManager object
    Rfm9XManager(const char* const compName  //!< The component name
    );

    //! Destroy Rfm9XManager object
    ~Rfm9XManager();

  private:
    //! Software lifecycle, advanced one bounded step per run tick:
    //! RESET_ASSERT → RESET_HOLD → RESET_SETTLE → DETECT → CONFIGURE → READY.
    //! RESET / parameter updates re-enter earlier states; run retries failures.
    enum RadioState {
        RESET_ASSERT,  //!< Drive the optional RST line HIGH (skip if unwired)
        RESET_HOLD,    //!< Hold RST HIGH for RESET_HOLD_TICKS, then drive LOW
        RESET_SETTLE,  //!< Wait RESET_SETTLE_TICKS after releasing RST
        DETECT,        //!< Looking for RegVersion on SPI
        CONFIGURE,     //!< Writing profile + entering RX
        READY          //!< Polling RX / accepting TX
    };

    //! RST must be held HIGH at least 100 us (datasheet section 7.2.2); one
    //! 1 kHz tick provides 1 ms with margin.
    static constexpr U32 RESET_HOLD_TICKS = 1;
    //! The radio needs 5 ms after RST release before use (datasheet 7.2.2).
    static constexpr U32 RESET_SETTLE_TICKS = 6;

    //! Non-blocking downlink sequence. LoRa's FIFO is filled once in Standby
    //! (datasheet section 4.1.5: "The LoRa FIFO can only be filled in
    //! Stand-by mode") and the full 255-byte max payload fits the 256-byte
    //! FIFO in one shot, so unlike FSK there is no in-flight top-up state.
    enum TxState {
        TX_IDLE,
        TX_REQUEST_STANDBY,
        TX_SET_FIFO_PTR,
        TX_WRITE_PAYLOAD,
        TX_REQUEST_TX,
        TX_WAIT_TXDONE,
        TX_REQUEST_RX,
        TX_ABORT_REQUEST_RX
    };

    enum TxProgress {
        TX_IN_PROGRESS,
        TX_SUCCEEDED,
        TX_FAILED
    };

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn: transmit a frame over the radio
    void dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn: buffer ownership returned
    void dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) override;

    //! Handler implementation for run: receive polling (or init retry if not READY)
    void run_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // Command handler implementations
    // ----------------------------------------------------------------------

    //! Handler for the TRANSMIT command: enable/disable downlink
    void TRANSMIT_cmdHandler(FwOpcodeType opCode,     //!< The command opcode
                             U32 cmdSeq,              //!< The command sequence number
                             const Rfm9X::TransmitState& enabled  //!< Desired transmit state
                             ) override;

    //! Handler for RESET: pulse hardware RST and reinitialize the radio
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The command opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    //! Schedule CONFIGURE on the next run tick after a parameter set
    void parameterUpdated(FwPrmIdType id  //!< The parameter ID
                          ) override;

    //! Mark parameters available; bring-up itself is advanced by run() ticks
    void parametersLoaded() override;

    // ----------------------------------------------------------------------
    // Helper functions: radio state management (Rfm9XManager.cpp)
    // ----------------------------------------------------------------------

    //! Advance detection/configuration toward READY
    void initializeRadio();

    //! Poll for and deliver a received packet (one IRQ_FLAGS read per tick)
    void pollReceive();

    //! Read IRQ_FLAGS and, on RxDone, drain and stage a received packet
    //! (or drop+count a CRC failure). No-op when neither flag is set.
    void tryDeliverReceivedPacket();

    //! Allocate and emit a received packet on dataOut (outside m_lock)
    void deliverReceivedPacket(const U8* data, FwSizeType size);

    //! Enter the tick-driven hardware reset sequence (or DETECT when the
    //! optional RST line is unwired).
    void beginResetSequence();

    //! Map the internal lifecycle state to telemetry and write the channel.
    void reportRadioState();

    //! Stage one native RF packet for scheduler-driven transmission.
    bool startTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context);

    //! True when this deployment has no ComQueue retry path (comStatusOut unwired).
    //! Ground-station topologies use a local pending-TX hold instead of dropping.
    bool usesLocalTxHold() const;

    //! Enqueue one uplink buffer for a later run() tick. Returns false if full.
    bool enqueuePendingTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context);

    //! If idle and clear, start the oldest pending uplink packet.
    //! Returns true when dropBuffer/dropContext must be returned after unlock.
    bool tryStartPendingTransmit(Fw::Buffer& dropBuffer, ComCfg::FrameContext& dropContext);

    //! Advance the scheduler-driven transmit sequence by one bounded step.
    TxProgress advanceTransmit();

    //! Enter abort recovery after a transmit SPI/timeout failure.
    void abortTransmit();

    // ----------------------------------------------------------------------
    // Helper functions: RFM9X register access (Rfm9XHelpers.cpp)
    // ----------------------------------------------------------------------

    //! \brief Read a single radio register
    Drv::SpiStatus readRegister(U8 address,  //!< Register address
                                U8& value    //!< [out] Register value
    );

    //! \brief Write a single radio register
    Drv::SpiStatus writeRegister(U8 address,  //!< Register address
                                 U8 value     //!< Value to write
    );

    //! \brief Detect the radio by checking the version register
    bool detectRadio();

    //! \brief Apply the LoRa modem configuration to the radio
    bool configureRadio();

    //! \brief Enter LoRa mode (RegOpMode.LongRangeMode), per the mandatory
    //! SLEEP-mode-only sequence in datasheet section 6.4.
    bool enterLoRaMode();

    //! Request an operating mode, preserving LongRangeMode (RegOpMode bits 2-0)
    bool requestMode(U8 mode  //!< Mode field value (Rfm9X::Mode)
    );

    //! \brief True when a reception is in progress (defer TX via Com FAILURE)
    bool channelBusy();

    //! \brief True when downlink should wait (RX in progress or post-RX holdoff)
    bool downlinkBlocked();

    //! Emit comStatusOut when connected
    void emitComStatus(Fw::Success status);

    //! If a prior FAILURE left ComQueue waiting, clear resume flag once ready.
    //! Returns true when the caller should emit SUCCESS after releasing m_lock.
    bool maybeResumeComStatus();

    //! \brief Burst-write a block of payload bytes into the FIFO
    bool writeFifo(const U8* data,  //!< Bytes to load
                   FwSizeType size  //!< Byte count
    );

    //! Burst-read bytes from the FIFO.
    bool readFifo(U8* data,  //!< Destination bytes
                  FwSizeType size //!< Byte count
    );

    //! \brief Read RegPktRssiValue/RegPktSnrValue and update telemetry
    void updateRssiSnr();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Serializes radio access between dataIn (com queue thread) and run
    //! (rate group thread): both drive multi-transaction SPI sequences and
    //! share the SPI scratch buffers
    Os::Mutex m_lock;

    //! True while a downlink buffer is owned by the scheduler-driven TX state machine.
    bool m_txActive;
    TxState m_txState;                //!< Current step of the staged TX sequence
    Fw::Buffer m_txBuffer;            //!< Downlink buffer owned by the TX machine
    ComCfg::FrameContext m_txContext; //!< Frame context returned with m_txBuffer
    U32 m_txWaitTicks;                //!< Ticks spent waiting for TxDone

    //! Local hold used when comStatusOut is unwired (no ComQueue). Mirrors the
    //! Arduino RadioHead ground-station usbHold/retry behavior for half-duplex.
    //! Sized to the GS frame-bin pool (8×272) so a short GDS burst cannot
    //! overflow and drop mid-file CFDP DATA packets (flight ignores this hold).
    static constexpr U32 PENDING_TX_DEPTH = 8;
    Fw::Buffer m_pendingTxBuffers[PENDING_TX_DEPTH];
    ComCfg::FrameContext m_pendingTxContexts[PENDING_TX_DEPTH];
    U32 m_pendingTxCount;

    //! Completed packet length staged for post-unlock delivery (0 = none)
    FwSizeType m_rxDeliverSize;
    U8 m_rxPayload[MAX_PACKET_PAYLOAD];
    static_assert(sizeof(m_rxPayload) == MAX_PACKET_PAYLOAD,
                  "RX delivery bounds checks assume m_rxPayload holds MAX_PACKET_PAYLOAD bytes");
    //! Remaining 1 kHz ticks to defer flight downlink after a completed RX
    U32 m_rxTxHoldoffTicks;
    //! Remaining 1 kHz ticks to defer the next local TX after a completed TX
    U32 m_txTxHoldoffTicks;
    //! Consecutive ticks a pending downlink has been blocked by the RX lease
    U32 m_txStarvedTicks;

    RadioState m_state;        //!< Radio management state
    U32 m_resetTicks;          //!< Ticks elapsed in the current reset phase
    bool m_configured;         //!< FPP parameters have been loaded
    U32 m_packetsTransmitted;  //!< Count of transmitted packets
    U32 m_packetsReceived;     //!< Count of received packets
    U32 m_rxCrcErrors;         //!< Count of RX frames dropped for failing CRC
    TransmitState m_transmitEnabled;  //!< Whether downlink transmit is permitted
    bool m_comStatusAnnounced;          //!< Initial link-ready status has been sent
    //! Set after reporting FAILURE so run can emit SUCCESS when downlink is
    //! allowed again (holdoff expired, radio ready).
    bool m_comResumeNeeded;

    //! Scratch buffers for SPI transactions (address byte + up to one full
    //! 255-byte LoRa FIFO payload)
    U8 m_mosi[MAX_PACKET_PAYLOAD + 2];
    U8 m_miso[MAX_PACKET_PAYLOAD + 2];
};

}  // namespace Rfm9X

#endif  // Rfm9X_Rfm9XManager_HPP