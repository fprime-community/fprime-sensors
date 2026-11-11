// ======================================================================
// \title  Rfm9XSimModelTestMain.cpp
// \brief  Register-level tests proving Rfm9XSimModel behaves like the
//         RFM9X (SX1276/8) LoRa radio visible over SPI (datasheet).
//
// The manager unit tests use this model as their radio; these tests
// validate the model itself so manager results are trustworthy.
// ======================================================================

#include <gtest/gtest.h>
#include <cstring>
#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.hpp"

namespace Rfm9X {

class Rfm9XSimModelTest : public ::testing::Test {
  protected:
    //! Single-register read as the manager performs it (address, dummy)
    U8 readReg(U8 address) {
        U8 mosi[2] = {static_cast<U8>(address & SPI_ADDRESS_MASK), 0};
        U8 miso[2] = {0xFF, 0xFF};
        this->model.spiTransaction(mosi, miso, sizeof mosi);
        return miso[1];
    }

    //! Single-register write as the manager performs it (address|wnr, value)
    void writeReg(U8 address, U8 value) {
        U8 mosi[2] = {static_cast<U8>((address & SPI_ADDRESS_MASK) | SPI_WRITE_FLAG), value};
        U8 miso[2] = {0, 0};
        this->model.spiTransaction(mosi, miso, sizeof mosi);
    }

    //! Enter a mode field value, preserving LongRangeMode, and clock past
    //! any modeled settling time
    void enterMode(U8 mode, FwSizeType settleClocks = 0) {
        this->writeReg(Reg::OP_MODE, static_cast<U8>(Mode::LONG_RANGE_MODE | mode));
        for (FwSizeType i = 0; i < settleClocks; i++) {
            (void)this->readReg(Reg::IRQ_FLAGS);
        }
    }

    //! Stage a payload at FifoTxBaseAddr and set PayloadLength, as the
    //! manager's TX_SET_FIFO_PTR/TX_WRITE_PAYLOAD states do
    void stageTxPayload(const U8* data, FwSizeType size) {
        this->writeReg(Reg::FIFO_TX_BASE_ADDR, 0x00);
        this->writeReg(Reg::PAYLOAD_LENGTH, static_cast<U8>(size));
        this->writeReg(Reg::FIFO_ADDR_PTR, 0x00);
        for (FwSizeType i = 0; i < size; i++) {
            this->writeReg(Reg::FIFO, data[i]);
        }
    }

    //! Poll IRQ_FLAGS until the given bit sets or the bound is reached
    U8 pollIrqUntil(U8 bit, FwSizeType bound) {
        U8 flags = 0;
        for (FwSizeType i = 0; (i < bound) && ((flags & bit) == 0); i++) {
            flags = this->readReg(Reg::IRQ_FLAGS);
        }
        return flags;
    }

    Rfm9XSimModel model;
};

// ----------------------------------------------------------------------
// Register file semantics
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, ResetRegisterDefaults) {
    // datasheet section 6.1/6.4 power-on values the manager relies on
    EXPECT_EQ(this->readReg(Reg::VERSION), VERSION_VALUE);
    EXPECT_EQ(this->readReg(Reg::OP_MODE) & Mode::LONG_RANGE_MODE, Mode::LONG_RANGE_MODE);
    EXPECT_EQ(this->readReg(Reg::OP_MODE) & Mode::MASK, Mode::STANDBY);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FRF_MSB), 0x6C);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FRF_MID), 0x80);
    EXPECT_EQ(this->readReg(Reg::PKT_RSSI_VALUE), static_cast<U8>(Rfm9XSimModel::RSSI_READBACK));
    EXPECT_EQ(this->readReg(Reg::PKT_SNR_VALUE), static_cast<U8>(Rfm9XSimModel::SNR_READBACK));
}

TEST_F(Rfm9XSimModelTest, SingleWriteReadBack) {
    this->writeReg(Reg::MODEM_CONFIG_1, 0x72);
    EXPECT_EQ(this->readReg(Reg::MODEM_CONFIG_1), 0x72);
    EXPECT_EQ(this->model.readRegisterValue(Reg::MODEM_CONFIG_1), 0x72);
}

