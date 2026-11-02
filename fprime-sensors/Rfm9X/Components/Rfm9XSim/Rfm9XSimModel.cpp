// ======================================================================
// \title  Rfm9XSimModel.cpp
// \brief  Register-level model of an RFM9X (SX1276/8) LoRa radio
// ======================================================================

#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.hpp"
#include <cstring>

namespace Rfm9X {

Rfm9XSimModel::Rfm9XSimModel()
    : m_irqFlags(0),
      m_registerWriteCount(0),
      m_modeSettleByteTimes(0),
      m_modeSettleRemaining(0),
      m_radioPresent(true),
      m_channelBusy(false),
      m_txDoneStall(false),
      m_rssiReadback(RSSI_READBACK),
      m_snrReadback(SNR_READBACK),
      m_txActive(false),
      m_txExpected(0),
      m_txElapsed(0),
      m_rxActive(false),
      m_rxPendingDelivery(false),
      m_rxStartAddr(0),
      m_rxTotal(0),
      m_rxDelivered(0),
      m_rxCorrupt(false),
      m_rxGapRemaining(0),
      m_airClock(0),
      m_airCount(0),
      m_txCount(0) {
    this->reset();
}

void Rfm9XSimModel::reset() {
    (void)::memset(this->m_registers, 0, sizeof this->m_registers);
    (void)::memset(this->m_fifoBuffer, 0, sizeof this->m_fifoBuffer);
    (void)::memset(this->m_registerWriteAddresses, 0, sizeof this->m_registerWriteAddresses);
    (void)::memset(this->m_registerWriteValues, 0, sizeof this->m_registerWriteValues);
    (void)::memset(this->m_txAccum, 0, sizeof this->m_txAccum);
    (void)::memset(this->m_rxPacket, 0, sizeof this->m_rxPacket);
    (void)::memset(this->m_airBuffer, 0, sizeof this->m_airBuffer);
    (void)::memset(this->m_txQueue, 0, sizeof this->m_txQueue);
    (void)::memset(this->m_txSizes, 0, sizeof this->m_txSizes);
    // datasheet section 6.1/6.4 reset values for registers the manager interacts with
    this->m_registers[Reg::OP_MODE] = static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::STANDBY);
    this->m_registers[Reg::FRF_MSB] = 0x6C;
    this->m_registers[Reg::FRF_MID] = 0x80;
    this->m_registers[Reg::FRF_LSB] = 0x00;
    this->m_registers[Reg::PA_CONFIG] = 0x4F;
    this->m_registers[Reg::PA_RAMP] = 0x09;
    this->m_registers[Reg::OCP] = 0x2B;
    this->m_registers[Reg::LNA] = 0x20;
    this->m_registers[Reg::FIFO_ADDR_PTR] = 0x00;
    this->m_registers[Reg::FIFO_TX_BASE_ADDR] = 0x80;
    this->m_registers[Reg::FIFO_RX_BASE_ADDR] = 0x00;
    this->m_registers[Reg::MODEM_CONFIG_1] = 0x72;
    this->m_registers[Reg::MODEM_CONFIG_2] = 0x70;
    this->m_registers[Reg::SYMB_TIMEOUT_LSB] = 0x64;
    this->m_registers[Reg::PREAMBLE_LSB] = 0x08;
    this->m_registers[Reg::PAYLOAD_LENGTH] = 0x01;
    this->m_registers[Reg::MAX_PAYLOAD_LENGTH] = 0xFF;
    this->m_registers[Reg::DIO_MAPPING_1] = 0x00;
    this->m_registers[Reg::DIO_MAPPING_2] = 0x00;
    this->m_registers[Reg::VERSION] = VERSION_VALUE;
    this->m_registers[Reg::PA_DAC] = 0x84;
    this->m_irqFlags = 0;
    this->m_registerWriteCount = 0;
    this->m_modeSettleRemaining = this->m_modeSettleByteTimes;
    this->m_channelBusy = false;
    this->m_txDoneStall = false;
    this->m_rssiReadback = RSSI_READBACK;
    this->m_snrReadback = SNR_READBACK;
    this->m_txActive = false;
    this->m_txExpected = 0;
    this->m_txElapsed = 0;
    this->m_rxActive = false;
    this->m_rxPendingDelivery = false;
    this->m_rxStartAddr = 0;
    this->m_rxTotal = 0;
    this->m_rxDelivered = 0;
    this->m_rxCorrupt = false;
    this->m_rxGapRemaining = 0;
    this->m_airClock = 0;
    this->m_airCount = 0;
    this->m_txCount = 0;
}

