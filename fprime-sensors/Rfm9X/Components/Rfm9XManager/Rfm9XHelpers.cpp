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
// Register access primitives
// ----------------------------------------------------------------------

Drv::SpiStatus Rfm9XManager ::readRegister(U8 address, U8& value) {
    // Single access read: address byte (wnr=0) then one dummy byte
    this->m_mosi[0] = address & SPI_ADDRESS_MASK;
    this->m_mosi[1] = 0;
    Fw::Buffer writeBuffer(this->m_mosi, 2);
    Fw::Buffer readBuffer(this->m_miso, 2);
    const Drv::SpiStatus status = this->spiWriteRead_out(0, writeBuffer, readBuffer);
    value = this->m_miso[1];
    return status;
}

Drv::SpiStatus Rfm9XManager ::writeRegister(U8 address, U8 value) {
    // Single access write: address byte with wnr=1 then the data byte
    this->m_mosi[0] = (address & SPI_ADDRESS_MASK) | SPI_WRITE_FLAG;
    this->m_mosi[1] = value;
    Fw::Buffer writeBuffer(this->m_mosi, 2);
    Fw::Buffer readBuffer(this->m_miso, 2);
    return this->spiWriteRead_out(0, writeBuffer, readBuffer);
}

// ----------------------------------------------------------------------
// Radio management
// ----------------------------------------------------------------------

bool Rfm9XManager ::detectRadio() {
    U8 version = 0;
    const Drv::SpiStatus status = this->readRegister(Reg::VERSION, version);
    return (status == Drv::SpiStatus::SPI_OK) && (version == VERSION_VALUE);
}

bool Rfm9XManager ::enterLoRaMode() {
    // datasheet section 6.4, RegOpMode bit 7 (LongRangeMode): "This bit can be
    // modified only in Sleep mode. A write operation on other device modes
    // is ignored." Force SLEEP first, then set LongRangeMode in the same
    // (now-safe) mode, then move to STANDBY for configuration.
    if (this->writeRegister(Reg::OP_MODE, Mode::SLEEP) != Drv::SpiStatus::SPI_OK) {
        return false;
    }
    if (this->writeRegister(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::SLEEP)) !=
        Drv::SpiStatus::SPI_OK) {
        return false;
    }
    return this->writeRegister(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | Mode::STANDBY)) ==
           Drv::SpiStatus::SPI_OK;
}

bool Rfm9XManager ::configureRadio() {
    // Read operator params at configure time (same pattern as the prior FSK
    // profile). Parameter validity is ground-managed input: fail into
    // ConfigurationFailed and retry on the next tick rather than asserting.
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const SpreadingFactor sfParam = this->paramGet_SPREADING_FACTOR(isValid);
    if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
        return false;
    }
    const Bandwidth bwParam = this->paramGet_BANDWIDTH(isValid);
    if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
        return false;
    }
    const CodingRate crParam = this->paramGet_CODING_RATE(isValid);
    if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
        return false;
    }
    const U32 freqParam = this->paramGet_FREQUENCY_HZ(isValid);
    if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
        return false;
    }
    const Rfm9XTxPower txPowerParam = this->paramGet_TX_POWER(isValid);
    if ((isValid != Fw::ParamValid::VALID) && (isValid != Fw::ParamValid::DEFAULT)) {
        return false;
    }

    U8 sfReg = 0;
    U8 bwReg = 0;
    U8 crReg = 0;
    TxPowerSetting power{};
    if (!getSpreadingFactorReg(sfParam, sfReg) || !getBandwidthReg(bwParam, bwReg) ||
        !getCodingRateReg(crParam, crReg) || !getTxPowerSetting(txPowerParam, power)) {
        return false;
    }

    // SLEEP -> LongRangeMode -> STANDBY (datasheet section 6.4 RegOpMode note).
    if (!this->enterLoRaMode()) {
        return false;
    }

    const U32 frf = frequencyToFrf(freqParam);
    const struct {
        U8 address;
        U8 value;
    } configuration[] = {
        {Reg::FRF_MSB, static_cast<U8>(frf >> 16)},
        {Reg::FRF_MID, static_cast<U8>(frf >> 8)},
        {Reg::FRF_LSB, static_cast<U8>(frf & 0xFF)},
        {Reg::PA_CONFIG, power.paConfig},
        {Reg::OCP, power.ocp},
        {Reg::PA_DAC, power.paDac},
        // RegModemConfig1: Bw | CodingRate | ImplicitHeaderModeOn=0 (explicit header)
        {Reg::MODEM_CONFIG_1, static_cast<U8>(bwReg | crReg)},
        // RegModemConfig2: SpreadingFactor | TxContinuousMode=0 | RxPayloadCrcOn=1 | SymbTimeout(9:8)=0
        {Reg::MODEM_CONFIG_2, static_cast<U8>(static_cast<U8>(sfReg << 4) | 0x04)},
        {Reg::SYMB_TIMEOUT_LSB, 0x64},  // datasheet reset default (100 symbols)
        {Reg::PREAMBLE_MSB, static_cast<U8>(FIXED_PREAMBLE_LENGTH >> 8)},
        {Reg::PREAMBLE_LSB, static_cast<U8>(FIXED_PREAMBLE_LENGTH & 0xFF)},
        {Reg::MAX_PAYLOAD_LENGTH, static_cast<U8>(MAX_PACKET_PAYLOAD)},
        {Reg::FIFO_TX_BASE_ADDR, FIFO_BASE_ADDR},
        {Reg::FIFO_RX_BASE_ADDR, FIFO_BASE_ADDR},
        {Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE},  // Default to RX mapping; TX flips this before each send
    };
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(configuration); i++) {
        if (this->writeRegister(configuration[i].address, configuration[i].value) != Drv::SpiStatus::SPI_OK) {
            return false;
        }
    }
    // Clear any IRQ residue from before configuration (write 1 to clear, datasheet section 6.4)
    return this->writeRegister(Reg::IRQ_FLAGS, 0xFF) == Drv::SpiStatus::SPI_OK;
}

