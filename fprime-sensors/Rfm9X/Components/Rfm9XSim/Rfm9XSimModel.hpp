// ======================================================================
// \title  Rfm9XSimModel.hpp
// \brief  Register-level model of an RFM9X (SX1276/8) LoRa radio
//
// Plain C++ class (no component dependencies) modeling the RFM9X LoRa
// register file, dual-pointer FIFO, and IRQ behavior visible over SPI.
// Used by the Rfm9XSim component and by the Rfm9XManager unit tests.
//
// Time is modeled with a byte clock: each SPI byte exchanged advances
// the modeled air interface by one byte time, so TX/RX airtime is paced
// against the manager's polling the same way the real radio's airtime
// is paced against the SPI clock.
// ======================================================================

#ifndef Rfm9X_Rfm9XSimModel_HPP
#define Rfm9X_Rfm9XSimModel_HPP

#include <Fw/FPrimeBasicTypes.hpp>
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XRadio.hpp"

namespace Rfm9X {

class Rfm9XSimModel {
  public:
    //! Number of modeled registers (covers all 7-bit SPI addresses)
    static constexpr FwSizeType REGISTER_COUNT = 0x80;
    //! Bound on buffered air-interface (uplink) bytes
    static constexpr FwSizeType AIR_BUFFER_SIZE = 2048;
    //! Bound on queued transmitted packets awaiting retrieval
    static constexpr FwSizeType TX_QUEUE_DEPTH = 8;
    //! Default RegPktRssiValue readback: RSSI[dBm] = -137 + reg, reg=97 => -40 dBm
    static constexpr U8 RSSI_READBACK = 0x61;
    //! Default RegPktSnrValue readback: SNR[dB] = (two's complement reg) / 4, reg=0 => 0 dB
    static constexpr U8 SNR_READBACK = 0x00;
    //! Bound on non-FIFO register writes retained for white-box assertions
    static constexpr FwSizeType REGISTER_WRITE_HISTORY_SIZE = 64;
    //! Byte times between queued air packets, modeling the preamble/sync
    //! that precede every over-the-air LoRa frame
    static constexpr FwSizeType RX_INTER_PACKET_GAP = 12;
    //! SPI byte clocks per air byte: the RF bit rate is slower than the SPI
    //! clock, so the host can always drain faster than the air fills
    static constexpr FwSizeType AIR_CLOCK_DIVISOR = 2;

    Rfm9XSimModel();

    //! Reset registers, FIFO, and buffers to power-on state
    void reset();

    //! Perform one SPI frame (NSS low..high): full-duplex, first byte address
    void spiTransaction(const U8* mosi, U8* miso, FwSizeType size);

    //! Inject bytes arriving over the simulated air interface (uplink)
    void injectAirData(const U8* data, FwSizeType size);

    //! Inject a frame that fails the hardware CRC. RxDone still asserts
    //! (LoRa reports header/packet completion independently of CRC result),
    //! but PayloadCrcError asserts alongside it, matching datasheet section
    //! 4.1.5: "The PayloadCrcError flag should be checked for packet
    //! integrity." Used to verify the manager drops such frames instead of
    //! forwarding them to the deframer.
    void injectCorruptFrame(U8 payloadLen);

    //! Retrieve the next transmitted packet payload (downlink).
    //! \return payload size, or 0 when no packet is pending
    FwSizeType retrievePacket(U8* data, FwSizeType capacity);

    //! Return a register value for white-box unit-test assertions. Dynamic
    //! registers (RegIrqFlags, RegModemStat) are synthesized by the model
    //! and are therefore intentionally not exposed through this helper.
    U8 readRegisterValue(U8 address) const;

    //! Suppress TxDone completion for an in-flight TX packet. This lets
    //! tests exercise the manager's bounded TX wait and RX recovery path.
    void setTxDoneStall(bool stall);

    //! Clear the bounded non-FIFO register-write history.
    void clearRegisterWriteHistory();

    //! Return whether a non-FIFO register write occurred since the history
    //! was last cleared. This exposes transient radio controls such as the
    //! DBM_20 PA_BOOST/PaDac settings without changing the simulation's
    //! functional API.
    bool wasRegisterWritten(U8 address, U8 value) const;

    //! Model oscillator/PLL settling: no packet-engine byte-clock activity
    //! for `byteTimes` byte clocks after every mode change, as on real
    //! hardware. Zero restores instant settling.
    void setModeSettleByteTimes(FwSizeType byteTimes);

    //! Model an absent or unpowered radio: SPI reads return zero and writes
    //! are ignored, exercising the manager's detection and retry paths.
    void setRadioPresent(bool present);