TEST_F(Rfm9XSimModelTest, BurstWriteAutoIncrements) {
    // Burst access auto-increments the 7-bit SPI address (datasheet section 4.3)
    U8 mosi[4] = {static_cast<U8>(Reg::FRF_MSB | SPI_WRITE_FLAG), 0x6C, 0x80, 0x12};
    U8 miso[4] = {0};
    this->model.spiTransaction(mosi, miso, sizeof mosi);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FRF_MSB), 0x6C);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FRF_MID), 0x80);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FRF_LSB), 0x12);
}

TEST_F(Rfm9XSimModelTest, BurstReadAutoIncrements) {
    this->writeReg(Reg::FRF_MSB, 0xE4);
    this->writeReg(Reg::FRF_MID, 0xC0);
    this->writeReg(Reg::FRF_LSB, 0x12);
    U8 mosi[4] = {Reg::FRF_MSB, 0, 0, 0};
    U8 miso[4] = {0};
    this->model.spiTransaction(mosi, miso, sizeof mosi);
    EXPECT_EQ(miso[1], 0xE4);
    EXPECT_EQ(miso[2], 0xC0);
    EXPECT_EQ(miso[3], 0x12);
}

TEST_F(Rfm9XSimModelTest, RegisterWriteHistory) {
    this->model.clearRegisterWriteHistory();
    this->writeReg(Reg::PA_CONFIG, 0xCF);
    EXPECT_TRUE(this->model.wasRegisterWritten(Reg::PA_CONFIG, 0xCF));
    EXPECT_FALSE(this->model.wasRegisterWritten(Reg::PA_CONFIG, 0x00));
    this->model.clearRegisterWriteHistory();
    EXPECT_FALSE(this->model.wasRegisterWritten(Reg::PA_CONFIG, 0xCF));
}

TEST_F(Rfm9XSimModelTest, RegisterWriteHistoryBounded) {
    this->model.clearRegisterWriteHistory();
    // Overflow the bounded history; the earliest writes are retained
    for (FwSizeType i = 0; i < (Rfm9XSimModel::REGISTER_WRITE_HISTORY_SIZE + 8); i++) {
        this->writeReg(Reg::PA_CONFIG, static_cast<U8>(i));
    }
    EXPECT_TRUE(this->model.wasRegisterWritten(Reg::PA_CONFIG, 0));
    EXPECT_FALSE(
        this->model.wasRegisterWritten(Reg::PA_CONFIG, static_cast<U8>(Rfm9XSimModel::REGISTER_WRITE_HISTORY_SIZE + 4)));
}

// ----------------------------------------------------------------------
// Dual-pointer FIFO semantics (datasheet section 4.1.2)
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, FifoAddrPtrAutoIncrementsAndWraps) {
    this->writeReg(Reg::FIFO_ADDR_PTR, 0x00);
    this->writeReg(Reg::FIFO, 0x11);
    this->writeReg(Reg::FIFO, 0x22);
    this->writeReg(Reg::FIFO, 0x33);
    EXPECT_EQ(this->model.readRegisterValue(Reg::FIFO_ADDR_PTR), 0x03);
    this->writeReg(Reg::FIFO_ADDR_PTR, 0x00);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0x11);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0x22);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0x33);
}

TEST_F(Rfm9XSimModelTest, FifoAccessDoesNotIncrementOuterSpiAddress) {
    // Multi-byte FIFO access advances FifoAddrPtr, not the outer SPI address
    // (datasheet section 4.1.2 vs. burst-access auto-increment, section 4.3).
    this->writeReg(Reg::FIFO_ADDR_PTR, 0x05);
    U8 mosi[4] = {static_cast<U8>(Reg::FIFO | SPI_WRITE_FLAG), 0xAA, 0xBB, 0xCC};
    U8 miso[4] = {0};
    this->model.spiTransaction(mosi, miso, sizeof mosi);
    this->writeReg(Reg::FIFO_ADDR_PTR, 0x05);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0xAA);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0xBB);
    EXPECT_EQ(this->readReg(Reg::FIFO), 0xCC);
}

// ----------------------------------------------------------------------
// Mode transitions and settling
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, ModeFieldReadbackPreservesLongRangeMode) {
    this->enterMode(Mode::RX_CONTINUOUS);
    EXPECT_EQ(this->readReg(Reg::OP_MODE) & Mode::MASK, Mode::RX_CONTINUOUS);
    EXPECT_NE(this->readReg(Reg::OP_MODE) & Mode::LONG_RANGE_MODE, 0);
    this->enterMode(Mode::STANDBY);
    EXPECT_EQ(this->readReg(Reg::OP_MODE) & Mode::MASK, Mode::STANDBY);
    EXPECT_NE(this->readReg(Reg::OP_MODE) & Mode::LONG_RANGE_MODE, 0);
}