void Rfm9XSimModel::spiTransaction(const U8* mosi, U8* miso, FwSizeType size) {
    if ((mosi == nullptr) || (miso == nullptr) || (size == 0)) {
        return;
    }
    // An absent or unpowered radio drives MISO low and latches nothing
    if (!this->m_radioPresent) {
        (void)::memset(miso, 0, size);
        return;
    }
    // Each SPI byte exchanged advances the modeled air interface one byte
    // time, pacing transmission/reception against the manager's polling
    this->advanceClock(size);
    // First byte: wnr flag plus 7-bit address (datasheet section 4.3/section 5.2.1 analog)
    const bool isWrite = (mosi[0] & SPI_WRITE_FLAG) != 0;
    U8 address = mosi[0] & SPI_ADDRESS_MASK;
    miso[0] = 0;
    for (FwSizeType i = 1; i < size; i++) {
        if (isWrite) {
            // The pre-write register value appears on MISO; FIFO writes must
            // not trigger the FIFO-pop side effect of a read
            miso[i] = (address == Reg::FIFO) ? 0 : this->m_registers[address & SPI_ADDRESS_MASK];
            this->writeRegister(address, mosi[i]);
        } else {
            miso[i] = this->readRegister(address);
        }
        // Burst access on a non-FIFO register auto-increments the 7-bit SPI
        // address (datasheet section 4.3, burst access). FIFO access instead
        // auto-increments the internal FifoAddrPtr (section 4.1.5), modeled
        // in readRegister/writeRegister on the FIFO case, so the outer
        // address must stay pinned at Reg::FIFO across the whole transaction.
        if (address != Reg::FIFO) {
            address = (address + 1) & SPI_ADDRESS_MASK;
        }
    }
}

void Rfm9XSimModel::injectAirData(const U8* data, FwSizeType size) {
    if ((data == nullptr) || (size == 0)) {
        return;
    }
    // Drop oldest bytes when the air buffer would overflow
    if (size > AIR_BUFFER_SIZE) {
        data = &data[size - AIR_BUFFER_SIZE];
        size = AIR_BUFFER_SIZE;
    }
    if ((this->m_airCount + size) > AIR_BUFFER_SIZE) {
        const FwSizeType excess = (this->m_airCount + size) - AIR_BUFFER_SIZE;
        (void)::memmove(this->m_airBuffer, &this->m_airBuffer[excess], this->m_airCount - excess);
        this->m_airCount -= excess;
    }
    (void)::memcpy(&this->m_airBuffer[this->m_airCount], data, size);
    this->m_airCount += size;
    this->startReceive();
}

void Rfm9XSimModel::injectCorruptFrame(U8 payloadLen) {
    // Only stage when the radio is idle in RX (mirrors startReceive() guards).
    if (((this->m_registers[Reg::OP_MODE] & Mode::MASK) != Mode::RX_CONTINUOUS) || this->m_rxActive ||
        this->m_rxPendingDelivery) {
        return;
    }
    if (payloadLen > MAX_PACKET_PAYLOAD) {
        payloadLen = MAX_PACKET_PAYLOAD;
    }
    for (FwSizeType i = 0; i < static_cast<FwSizeType>(payloadLen); i++) {
        this->m_rxPacket[i] = static_cast<U8>(0xA5 ^ i);
    }
    this->m_rxTotal = static_cast<FwSizeType>(payloadLen);
    this->m_rxDelivered = 0;
    this->m_rxActive = true;
    this->m_rxCorrupt = true;
}