bool Rfm9XManager ::requestMode(U8 mode) {
    // LongRangeMode (bit 7) must stay set once configured; only the mode
    // field (bits 2-0) changes here.
    const U8 opMode = static_cast<U8>(Mode::LONG_RANGE_MODE | (mode & Mode::MASK));
    return this->writeRegister(Reg::OP_MODE, opMode) == Drv::SpiStatus::SPI_OK;
}

bool Rfm9XManager ::channelBusy() {
    // LoRa has no SyncAddressMatch-equivalent IRQ bit; RegModemStat reports
    // live demodulator activity (datasheet section 6.4, RegModemStat 0x18).
    U8 stat = 0;
    if (this->readRegister(Reg::MODEM_STAT, stat) != Drv::SpiStatus::SPI_OK) {
        // A bus fault reads as "clear" so the transmit path is taken and its
        // own SPI failure handling reports RADIO_FAULT instead of silently
        // deferring downlink forever on a dead bus.
        return false;
    }
    return (stat & static_cast<U8>(ModemStat::RX_ONGOING | ModemStat::SIGNAL_DETECTED)) != 0;
}

bool Rfm9XManager ::downlinkBlocked() {
    return this->channelBusy() || (this->m_rxTxHoldoffTicks > 0) || (this->m_txTxHoldoffTicks > 0);
}

// ----------------------------------------------------------------------
// Packet handling
// ----------------------------------------------------------------------

bool Rfm9XManager ::writeFifo(const U8* data, FwSizeType size) {
    FW_ASSERT(data != nullptr);
    FW_ASSERT((size + 1) <= sizeof this->m_mosi, static_cast<FwAssertArgType>(size));
    this->m_mosi[0] = Reg::FIFO | SPI_WRITE_FLAG;
    (void)::memcpy(&this->m_mosi[1], data, size);
    Fw::Buffer writeBuffer(this->m_mosi, size + 1);
    Fw::Buffer readBuffer(this->m_miso, size + 1);
    return this->spiWriteRead_out(0, writeBuffer, readBuffer) == Drv::SpiStatus::SPI_OK;
}

bool Rfm9XManager ::readFifo(U8* data, FwSizeType size) {
    FW_ASSERT(data != nullptr);
    FW_ASSERT((size + 1) <= sizeof this->m_mosi, static_cast<FwAssertArgType>(size));
    this->m_mosi[0] = Reg::FIFO;
    (void)::memset(&this->m_mosi[1], 0, size);
    Fw::Buffer writeBuffer(this->m_mosi, size + 1);
    Fw::Buffer readBuffer(this->m_miso, size + 1);
    if (this->spiWriteRead_out(0, writeBuffer, readBuffer) != Drv::SpiStatus::SPI_OK) {
        return false;
    }
    (void)::memcpy(data, &this->m_miso[1], size);
    return true;
}

void Rfm9XManager ::abortTransmit() {
    this->log_WARNING_HI_SendFailed(Rfm9XSendFailure::RADIO_FAULT);
    this->m_txState = TX_ABORT_REQUEST_RX;
}