TEST_F(Rfm9XSimModelTest, ModeSettleDelaysPacketEngineActivity) {
    // Oscillator/PLL settling withholds packet-engine byte-clock activity
    this->model.setModeSettleByteTimes(16);
    this->enterMode(Mode::RX_CONTINUOUS);
    const U8 uplink[4] = {0x01, 0x02, 0x03, 0x04};
    this->model.injectAirData(uplink, sizeof uplink);
    // A few reads fall entirely within the settle window
    for (FwSizeType i = 0; i < 4; i++) {
        EXPECT_EQ(this->readReg(Reg::IRQ_FLAGS) & IrqFlags::RX_DONE, 0);
    }
    const U8 flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    EXPECT_NE(flags & IrqFlags::RX_DONE, 0);
}

// ----------------------------------------------------------------------
// Transmit path (datasheet section 4.1.5, Data Transmission Sequence)
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, TransmitQueuesPacketOnTxDone) {
    const U8 payload[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    this->stageTxPayload(payload, sizeof payload);
    this->enterMode(Mode::TX);
    const U8 flags = this->pollIrqUntil(IrqFlags::TX_DONE, 64);
    EXPECT_NE(flags & IrqFlags::TX_DONE, 0);
    U8 out[MAX_PACKET_PAYLOAD] = {0};
    const FwSizeType size = this->model.retrievePacket(out, sizeof out);
    ASSERT_EQ(size, sizeof payload);
    EXPECT_EQ(::memcmp(out, payload, size), 0);
}

TEST_F(Rfm9XSimModelTest, ModeReturnsToStandbyAfterTxDone) {
    // datasheet section 4.1.5: "Upon completion the TxDone interrupt is
    // issued and the radio returns to Stand-by mode."
    const U8 payload[2] = {0xAA, 0xBB};
    this->stageTxPayload(payload, sizeof payload);
    this->enterMode(Mode::TX);
    ASSERT_NE(this->pollIrqUntil(IrqFlags::TX_DONE, 64) & IrqFlags::TX_DONE, 0);
    EXPECT_EQ(this->model.readRegisterValue(Reg::OP_MODE) & Mode::MASK, Mode::STANDBY);
}

TEST_F(Rfm9XSimModelTest, TxDoneStall) {
    this->model.setTxDoneStall(true);
    const U8 payload[2] = {0xAA, 0xBB};
    this->stageTxPayload(payload, sizeof payload);
    this->enterMode(Mode::TX);
    U8 flags = 0;
    for (FwSizeType i = 0; i < 64; i++) {
        flags = this->readReg(Reg::IRQ_FLAGS);
    }
    // TxDone never asserts and no packet is queued
    EXPECT_EQ(flags & IrqFlags::TX_DONE, 0);
    U8 out[MAX_PACKET_PAYLOAD];
    EXPECT_EQ(this->model.retrievePacket(out, sizeof out), 0u);
}

TEST_F(Rfm9XSimModelTest, MultipleQueuedTransmits) {
    for (U8 packet = 0; packet < 3; packet++) {
        this->enterMode(Mode::STANDBY);
        const U8 payload[1] = {static_cast<U8>(0xC0 + packet)};
        this->stageTxPayload(payload, sizeof payload);
        this->enterMode(Mode::TX);
        ASSERT_NE(this->pollIrqUntil(IrqFlags::TX_DONE, 64) & IrqFlags::TX_DONE, 0);
        // Acknowledge TxDone so the next loop iteration's wait is meaningful
        this->writeReg(Reg::IRQ_FLAGS, IrqFlags::TX_DONE);
    }
    U8 out[MAX_PACKET_PAYLOAD];
    for (U8 packet = 0; packet < 3; packet++) {
        ASSERT_EQ(this->model.retrievePacket(out, sizeof out), 1u);
        EXPECT_EQ(out[0], static_cast<U8>(0xC0 + packet));
    }
    EXPECT_EQ(this->model.retrievePacket(out, sizeof out), 0u);
}

TEST_F(Rfm9XSimModelTest, TransmitQueueOverflowDropsExcess) {
    // Fill the retrieval queue past its depth: extra packets are dropped
    for (FwSizeType packet = 0; packet < (Rfm9XSimModel::TX_QUEUE_DEPTH + 2); packet++) {
        this->enterMode(Mode::STANDBY);
        const U8 payload[1] = {static_cast<U8>(packet)};
        this->stageTxPayload(payload, sizeof payload);
        this->enterMode(Mode::TX);
        ASSERT_NE(this->pollIrqUntil(IrqFlags::TX_DONE, 64) & IrqFlags::TX_DONE, 0);
        this->writeReg(Reg::IRQ_FLAGS, IrqFlags::TX_DONE);
    }
    U8 out[MAX_PACKET_PAYLOAD];
    for (FwSizeType packet = 0; packet < Rfm9XSimModel::TX_QUEUE_DEPTH; packet++) {
        ASSERT_EQ(this->model.retrievePacket(out, sizeof out), 1u);
        EXPECT_EQ(out[0], static_cast<U8>(packet));
    }
    EXPECT_EQ(this->model.retrievePacket(out, sizeof out), 0u);
}

// ----------------------------------------------------------------------
// Receive path (datasheet section 4.1.5, Data Reception Sequence)
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, ReceiveSmallPacket) {
    this->enterMode(Mode::RX_CONTINUOUS);
    const U8 uplink[4] = {0x01, 0x02, 0x03, 0x04};
    this->model.injectAirData(uplink, sizeof uplink);
    const U8 flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    EXPECT_EQ(flags & IrqFlags::PAYLOAD_CRC_ERROR, 0);
    EXPECT_EQ(this->readReg(Reg::RX_NB_BYTES), sizeof uplink);
    const U8 currentAddr = this->readReg(Reg::FIFO_RX_CURRENT_ADDR);
    this->writeReg(Reg::FIFO_ADDR_PTR, currentAddr);
    for (FwSizeType i = 0; i < sizeof uplink; i++) {
        EXPECT_EQ(this->readReg(Reg::FIFO), uplink[i]);
    }
}