FwSizeType Rfm9XSimModel::retrievePacket(U8* data, FwSizeType capacity) {
    if ((data == nullptr) || (this->m_txCount == 0)) {
        return 0;
    }
    const FwSizeType size = FW_MIN(this->m_txSizes[0], capacity);
    (void)::memcpy(data, this->m_txQueue[0], size);
    // Shift the queue forward
    for (FwSizeType i = 1; i < this->m_txCount; i++) {
        (void)::memcpy(this->m_txQueue[i - 1], this->m_txQueue[i], sizeof this->m_txQueue[i]);
        this->m_txSizes[i - 1] = this->m_txSizes[i];
    }
    this->m_txCount--;
    return size;
}

U8 Rfm9XSimModel::readRegisterValue(U8 address) const {
    return this->m_registers[address & SPI_ADDRESS_MASK];
}

void Rfm9XSimModel::setTxDoneStall(bool stall) {
    this->m_txDoneStall = stall;
}

void Rfm9XSimModel::setModeSettleByteTimes(FwSizeType byteTimes) {
    this->m_modeSettleByteTimes = byteTimes;
}

void Rfm9XSimModel::setRadioPresent(bool present) {
    this->m_radioPresent = present;
}

void Rfm9XSimModel::setRssiReadback(U8 value) {
    this->m_rssiReadback = value;
}

void Rfm9XSimModel::setSnrReadback(U8 value) {
    this->m_snrReadback = value;
}

void Rfm9XSimModel::setChannelBusy(bool busy) {
    this->m_channelBusy = busy;
}

void Rfm9XSimModel::clearRegisterWriteHistory() {
    this->m_registerWriteCount = 0;
}

bool Rfm9XSimModel::wasRegisterWritten(U8 address, U8 value) const {
    const U8 normalizedAddress = address & SPI_ADDRESS_MASK;
    for (FwSizeType i = 0; i < this->m_registerWriteCount; i++) {
        if ((this->m_registerWriteAddresses[i] == normalizedAddress) && (this->m_registerWriteValues[i] == value)) {
            return true;
        }
    }
    return false;
}

void Rfm9XSimModel::advanceClock(FwSizeType byteTimes) {
    const U8 mode = this->m_registers[Reg::OP_MODE] & Mode::MASK;
    for (FwSizeType t = 0; t < byteTimes; t++) {
        // Oscillator/PLL settling: no packet engine activity until settled
        if (this->m_modeSettleRemaining > 0) {
            this->m_modeSettleRemaining--;
            continue;
        }
        // The air interface runs slower than the SPI clock
        this->m_airClock++;
        if ((this->m_airClock % AIR_CLOCK_DIVISOR) != 0) {
            continue;
        }
        if (mode == Mode::TX) {
            if (this->m_txActive && (this->m_txElapsed < this->m_txExpected)) {
                this->m_txElapsed++;
                if (this->m_txElapsed >= this->m_txExpected) {
                    if (this->m_txDoneStall) {
                        // Leave TX active and TxDone clear until the
                        // manager's timeout returns the device to RX.
                        continue;
                    }
                    this->completeTransmit();
                }
            }
        } else if (mode == Mode::RX_CONTINUOUS) {
            if (!this->m_rxActive && !this->m_rxPendingDelivery) {
                // Each over-the-air frame is preceded by preamble and sync
                // byte times during which no payload bytes arrive
                if (this->m_rxGapRemaining > 0) {
                    this->m_rxGapRemaining--;
                } else {
                    this->startReceive();
                }
            }
            if (this->m_rxActive) {
                this->m_rxDelivered++;
                if (this->m_rxDelivered >= this->m_rxTotal) {
                    // datasheet section 4.1.5: "Upon reception of a valid
                    // header CRC the RxDone interrupt is set." RxDone and
                    // PayloadCrcError are independent; a corrupt frame still
                    // completes RxDone but also flags PayloadCrcError.
                    this->m_rxActive = false;
                    this->m_rxPendingDelivery = true;
                    this->m_irqFlags |= IrqFlags::RX_DONE;
                    if (this->m_rxCorrupt) {
                        this->m_irqFlags |= IrqFlags::PAYLOAD_CRC_ERROR;
                    }
                    // RegFifoRxCurrentAddr latches the start of the just-received
                    // packet (datasheet section 6.4, RegFifoRxCurrentAddr).
                    this->m_registers[Reg::FIFO_RX_CURRENT_ADDR] = this->m_rxStartAddr;
                    this->m_registers[Reg::RX_NB_BYTES] = static_cast<U8>(this->m_rxTotal);
                    (void)::memcpy(&this->m_fifoBuffer[this->m_rxStartAddr], this->m_rxPacket, this->m_rxTotal);
                }
            }
        }
    }
}