Rfm9XManager::TxProgress Rfm9XManager ::advanceTransmit() {
    FW_ASSERT(this->m_txActive);
    const FwSizeType size = this->m_txBuffer.getSize();
    const U8* const data = this->m_txBuffer.getData();
    FW_ASSERT(data != nullptr);
    FW_ASSERT((size > 0) && (size <= MAX_PACKET_PAYLOAD), static_cast<FwAssertArgType>(size));

    U8 flags = 0;
    switch (this->m_txState) {
        case TX_REQUEST_STANDBY:
            if (!this->requestMode(Mode::STANDBY)) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_SET_FIFO_PTR;
            }
            break;

        case TX_SET_FIFO_PTR:
            // LoRa Transmit Data FIFO Filling (datasheet section 4.1.5):
            // "Set FifoPtrAddr to FifoTxPtrBase. Write PayloadLength bytes to FIFO."
            if ((this->writeRegister(Reg::FIFO_ADDR_PTR, FIFO_BASE_ADDR) != Drv::SpiStatus::SPI_OK) ||
                (this->writeRegister(Reg::PAYLOAD_LENGTH, static_cast<U8>(size)) != Drv::SpiStatus::SPI_OK) ||
                (this->writeRegister(Reg::DIO_MAPPING_1, Dio0Mapping::TX_DONE) != Drv::SpiStatus::SPI_OK)) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_WRITE_PAYLOAD;
            }
            break;

        case TX_WRITE_PAYLOAD:
            // Single standby fill: the 256-byte LoRa FIFO holds the full
            // 255-byte max payload in one burst (no in-flight top-up needed).
            if (!this->writeFifo(data, size)) {
                this->abortTransmit();
            } else {
                this->m_txState = TX_REQUEST_TX;
            }
            break;

        case TX_REQUEST_TX:
            if (!this->requestMode(Mode::TX)) {
                this->abortTransmit();
            } else {
                this->m_txWaitTicks = 0;
                this->m_txState = TX_WAIT_TXDONE;
            }
            break;

        case TX_WAIT_TXDONE:
            if (this->readRegister(Reg::IRQ_FLAGS, flags) != Drv::SpiStatus::SPI_OK) {
                this->abortTransmit();
            } else if ((flags & IrqFlags::TX_DONE) != 0) {
                (void)this->writeRegister(Reg::IRQ_FLAGS, IrqFlags::TX_DONE);
                this->m_txState = TX_REQUEST_RX;
            } else if (++this->m_txWaitTicks >= TX_DONE_TIMEOUT_TICKS) {
                this->abortTransmit();
            }
            break;

        case TX_REQUEST_RX:
            if ((this->writeRegister(Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE) != Drv::SpiStatus::SPI_OK) ||
                !this->requestMode(Mode::RX_CONTINUOUS)) {
                this->abortTransmit();
                break;
            }
            this->m_packetsTransmitted++;
            this->tlmWrite_PacketsTransmitted(this->m_packetsTransmitted);
            this->log_WARNING_HI_ConfigurationFailed_ThrottleClear();
            this->log_WARNING_HI_SendFailed_ThrottleClear();
            return TX_SUCCEEDED;

        case TX_ABORT_REQUEST_RX:
            (void)this->writeRegister(Reg::IRQ_FLAGS, 0xFF);
            (void)this->writeRegister(Reg::DIO_MAPPING_1, Dio0Mapping::RX_DONE);
            (void)this->requestMode(Mode::RX_CONTINUOUS);
            return TX_FAILED;

        case TX_IDLE:
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_txState));
            break;
    }
    return TX_IN_PROGRESS;
}

void Rfm9XManager ::updateRssiSnr() {
    U8 rssi = 0;
    if (this->readRegister(Reg::PKT_RSSI_VALUE, rssi) == Drv::SpiStatus::SPI_OK) {
        // RSSI [dBm] = -137 + PacketRssi (datasheet section 6.4, RegPktRssiValue)
        this->tlmWrite_LastPacketRssi(-137.0f + static_cast<F32>(rssi));
    }
    U8 snr = 0;
    if (this->readRegister(Reg::PKT_SNR_VALUE, snr) == Drv::SpiStatus::SPI_OK) {
        // SNR [dB] = PacketSnr (two's complement) / 4 (datasheet section 6.4, RegPktSnrValue)
        this->tlmWrite_LastPacketSnr(static_cast<F32>(static_cast<I8>(snr)) / 4.0f);
    }
}

}  // namespace Rfm9X