    //! Set the RegPktRssiValue readback applied to the next completed reception.
    void setRssiReadback(U8 value);

    //! Set the RegPktSnrValue readback applied to the next completed reception.
    void setSnrReadback(U8 value);

    //! Model a busy RF channel: RegModemStat reports RX ongoing / signal
    //! detected even with no packet in delivery, exercising listen-before-talk
    //! holdoff.
    void setChannelBusy(bool busy);

  private:
    //! Advance the modeled air interface by a number of byte times
    void advanceClock(FwSizeType byteTimes);

    //! Read a register as seen over SPI, applying side effects (FIFO pop)
    U8 readRegister(U8 address);

    //! Write a register over SPI, applying side effects (FIFO push, mode change)
    void writeRegister(U8 address, U8 value);

    //! Handle a mode-field change written to RegOpMode (bits 2-0)
    void handleModeChange(U8 mode);

    //! Begin transmitting the payload staged in the FIFO at FifoTxBaseAddr
    void startTransmit();

    //! Complete an in-progress transmission: queue the payload, assert
    //! TxDone, and auto-return the mode field to Standby (datasheet section
    //! 4.1.5: "Upon completion the TxDone interrupt is issued and the radio
    //! returns to Stand-by mode").
    void completeTransmit();

    //! Begin delivering the next uplink packet from the air buffer
    void startReceive();

    //! Current RegModemStat value
    U8 modemStat() const;

    //! Current register backing store (excludes the FIFO data buffer, which
    //! is addressed indirectly through RegFifoAddrPtr)
    U8 m_registers[REGISTER_COUNT];

    //! 256-byte LoRa FIFO data buffer, shared between TX and RX via base
    //! address registers (datasheet section 4.1.2)
    U8 m_fifoBuffer[FIFO_SIZE];

    //! Soft RegIrqFlags state (RxDone/TxDone/PayloadCrcError/...). Writing a
    //! 1 to a bit clears it, mirroring the real register's "rc" access type.
    U8 m_irqFlags;

    // Bounded non-FIFO write history for unit-test observation of transient
    // register values. FIFO data is intentionally excluded.
    U8 m_registerWriteAddresses[REGISTER_WRITE_HISTORY_SIZE];
    U8 m_registerWriteValues[REGISTER_WRITE_HISTORY_SIZE];
    FwSizeType m_registerWriteCount;

    // Fault/behavior injection controls
    FwSizeType m_modeSettleByteTimes;
    FwSizeType m_modeSettleRemaining;
    bool m_radioPresent;
    bool m_channelBusy;
    bool m_txDoneStall;
    U8 m_rssiReadback;
    U8 m_snrReadback;

    // In-progress transmission: airtime is modeled as a fixed number of byte
    // clocks proportional to the payload length staged in the FIFO.
    bool m_txActive;
    FwSizeType m_txExpected;
    FwSizeType m_txElapsed;
    U8 m_txAccum[MAX_PACKET_PAYLOAD];

    // In-progress reception: bytes stream into the FIFO buffer by the byte
    // clock, starting at the RX base address captured when reception began.
    bool m_rxActive;
    //! True once RxDone has been asserted but not yet acknowledged (cleared)
    //! by the host. Blocks starting a new reception, mirroring real hardware
    //! holding the completed packet in the FIFO until the host reads it out.
    bool m_rxPendingDelivery;
    U8 m_rxStartAddr;
    FwSizeType m_rxTotal;
    FwSizeType m_rxDelivered;
    //! When set, the completed reception asserts PayloadCrcError alongside
    //! RxDone (datasheet section 4.1.5: RxDone and CRC pass/fail are independent).
    bool m_rxCorrupt;
    //! Remaining preamble/sync byte times before the next queued packet starts
    FwSizeType m_rxGapRemaining;
    //! SPI byte clock counter used to derive the slower air byte clock
    FwSizeType m_airClock;
    U8 m_rxPacket[MAX_PACKET_PAYLOAD];

    // Buffered uplink bytes awaiting packetization
    U8 m_airBuffer[AIR_BUFFER_SIZE];
    FwSizeType m_airCount;

    // Queue of transmitted packet payloads awaiting retrieval
    U8 m_txQueue[TX_QUEUE_DEPTH][MAX_PACKET_PAYLOAD];
    FwSizeType m_txSizes[TX_QUEUE_DEPTH];
    FwSizeType m_txCount;
};

}  // namespace Rfm9X

#endif  // Rfm9X_Rfm9XSimModel_HPP