U8 Rfm9XSimModel::readRegister(U8 address) {
    U8 value = 0;
    switch (address) {
        case Reg::FIFO: {
            const U8 ptr = this->m_registers[Reg::FIFO_ADDR_PTR];
            value = this->m_fifoBuffer[ptr];
            this->m_registers[Reg::FIFO_ADDR_PTR] = static_cast<U8>(ptr + 1);
            break;
        }
        case Reg::IRQ_FLAGS:
            value = this->m_irqFlags;
            break;
        case Reg::MODEM_STAT:
            value = this->modemStat();
            break;
        case Reg::PKT_RSSI_VALUE:
            value = this->m_rssiReadback;
            break;
        case Reg::PKT_SNR_VALUE:
            value = this->m_snrReadback;
            break;
        default:
            value = this->m_registers[address & SPI_ADDRESS_MASK];
            break;
    }
    return value;
}

void Rfm9XSimModel::writeRegister(U8 address, U8 value) {
    const U8 normalizedAddress = address & SPI_ADDRESS_MASK;
    if ((normalizedAddress != Reg::FIFO) && (this->m_registerWriteCount < REGISTER_WRITE_HISTORY_SIZE)) {
        this->m_registerWriteAddresses[this->m_registerWriteCount] = normalizedAddress;
        this->m_registerWriteValues[this->m_registerWriteCount] = value;
        this->m_registerWriteCount++;
    }
    switch (address) {
        case Reg::FIFO: {
            const U8 ptr = this->m_registers[Reg::FIFO_ADDR_PTR];
            this->m_fifoBuffer[ptr] = value;
            this->m_registers[Reg::FIFO_ADDR_PTR] = static_cast<U8>(ptr + 1);
            break;
        }
        case Reg::IRQ_FLAGS:
            // Write 1 to clear (datasheet section 6.4, RegIrqFlags "rc" access).
            this->m_irqFlags = static_cast<U8>(this->m_irqFlags & static_cast<U8>(~value));
            // Acknowledging RxDone frees the FIFO for the next reception.
            if ((value & IrqFlags::RX_DONE) != 0) {
                this->m_rxPendingDelivery = false;
                this->m_rxCorrupt = false;
                this->m_rxGapRemaining = RX_INTER_PACKET_GAP;
            }
            break;
        case Reg::OP_MODE: {
            const U8 previous = this->m_registers[Reg::OP_MODE] & Mode::MASK;
            this->m_registers[Reg::OP_MODE] = value;
            if ((value & Mode::MASK) != previous) {
                this->handleModeChange(value & Mode::MASK);
            }
            break;
        }
        default:
            this->m_registers[address & SPI_ADDRESS_MASK] = value;
            break;
    }
}