TEST_F(Rfm9XSimModelTest, ChannelBusyReflectedInModemStat) {
    this->enterMode(Mode::RX_CONTINUOUS);
    EXPECT_EQ(this->readReg(Reg::MODEM_STAT) & ModemStat::RX_ONGOING, 0);
    this->model.setChannelBusy(true);
    EXPECT_NE(this->readReg(Reg::MODEM_STAT) & ModemStat::RX_ONGOING, 0);
    EXPECT_NE(this->readReg(Reg::MODEM_STAT) & ModemStat::SIGNAL_DETECTED, 0);
    this->model.setChannelBusy(false);
    EXPECT_EQ(this->readReg(Reg::MODEM_STAT) & ModemStat::RX_ONGOING, 0);
}

TEST_F(Rfm9XSimModelTest, ReceiveActiveReflectedInModemStat) {
    this->enterMode(Mode::RX_CONTINUOUS);
    U8 uplink[100] = {0x55};
    this->model.injectAirData(uplink, sizeof uplink);
    (void)this->readReg(Reg::IRQ_FLAGS);  // clock a few bytes in
    EXPECT_NE(this->readReg(Reg::MODEM_STAT) & ModemStat::RX_ONGOING, 0);
}

TEST_F(Rfm9XSimModelTest, CorruptFrameAssertsRxDoneAndPayloadCrcError) {
    // datasheet section 4.1.5: RxDone and PayloadCrcError are independent;
    // the manager checks PayloadCrcError before trusting the payload.
    this->enterMode(Mode::RX_CONTINUOUS);
    this->model.injectCorruptFrame(24);
    const U8 flags = this->pollIrqUntil(IrqFlags::RX_DONE, 128);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    EXPECT_NE(flags & IrqFlags::PAYLOAD_CRC_ERROR, 0);
}

