// ======================================================================
// \title  Rfm9XHelpers.cpp
// \brief  RFM9X (SX1276/8) LoRa radio-specific helper functions for the
//         Rfm9XManager
//
// SPI register access and packet handling per the HopeRF RFM95/96/97/98(W)
// datasheet. Keeps the radio specifics out of the F Prime handler code in
// Rfm9XManager.cpp.
// ======================================================================

#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManager.hpp"
#include <cstring>

namespace Rfm9X {

// ----------------------------------------------------------------------
// Register access primitives: issue a request, return immediately. The
// outcome arrives later via spiWriteReadComplete_handler(), which resumes
// whichever sequence `origin` names.
// ----------------------------------------------------------------------

void Rfm9XManager ::issueReadRegister(U8 address, SpiOpOrigin origin) {
    // Single access read: address byte (wnr=0) then one dummy byte
    this->m_mosi[0] = address & SPI_ADDRESS_MASK;
    this->m_mosi[1] = 0;
    Fw::Buffer writeBuffer(this->m_mosi, 2);
    Fw::Buffer readBuffer(this->m_miso, 2);
    this->m_spiOpOrigin = origin;
    this->spiWriteRead_out(0, writeBuffer, readBuffer);
}

void Rfm9XManager ::issueWriteRegister(U8 address, U8 value, SpiOpOrigin origin) {
    // Single access write: address byte with wnr=1 then the data byte
    this->m_mosi[0] = (address & SPI_ADDRESS_MASK) | SPI_WRITE_FLAG;
    this->m_mosi[1] = value;
    Fw::Buffer writeBuffer(this->m_mosi, 2);
    Fw::Buffer readBuffer(this->m_miso, 2);
    this->m_spiOpOrigin = origin;
    this->spiWriteRead_out(0, writeBuffer, readBuffer);
}

void Rfm9XManager ::issueWriteFifo(const U8* data, FwSizeType size, SpiOpOrigin origin) {
    FW_ASSERT(data != nullptr);
    FW_ASSERT((size + 1) <= sizeof this->m_mosi, static_cast<FwAssertArgType>(size));
    this->m_mosi[0] = Reg::FIFO | SPI_WRITE_FLAG;
    (void)::memcpy(&this->m_mosi[1], data, size);
    Fw::Buffer writeBuffer(this->m_mosi, size + 1);
    Fw::Buffer readBuffer(this->m_miso, size + 1);
    this->m_spiOpOrigin = origin;
    this->spiWriteRead_out(0, writeBuffer, readBuffer);
}

void Rfm9XManager ::issueReadFifo(FwSizeType size, SpiOpOrigin origin) {
    FW_ASSERT((size + 1) <= sizeof this->m_mosi, static_cast<FwAssertArgType>(size));
    this->m_mosi[0] = Reg::FIFO;
    (void)::memset(&this->m_mosi[1], 0, size);
    Fw::Buffer writeBuffer(this->m_mosi, size + 1);
    Fw::Buffer readBuffer(this->m_miso, size + 1);
    this->m_spiOpOrigin = origin;
    this->spiWriteRead_out(0, writeBuffer, readBuffer);
}

void Rfm9XManager ::issueDetectRadio() {
    this->issueReadRegister(Reg::VERSION, SPI_OP_INIT);
}

void Rfm9XManager ::issueRequestMode(U8 mode, SpiOpOrigin origin) {
    // LongRangeMode (bit 7) must stay set once configured; only the mode
    // field (bits 2-0) changes here.
    const U8 opMode = static_cast<U8>(Mode::LONG_RANGE_MODE | (mode & Mode::MASK));
    this->issueWriteRegister(Reg::OP_MODE, opMode, origin);
}

bool Rfm9XManager ::channelBusy() {
    // LoRa has no SyncAddressMatch-equivalent IRQ bit; RegModemStat reports
    // live demodulator activity (datasheet section 6.4, RegModemStat 0x18).
    // Pure accessor now that SPI is async: the RX sequence's RX_CHECK_BUSY
    // step refreshes m_lastChannelBusy once per idle poll cycle.
    return this->m_lastChannelBusy;
}

bool Rfm9XManager ::downlinkBlocked() {
    return this->channelBusy() || (this->m_rxTxHoldoffTicks > 0) || (this->m_txTxHoldoffTicks > 0);
}

// ----------------------------------------------------------------------
// Packet handling
// ----------------------------------------------------------------------

void Rfm9XManager ::abortTransmit() {
    this->log_WARNING_HI_SendFailed(Rfm9XSendFailure::RADIO_FAULT);
    this->m_txState = TX_ABORT_CLEAR_IRQ;
}

Rfm9XManager::TxProgress Rfm9XManager ::advanceTransmit() {
    FW_ASSERT(this->m_txActive);

    switch (this->m_txState) {
        case TX_REQUEST_STANDBY:
            this->issueRequestMode(Mode::STANDBY, SPI_OP_TX);
            break;

        case TX_WAIT_TXDONE:
            // Tick-driven, not request-driven: the timeout must advance
            // even while no SPI op is outstanding between polls of the
            // IRQ flag, so this one step issues its own read every entry
            // (same one-op-per-step discipline, just re-entered by run()
            // on each tick instead of waiting for a prior op here).
            this->issueReadRegister(Reg::IRQ_FLAGS, SPI_OP_TX);
            break;

        case TX_SET_FIFO_PTR:
            // LoRa Transmit Data FIFO Filling (datasheet section 4.1.5):
            // "Set FifoPtrAddr to FifoTxPtrBase. Write PayloadLength bytes to FIFO."
            this->issueWriteRegister(Reg::FIFO_ADDR_PTR, FIFO_BASE_ADDR, SPI_OP_TX);
            break;

        case TX_SET_PAYLOAD_LEN:
            this->issueWriteRegister(Reg::PAYLOAD_LENGTH, static_cast<U8>(this->m_txBuffer.getSize()), SPI_OP_TX);
            break;

        case TX_SET_DIO_TXDONE:
            this->issueWriteRegister(Reg::DIO_MAPPING_1, Dio0Mapping::TX_DONE, SPI_OP_TX);
            break;

        case TX_WRITE_PAYLOAD:
            // Single standby fill: the 256-byte LoRa FIFO holds the full
            // 255-byte max payload in one burst (no in-flight top-up needed).
            this->issueWriteFifo(this->m_txBuffer.getData(), this->m_txBuffer.getSize(), SPI_OP_TX);
            break;

        case TX_REQUEST_TX:
            this->issueRequestMode(Mode::TX, SPI_OP_TX);
            break;

        case TX_CLEAR_TXDONE_IRQ:
            this->issueWriteRegister(Reg::IRQ_FLAGS, IrqFlags::TX_DONE, SPI_OP_TX);
            break;

        case TX_SET_DIO_RXDONE:
            this->issueWriteRegister(Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE, SPI_OP_TX);
            break;

        case TX_REQUEST_RX:
            this->issueRequestMode(Mode::RX_CONTINUOUS, SPI_OP_TX);
            break;

        case TX_ABORT_CLEAR_IRQ:
            this->issueWriteRegister(Reg::IRQ_FLAGS, 0xFF, SPI_OP_TX);
            break;

        case TX_ABORT_SET_DIO_RXDONE:
            this->issueWriteRegister(Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE, SPI_OP_TX);
            break;

        case TX_ABORT_REQUEST_RX:
            this->issueRequestMode(Mode::RX_CONTINUOUS, SPI_OP_TX);
            break;

        case TX_IDLE:
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_txState));
            break;
    }
    return TX_IN_PROGRESS;
}