void Rfm9XSimModel::handleModeChange(U8 mode) {
    this->m_modeSettleRemaining = this->m_modeSettleByteTimes;
    if (mode == Mode::TX) {
        // Entering TX starts clocking out whatever is staged at FifoTxBaseAddr
        // (datasheet section 4.1.5: "Data transmission is initiated by sending
        // TX mode request"). Leaving RX abandons any in-progress reception.
        this->m_rxActive = false;
        this->startTransmit();
    } else if (mode == Mode::RX_CONTINUOUS) {
        // datasheet section 4.1.5: "Upon completion the TxDone interrupt is
        // issued and the radio returns to Stand-by mode" -- any TX state is
        // abandoned if the host instead commands RX directly.
        this->m_txActive = false;
        this->startReceive();
    } else {
        this->m_txActive = false;
        this->m_rxActive = false;
    }
}

void Rfm9XSimModel::startTransmit() {
    // datasheet section 4.1.5: "Set FifoPtrAddr to FifoTxPtrBase. Write
    // PayloadLength bytes to the FIFO." The manager has already done this by
    // the time TX mode is requested; the payload is staged at FifoTxBaseAddr.
    const U8 base = this->m_registers[Reg::FIFO_TX_BASE_ADDR];
    const FwSizeType length = FW_MIN(static_cast<FwSizeType>(this->m_registers[Reg::PAYLOAD_LENGTH]),
                                     sizeof this->m_txAccum);
    (void)::memcpy(this->m_txAccum, &this->m_fifoBuffer[base], length);
    this->m_txExpected = length;
    this->m_txElapsed = 0;
    this->m_txActive = true;
    if (length == 0) {
        this->completeTransmit();
    }
}

void Rfm9XSimModel::completeTransmit() {
    if (this->m_txCount < TX_QUEUE_DEPTH) {
        (void)::memcpy(this->m_txQueue[this->m_txCount], this->m_txAccum, this->m_txExpected);
        this->m_txSizes[this->m_txCount] = this->m_txExpected;
        this->m_txCount++;
    }
    this->m_txActive = false;
    this->m_irqFlags |= IrqFlags::TX_DONE;
    // datasheet section 4.1.5: "Upon completion the TxDone interrupt is issued
    // and the radio returns to Stand-by mode."
    this->m_registers[Reg::OP_MODE] =
        static_cast<U8>((this->m_registers[Reg::OP_MODE] & static_cast<U8>(~Mode::MASK)) | Mode::STANDBY);
}

void Rfm9XSimModel::startReceive() {
    // Begin delivery only when in RX, idle, with no undelivered packet pending
    if (((this->m_registers[Reg::OP_MODE] & Mode::MASK) != Mode::RX_CONTINUOUS) || this->m_rxActive ||
        this->m_rxPendingDelivery || (this->m_airCount == 0)) {
        return;
    }
    const FwSizeType payloadSize = FW_MIN(this->m_airCount, MAX_PACKET_PAYLOAD);
    (void)::memcpy(this->m_rxPacket, this->m_airBuffer, payloadSize);
    this->m_rxTotal = payloadSize;
    this->m_rxDelivered = 0;
    this->m_rxActive = true;
    this->m_rxCorrupt = false;
    this->m_rxStartAddr = this->m_registers[Reg::FIFO_RX_BASE_ADDR];
    (void)::memmove(this->m_airBuffer, &this->m_airBuffer[payloadSize], this->m_airCount - payloadSize);
    this->m_airCount -= payloadSize;
}

U8 Rfm9XSimModel::modemStat() const {
    U8 stat = ModemStat::MODEM_CLEAR;
    const U8 mode = this->m_registers[Reg::OP_MODE] & Mode::MASK;
    if (mode == Mode::RX_CONTINUOUS) {
        if (this->m_rxActive || this->m_rxPendingDelivery || this->m_channelBusy) {
            stat = static_cast<U8>(ModemStat::RX_ONGOING | ModemStat::SIGNAL_DETECTED |
                                   ModemStat::SIGNAL_SYNCHRONIZED);
        }
    }
    return stat;
}

}  // namespace Rfm9X