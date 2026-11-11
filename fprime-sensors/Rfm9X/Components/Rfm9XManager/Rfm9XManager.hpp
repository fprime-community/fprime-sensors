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

    //! Convert Rfm9X.ManagerCfg's millisecond-based timing bounds (TX_DONE_TIMEOUT_MS,
    //! RX_TX_HOLDOFF_MS, TX_TX_HOLDOFF_MS, RX_HOLDOFF_STARVATION_MS, RESET_HOLD_MS,
    //! RESET_SETTLE_MS) into tick counts for the tick period this instance is
    //! actually run at. Must be called once from configureTopology(), before
    //! run() is ever invoked -- mirrors MpuImu::ImuManager::configure()'s
    //! shape. Ticks are rounded up so a configured millisecond bound is
    //! never under-satisfied by a coarser tick period.
    //! \param tickPeriodMs milliseconds between consecutive run() ticks (e.g. 10 for
    //!        a 100 Hz rate group)
    void configure(U32 tickPeriodMs);

    //! Test-only accessors for the tick-count bounds configure()/the constructor
    //! compute from Rfm9X.ManagerCfg -- let Rfm9XManagerTester assert against the
    //! actual configured values instead of a removed compile-time constant.
    U32 getTxDoneTimeoutTicks() const { return this->m_txDoneTimeoutTicks; }
    U32 getRxTxHoldoffBoundTicks() const { return this->m_rxTxHoldoffBoundTicks; }
    U32 getTxTxHoldoffBoundTicks() const { return this->m_txTxHoldoffBoundTicks; }
    U32 getRxHoldoffStarvationTicks() const { return this->m_rxHoldoffStarvationTicks; }
    U32 getResetHoldTicks() const { return this->m_resetHoldTicks; }
    U32 getResetSettleTicks() const { return this->m_resetSettleTicks; }

  private:
    //! Software lifecycle, advanced one bounded step per run tick:
    //! RESET_ASSERT → RESET_HOLD → RESET_SETTLE → DETECT → CONFIGURE → READY.
    //! RESET / parameter updates re-enter earlier states; run retries failures.
    //!
    //! resetGpio's drive sequence (LOW then HIGH) is deliberately generic
    //! enough to serve either of two electrically-identical use cases: a
    //! dedicated NRESET line (active-low, SX127x datasheet 7.2.2 -- LOW
    //! asserts reset, HIGH releases it) or a power-enable/load-switch GPIO
    //! (LOW cuts power, HIGH restores it). Which one a given deployment
    //! wires up is purely a topology/hardware choice; this component treats
    //! both identically and only needs the hold/settle durations tuned per
    //! Rfm9X.ManagerCfg's RESET_HOLD_MS/RESET_SETTLE_MS (a power-enable
    //! deployment typically needs a longer RESET_SETTLE_MS to cover the
    //! load switch's own output-capacitor charge time, any onboard
    //! regulator rise time, and crystal oscillator startup, instead of just
    //! the radio's own reset-release settle time).
    enum RadioState {
        RESET_ASSERT,  //!< Drive the optional resetGpio LOW (skip if unwired)
        RESET_HOLD,    //!< Hold resetGpio LOW for m_resetHoldTicks, then drive HIGH to release
        RESET_SETTLE,  //!< Wait m_resetSettleTicks after releasing resetGpio
        DETECT,        //!< Looking for RegVersion on SPI
        CONFIGURE,     //!< Writing profile + entering RX
        READY          //!< Polling RX / accepting TX
    };

    //! Non-blocking downlink sequence. LoRa's FIFO is filled once in Standby
    //! (datasheet section 4.1.5: "The LoRa FIFO can only be filled in
    //! Stand-by mode") and the full 255-byte max payload fits the 256-byte
    //! FIFO in one shot, so unlike FSK there is no in-flight top-up state.
    //! Each state issues exactly one SPI op per entry, resuming on the next
    //! state only once spiWriteReadComplete_handler() reports success for
    //! the op that state issued -- states that used to issue 2-3 writes in
    //! one synchronous call are split into one state per write.
    enum TxState {
        TX_IDLE,
        TX_REQUEST_STANDBY,
        TX_SET_FIFO_PTR,
        TX_SET_PAYLOAD_LEN,
        TX_SET_DIO_TXDONE,
        TX_WRITE_PAYLOAD,
        TX_REQUEST_TX,
        TX_WAIT_TXDONE,
        TX_CLEAR_TXDONE_IRQ,
        TX_SET_DIO_RXDONE,
        TX_REQUEST_RX,
        TX_ABORT_CLEAR_IRQ,
        TX_ABORT_SET_DIO_RXDONE,
        TX_ABORT_REQUEST_RX
    };

    enum TxProgress {
        TX_IN_PROGRESS,
        TX_SUCCEEDED,
        TX_FAILED
    };

    //! Receive-side sequence, advanced one SPI op per spiWriteReadComplete
    //! callback. Mirrors TxState's one-op-per-step shape; replaces the old
    //! synchronous tryDeliverReceivedPacket() (7 sequential SPI ops in one
    //! call) now that SPI is async.
    enum RxState {
        RX_IDLE,            //!< No RX op in flight; run() may start RX_CHECK_BUSY
        RX_CHECK_BUSY,      //!< Reading MODEM_STAT (channelBusy() cache source)
        RX_CHECK_FLAGS,     //!< Reading IRQ_FLAGS
        RX_CLEAR_FLAGS,     //!< Writing IRQ_FLAGS to clear RxDone/CRC error
        RX_READ_RSSI,       //!< Reading PKT_RSSI_VALUE
        RX_READ_SNR,        //!< Reading PKT_SNR_VALUE
        RX_READ_LENGTH,     //!< Reading RX_NB_BYTES
        RX_READ_CUR_ADDR,   //!< Reading FIFO_RX_CURRENT_ADDR
        RX_SET_FIFO_PTR,    //!< Writing FIFO_ADDR_PTR
        RX_READ_FIFO        //!< Burst-reading the payload out of the FIFO
    };

    //! Which in-flight sequence a pending SPI request belongs to, so
    //! spiWriteReadComplete_handler() knows which continueXxx() to resume.
    //! Exactly one of these is ever in flight at a time (single m_mosi/
    //! m_miso scratch buffer, single outstanding spiWriteRead_out() call).
    enum SpiOpOrigin {
        SPI_OP_NONE,  //!< No request outstanding; run() may start a new one
        SPI_OP_INIT,  //!< initializeRadio()'s CONFIGURE step sequence
        SPI_OP_TX,    //!< advanceTransmit()'s step sequence
        SPI_OP_RX     //!< The RxState sequence above
    };

    //! Source of the in-flight transmit buffer. A dataIn-originated buffer
    //! is Com-owned: completion must return it (dataReturnOut) and update
    //! comStatusOut. A command-originated buffer (SEND_STRING) has no Com
    //! owner, so those calls must be skipped — unconditionally invoking
    //! them would hit an unconnected-port fault in topologies where no
    //! Svc::ComQueue is wired to this radio yet.
    enum TxOrigin {
        TX_ORIGIN_COM,
        TX_ORIGIN_COMMAND
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

    //! Handler implementation for spiWriteReadComplete: resumes whichever
    //! sequence (bring-up/TX/RX) issued the request this completion answers.
    void spiWriteReadComplete_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer,
                                       const Drv::SpiStatus& status) override;

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

    //! Handler for SEND_STRING: stage a command-supplied string payload
    //! for transmission through the same engine dataIn uses
    void SEND_STRING_cmdHandler(FwOpcodeType opCode,          //!< The command opcode
                                U32 cmdSeq,                   //!< The command sequence number
                                const Fw::CmdStringArg& payload  //!< Payload to transmit
                                ) override;

    //! Schedule CONFIGURE on the next run tick after a parameter set
    void parameterUpdated(FwPrmIdType id  //!< The parameter ID
                          ) override;

    //! Mark parameters available; bring-up itself is advanced by run() ticks
    void parametersLoaded() override;

    // ----------------------------------------------------------------------
    // Helper functions: radio state management (Rfm9XManager.cpp)
    // ----------------------------------------------------------------------

    //! Start the next bounded step of whichever sequence is active (bring-up,
    //! TX, or RX) by issuing exactly one SPI request -- a no-op if one is
    //! already outstanding (m_spiOpOrigin != SPI_OP_NONE).
    void initializeRadio();

    //! Resume the bring-up CONFIGURE step sequence from the result of the
    //! request initializeRadio() most recently issued.
    void continueInitializeRadio(const Drv::SpiStatus& status);

    //! Start the RX sequence's next bounded step (one SPI op) if idle;
    //! a no-op if another sequence (bring-up/TX) is outstanding.
    void pollReceive();

    //! Resume the RX sequence from the result of the request pollReceive()
    //! most recently issued.
    void continueReceivePoll(const Drv::SpiStatus& status);

    //! Allocate and emit a received packet on dataOut (outside m_lock)
    void deliverReceivedPacket(const U8* data, FwSizeType size);

    //! Enter the tick-driven hardware reset sequence (or DETECT when the
    //! optional RST line is unwired). Also clears m_spiOpOrigin to
    //! SPI_OP_NONE and m_rxState to RX_IDLE: a request issued before this
    //! call may still be outstanding at the driver (every request is
    //! eventually answered, even an abandoned one), and this component is
    //! single-threaded (command handlers, run(), and
    //! spiWriteReadComplete_handler() all execute on the same cyclic
    //! executive, never concurrently), so clearing the origin here is
    //! enough to make spiWriteReadComplete_handler() drop that stale
    //! completion when it eventually arrives, rather than resuming a TX/RX
    //! sequence that RESET has already discarded.
    void beginResetSequence();

    //! Map the internal lifecycle state to telemetry and write the channel.
    void reportRadioState();

    //! Stage one native RF packet for scheduler-driven transmission.
    bool startTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context, TxOrigin origin);

    //! True when this deployment has no ComQueue retry path (comStatusOut unwired).
    //! Ground-station topologies use a local pending-TX hold instead of dropping.
    bool usesLocalTxHold() const;

    //! Enqueue one uplink buffer for a later run() tick. Returns false if full.
    bool enqueuePendingTransmit(Fw::Buffer& data, const ComCfg::FrameContext& context);

    //! If idle and clear, start the oldest pending uplink packet.
    //! Returns true when dropBuffer/dropContext must be returned after unlock.
    bool tryStartPendingTransmit(Fw::Buffer& dropBuffer, ComCfg::FrameContext& dropContext);

    //! Start the TX sequence's next bounded step (one SPI op) if idle;
    //! a no-op if another sequence (bring-up/RX) is outstanding. Returns
    //! TX_IN_PROGRESS unless this call issues no further SPI request this
    //! tick (nothing to start, or still waiting on run()'s own tick-driven
    //! TX_WAIT_TXDONE check).
    TxProgress advanceTransmit();

    //! Resume the TX sequence from the result of the request
    //! advanceTransmit() most recently issued. Returns the same tri-state
    //! progress advanceTransmit() does, since this is where TX_SUCCEEDED/
    //! TX_FAILED are actually reached.
    TxProgress continueAdvanceTransmit(const Drv::SpiStatus& status);

    //! Enter abort recovery after a transmit SPI/timeout failure.
    void abortTransmit();

    // ----------------------------------------------------------------------
    // Helper functions: RFM9X register access (Rfm9XHelpers.cpp)
    //
    // Each issueXxx() builds m_mosi, calls spiWriteRead_out() (no return
    // value -- the outcome arrives later via spiWriteReadComplete_handler()),
    // and sets m_spiOpOrigin so the handler knows which continueXxx() to
    // resume. Only one of these may be outstanding at a time; callers must
    // check m_spiOpOrigin == SPI_OP_NONE first (run_handler()'s dispatch
    // already guarantees this).
    // ----------------------------------------------------------------------

    //! \brief Issue a single radio register read, resumed via origin
    void issueReadRegister(U8 address,  //!< Register address
                           SpiOpOrigin origin  //!< Sequence to resume on completion
    );

    //! \brief Issue a single radio register write, resumed via origin
    void issueWriteRegister(U8 address,  //!< Register address
                            U8 value,    //!< Value to write
                            SpiOpOrigin origin  //!< Sequence to resume on completion
    );

    //! \brief Issue a burst write of payload bytes into the FIFO, resumed via origin
    void issueWriteFifo(const U8* data,  //!< Bytes to load
                        FwSizeType size,  //!< Byte count
                        SpiOpOrigin origin  //!< Sequence to resume on completion
    );

    //! \brief Issue a burst read of bytes from the FIFO, resumed via origin
    void issueReadFifo(FwSizeType size,  //!< Byte count
                       SpiOpOrigin origin  //!< Sequence to resume on completion
    );

    //! \brief Begin detecting the radio by reading the version register
    void issueDetectRadio();

    //! Begin requesting an operating mode, preserving LongRangeMode
    //! (RegOpMode bits 2-0)
    void issueRequestMode(U8 mode,  //!< Mode field value (Rfm9X::Mode)
                          SpiOpOrigin origin  //!< Sequence to resume on completion
    );

    //! \brief True when a reception is in progress (defer TX via Com FAILURE).
    //! Pure accessor: no SPI call. Reads m_lastChannelBusy, refreshed once
    //! per RX_IDLE->RX_CHECK_BUSY step (bounded staleness, see RxState).
    bool channelBusy();

    //! \brief True when downlink should wait (RX in progress or post-RX holdoff)
    bool downlinkBlocked();

    //! Emit comStatusOut when connected
    void emitComStatus(Fw::Success status);

    //! If a prior FAILURE left ComQueue waiting, clear resume flag once ready.
    //! Returns true when the caller should emit SUCCESS after releasing m_lock.
    bool maybeResumeComStatus();

    //! \brief Update RSSI/SNR telemetry from a completed PKT_RSSI_VALUE/
    //! PKT_SNR_VALUE read (RxState handles the two reads sequentially;
    //! this just converts+writes whichever register the completion carries)
    void applyRssiReading(U8 rawValue);
    void applySnrReading(U8 rawValue);

    //! Every output-port call (spiWriteRead_out, log_*, tlmWrite_*,
    //! comStatusOut_out) reached from code that runs under m_lock stages its
    //! call here instead of calling directly.
    struct PendingOutputs {
        bool spiRequest = false;
        FwIndexType spiRequestPortNum = 0;
        FwSizeType spiRequestSize = 0;

        bool logConfigurationFailed = false;
        bool logRadioReady = false;
        bool emitComReady = false;  // one-time startup handshake (m_comStatusAnnounced)

        bool radioStateChanged = false;
        Rfm9XRadioState radioState = Rfm9XRadioState::NOT_STARTED;

        bool logSendFailedRadioFault = false;  // abortTransmit()
        //! startTransmit()/enqueuePendingTransmit()'s own failure reason --
        //! distinct from logSendFailedRadioFault since these two report a
        //! caller-supplied reason (INVALID_SIZE/QUEUE_FULL), not a fixed one.
        bool logSendFailed = false;
        Rfm9XSendFailure sendFailedReason = Rfm9XSendFailure::INVALID_SIZE;
        bool packetsTransmittedChanged = false;
        U32 packetsTransmitted = 0;

        bool rxCrcErrorsChanged = false;
        U32 rxCrcErrors = 0;
        bool packetsReceivedChanged = false;
        U32 packetsReceived = 0;
        bool rssiChanged = false;
        F32 rssiDbm = 0.0f;
        bool snrChanged = false;
        F32 snrDb = 0.0f;
    };

    //! Call every output port named by a pending flag/value in
    //! m_pendingOutputs, then clear it. Must be called only after m_lock has
    //! been released -- run_handler()/spiWriteReadComplete_handler() are the
    //! sole callers, immediately after their own lock scope closes. Safe as
    //! a single member (not a value threaded through every helper's
    //! signature) because this component is single-threaded: run(),
    //! spiWriteReadComplete(), and the command handlers never execute
    //! concurrently, so at most one lock scope -- and therefore one batch of
    //! staged outputs -- is ever open at a time.
    void flushPendingOutputs();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Serializes radio access between dataIn (com queue thread) and run
    //! (rate group thread): both drive multi-transaction SPI sequences and
    //! share the SPI scratch buffers
    Os::Mutex m_lock;

    //! Which sequence (if any) has an SPI request outstanding. Only
    //! SPI_OP_NONE may start a new issueXxx() call; run_handler()'s
    //! dispatch and spiWriteReadComplete_handler()'s resume both gate on
    //! this single discriminator, since only one m_mosi/m_miso transaction
    //! is ever in flight at a time.
    SpiOpOrigin m_spiOpOrigin;

    //! Current step of the bring-up CONFIGURE sequence (one write per
    //! continueInitializeRadio() call): 0-2 are enterLoRaMode()'s three
    //! writes, 3..3+N-1 are the configuration[] table entries, the next is
    //! the IRQ-flags clear, the last is requestMode(RX_CONTINUOUS).
    U32 m_configureStep;

    //! Current step of the RX sequence (one SPI op per
    //! continueReceivePoll() call).
    RxState m_rxState;
    //! Cached result of the RX_CHECK_BUSY step's MODEM_STAT read, returned
    //! by channelBusy() without issuing a fresh SPI transaction. Staleness
    //! is bounded by the RX_IDLE->RX_CHECK_BUSY cadence (once per idle RX
    //! poll cycle), comfortably inside LoRa packet airtimes.
    bool m_lastChannelBusy;
    //! IRQ_FLAGS byte latched by RX_CHECK_FLAGS, consulted by RX_CLEAR_FLAGS
    //! (to decide RxDone/CRC-error) and by RX_READ_LENGTH's caller.
    U8 m_rxIrqFlags;
    //! RX_NB_BYTES value latched by RX_READ_LENGTH, consumed by
    //! RX_READ_CUR_ADDR/RX_SET_FIFO_PTR/RX_READ_FIFO to size the burst read.
    U8 m_rxPacketLength;

    //! True while a downlink buffer is owned by the scheduler-driven TX state machine.
    bool m_txActive;
    TxState m_txState;                //!< Current step of the staged TX sequence
    Fw::Buffer m_txBuffer;            //!< Downlink buffer owned by the TX machine
    ComCfg::FrameContext m_txContext; //!< Frame context returned with m_txBuffer
    U32 m_txWaitTicks;                //!< Ticks spent waiting for TxDone
    //! Source of m_txBuffer: gates dataReturnOut/comStatusOut, which are only
    //! meaningful for a dataIn (Com-owned) buffer.
    TxOrigin m_txOrigin;
    //! Backing storage for a SEND_STRING command payload, wrapped in an
    //! Fw::Buffer and handed to startTransmit(). No dynamic allocation; safe
    //! as a single static array because m_txActive already excludes a second
    //! transmit (command or dataIn) from starting concurrently.
    U8 m_cmdPayload[MAX_PACKET_PAYLOAD];

    //! Local hold used when comStatusOut is unwired (no ComQueue). Mirrors the
    //! Arduino RadioHead ground-station usbHold/retry behavior for half-duplex.
    //! Depth comes from Rfm9X.ManagerCfg.PENDING_TX_DEPTH -- see that
    //! constant's own doc comment: each slot costs one Fw::Buffer + one
    //! ComCfg::FrameContext (~168 bytes on a 32-bit target), the single
    //! largest RAM cost in this component at the library default of 8
    //! (sized to the GS frame-bin pool so a short GDS burst cannot overflow
    //! and drop mid-file CFDP DATA packets). A deployment with a real
    //! ComQueue wired to comStatusOut (usesLocalTxHold() false) never fills
    //! this queue and can override it down to 0 or 1.
    static constexpr U32 PENDING_TX_DEPTH = Rfm9X::ManagerCfg::PENDING_TX_DEPTH;
    Fw::Buffer m_pendingTxBuffers[PENDING_TX_DEPTH];
    ComCfg::FrameContext m_pendingTxContexts[PENDING_TX_DEPTH];
    U32 m_pendingTxCount;

    //! Completed packet length staged for post-unlock delivery (0 = none)
    FwSizeType m_rxDeliverSize;
    U8 m_rxPayload[MAX_PACKET_PAYLOAD];
    static_assert(sizeof(m_rxPayload) == MAX_PACKET_PAYLOAD,
                  "RX delivery bounds checks assume m_rxPayload holds MAX_PACKET_PAYLOAD bytes");
    //! Remaining run() ticks to defer flight downlink after a completed RX
    U32 m_rxTxHoldoffTicks;
    //! Remaining run() ticks to defer the next local TX after a completed TX
    U32 m_txTxHoldoffTicks;
    //! Consecutive ticks a pending downlink has been blocked by the RX lease
    U32 m_txStarvedTicks;

    //! Tick-count bounds computed by configure() from Rfm9X.ManagerCfg's
    //! millisecond constants and this instance's actual tick period --
    //! replaces the old fixed "1 kHz tick" constants (TX_DONE_TIMEOUT_TICKS,
    //! RX_TX_HOLDOFF_TICKS, TX_TX_HOLDOFF_TICKS, RX_HOLDOFF_STARVATION_TICKS).
    //! Initialized in the constructor to the same values configure(1) would
    //! produce, so an instance that never calls configure() (e.g. a unit
    //! test) behaves exactly as the old hardcoded constants did.
    U32 m_txDoneTimeoutTicks;
    U32 m_rxTxHoldoffBoundTicks;
    U32 m_txTxHoldoffBoundTicks;
    U32 m_rxHoldoffStarvationTicks;
    //! RESET_HOLD/RESET_SETTLE's duration, same configure()-computed-from-ms
    //! pattern as the four above -- see RadioState's own doc comment for why
    //! these cover either a reset-line or a power-enable-GPIO deployment.
    U32 m_resetHoldTicks;
    U32 m_resetSettleTicks;

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
    //! 255-byte LoRa FIFO payload). DMA-safe placement (outside DTCM) is a
    //! linker-script/placement concern for the Stm32SpiDriver instance this
    //! component is wired to in DMA mode, not this component's responsibility.
    //! Safe to reuse across the whole bring-up/TX/RX lifetime because
    //! m_spiOpOrigin != SPI_OP_NONE already excludes a second transaction
    //! from starting while one is outstanding.
    alignas(32) U8 m_mosi[MAX_PACKET_PAYLOAD + 2];
    alignas(32) U8 m_miso[MAX_PACKET_PAYLOAD + 2];

    //! Output calls staged by code running under m_lock, drained by
    //! flushPendingOutputs() after the lock releases.
    PendingOutputs m_pendingOutputs;

    //! One entry of configureRadio()'s modem-profile register table.
    struct ConfigEntry {
        U8 address;
        U8 value;
    };
    //! The modem-profile register table itself, populated once by
    //! continueInitializeRadio() the first time it enters the
    //! post-enterLoRaMode() steps (from current FPP parameters) and then
    //! walked one entry per subsequent step via m_configureStep.
    static constexpr FwSizeType CONFIG_TABLE_SIZE = 16;
    ConfigEntry m_configTable[CONFIG_TABLE_SIZE];
};

}  // namespace Rfm9X

#endif  // Rfm9X_Rfm9XManager_HPP