Rfm9XManager::TxProgress Rfm9XManager ::continueAdvanceTransmit(const Drv::SpiStatus& status) {
    FW_ASSERT(this->m_txActive);
    const FwSizeType size = this->m_txBuffer.getSize();
    const U8* const data = this->m_txBuffer.getData();
    FW_ASSERT(data != nullptr);
    FW_ASSERT((size > 0) && (size <= MAX_PACKET_PAYLOAD), static_cast<FwAssertArgType>(size));
    const bool ok = (status == Drv::SpiStatus::SPI_OK);

    switch (this->m_txState) {
        case TX_REQUEST_STANDBY:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_SET_FIFO_PTR;
            }
            break;

        case TX_SET_FIFO_PTR:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_SET_PAYLOAD_LEN;
            }
            break;

        case TX_SET_PAYLOAD_LEN:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_SET_DIO_TXDONE;
            }
            break;

        case TX_SET_DIO_TXDONE:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_WRITE_PAYLOAD;
            }
            break;

        case TX_WRITE_PAYLOAD:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_REQUEST_TX;
            }
            break;

        case TX_REQUEST_TX:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txWaitTicks = 0;
                this->m_txState = TX_WAIT_TXDONE;
            }
            break;

        case TX_WAIT_TXDONE: {
            if (!ok) {
                this->abortTransmit();
                break;
            }
            const U8 flags = this->m_miso[1];
            if ((flags & IrqFlags::TX_DONE) != 0) {
                this->m_txState = TX_CLEAR_TXDONE_IRQ;
            } else if (++this->m_txWaitTicks >= TX_DONE_TIMEOUT_TICKS) {
                this->abortTransmit();
            }
            // else: stay in TX_WAIT_TXDONE: advanceTransmit() re-issues the
            // IRQ_FLAGS read on the next run() tick.
            break;
        }

        case TX_CLEAR_TXDONE_IRQ:
            // Best-effort clear, matching the original design: proceed to
            // RX re-entry regardless of this write's outcome.
            this->m_txState = TX_SET_DIO_RXDONE;
            break;

        case TX_SET_DIO_RXDONE:
            if (!ok) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_REQUEST_RX;
            }
            break;

        case TX_REQUEST_RX:
            if (!ok) {
                this->abortTransmit();
                break;
            }
            this->m_packetsTransmitted++;
            this->tlmWrite_PacketsTransmitted(this->m_packetsTransmitted);
            this->log_WARNING_HI_ConfigurationFailed_ThrottleClear();
            this->log_WARNING_HI_SendFailed_ThrottleClear();
            return TX_SUCCEEDED;

        case TX_ABORT_CLEAR_IRQ:
            // Best-effort, same as the forward-path TX_CLEAR_TXDONE_IRQ.
            this->m_txState = TX_ABORT_SET_DIO_RXDONE;
            break;

        case TX_ABORT_SET_DIO_RXDONE:
            // Best-effort: recover to RX regardless of outcome -- this is
            // already the failure path, matching the original design's
            // unconditional abort-recovery writes.
            this->m_txState = TX_ABORT_REQUEST_RX;
            break;

        case TX_ABORT_REQUEST_RX:
            return TX_FAILED;

        case TX_IDLE:
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_txState));
            break;
    }
    return TX_IN_PROGRESS;
}

void Rfm9XManager ::applyRssiReading(U8 rawValue) {
    // RSSI [dBm] = -137 + PacketRssi (datasheet section 6.4, RegPktRssiValue)
    this->tlmWrite_LastPacketRssi(-137.0f + static_cast<F32>(rawValue));
}

void Rfm9XManager ::applySnrReading(U8 rawValue) {
    // SNR [dB] = PacketSnr (two's complement) / 4 (datasheet section 6.4, RegPktSnrValue)
    this->tlmWrite_LastPacketSnr(static_cast<F32>(static_cast<I8>(rawValue)) / 4.0f);
}

}  // namespace Rfm9X