TEST_F(Rfm9XSimModelTest, AcknowledgingRxDoneAllowsNextReception) {
    this->enterMode(Mode::RX_CONTINUOUS);
    const U8 first[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    this->model.injectAirData(first, sizeof first);
    U8 flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    // Acknowledge (clear) RxDone, as the manager does after draining the FIFO
    this->writeReg(Reg::IRQ_FLAGS, static_cast<U8>(IrqFlags::RX_DONE | IrqFlags::PAYLOAD_CRC_ERROR));
    EXPECT_EQ(this->readReg(Reg::IRQ_FLAGS) & IrqFlags::RX_DONE, 0);
    // A following packet is received normally
    const U8 second[3] = {0xA1, 0xA2, 0xA3};
    this->model.injectAirData(second, sizeof second);
    flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    EXPECT_EQ(this->readReg(Reg::RX_NB_BYTES), sizeof second);
}

TEST_F(Rfm9XSimModelTest, ReceiveMultipleQueuedPackets) {
    this->enterMode(Mode::RX_CONTINUOUS);
    U8 first[20];
    for (FwSizeType i = 0; i < sizeof first; i++) {
        first[i] = static_cast<U8>(i + 1);
    }
    this->model.injectAirData(first, sizeof first);
    U8 flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    EXPECT_EQ(this->readReg(Reg::RX_NB_BYTES), sizeof first);
    const U8 second[5] = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5};
    this->model.injectAirData(second, sizeof second);
    // A second reception cannot begin until RxDone is acknowledged
    this->writeReg(Reg::IRQ_FLAGS, IrqFlags::RX_DONE);
    flags = this->pollIrqUntil(IrqFlags::RX_DONE, 64);
    ASSERT_NE(flags & IrqFlags::RX_DONE, 0);
    EXPECT_EQ(this->readReg(Reg::RX_NB_BYTES), sizeof second);
    const U8 currentAddr = this->readReg(Reg::FIFO_RX_CURRENT_ADDR);
    this->writeReg(Reg::FIFO_ADDR_PTR, currentAddr);
    for (FwSizeType i = 0; i < sizeof second; i++) {
        EXPECT_EQ(this->readReg(Reg::FIFO), second[i]);
    }
}

TEST_F(Rfm9XSimModelTest, InvalidSpiTransactionsIgnored) {
    U8 mosi[2] = {Reg::VERSION, 0};
    U8 miso[2] = {0xAA, 0xAA};
    // Null pointers and zero size must be safely ignored
    this->model.spiTransaction(nullptr, miso, sizeof miso);
    this->model.spiTransaction(mosi, nullptr, sizeof mosi);
    this->model.spiTransaction(mosi, miso, 0);
    EXPECT_EQ(miso[1], 0xAA);  // untouched
    // The model still functions normally afterwards
    EXPECT_EQ(this->readReg(Reg::VERSION), VERSION_VALUE);
}

// ----------------------------------------------------------------------
// Fault injection controls
// ----------------------------------------------------------------------

TEST_F(Rfm9XSimModelTest, AbsentRadioReadsZero) {
    this->model.setRadioPresent(false);
    EXPECT_EQ(this->readReg(Reg::VERSION), 0);
    this->writeReg(Reg::MODEM_CONFIG_1, 0x77);  // latched nothing
    this->model.setRadioPresent(true);
    EXPECT_EQ(this->readReg(Reg::VERSION), VERSION_VALUE);
    // The reset-default value (datasheet section 6.4), not the 0x77 write
    // attempted while the radio was absent.
    EXPECT_EQ(this->readReg(Reg::MODEM_CONFIG_1), 0x72);
}

TEST_F(Rfm9XSimModelTest, RssiReadbackControl) {
    this->model.setRssiReadback(0x61);  // -137 + 0x61 (97) = -40 dBm
    EXPECT_EQ(this->readReg(Reg::PKT_RSSI_VALUE), 0x61);
}

TEST_F(Rfm9XSimModelTest, SnrReadbackControl) {
    this->model.setSnrReadback(0xF4);  // two's complement -12 / 4 = -3 dB
    EXPECT_EQ(this->readReg(Reg::PKT_SNR_VALUE), 0xF4);
}

TEST_F(Rfm9XSimModelTest, ResetRestoresPowerOnState) {
    this->writeReg(Reg::MODEM_CONFIG_1, 0xAB);
    this->enterMode(Mode::RX_CONTINUOUS);
    this->model.reset();
    EXPECT_EQ(this->model.readRegisterValue(Reg::MODEM_CONFIG_1), 0x72);
    EXPECT_EQ(this->model.readRegisterValue(Reg::OP_MODE) & Mode::MASK, Mode::STANDBY);
    EXPECT_EQ(this->readReg(Reg::VERSION), VERSION_VALUE);
}

}  // namespace Rfm9X

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
