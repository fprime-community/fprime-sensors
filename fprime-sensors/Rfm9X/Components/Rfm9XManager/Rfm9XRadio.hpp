// ======================================================================
// \title  Rfm9XRadio.hpp
// \brief  RFM9X (SX1276/8) LoRa register map, modem enum maps
//
// Register addresses, bit fields, and reset values from the HopeRF
// RFM95/96/97/98(W) datasheet, section 6.1 (Register Table Summary)
// and section 6.4 (LoRa Mode Register Map).
// ======================================================================

#ifndef Rfm9X_Rfm9XRadio_HPP
#define Rfm9X_Rfm9XRadio_HPP

#include <Fw/FPrimeBasicTypes.hpp>
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManagerComponentAc.hpp"

namespace Rfm9X {

//! RFM9X LoRa register addresses (datasheet section 6.1/6.4)
namespace Reg {
constexpr U8 FIFO = 0x00;                   //!< LoRa base-band FIFO data input/output
constexpr U8 OP_MODE = 0x01;                //!< Operating mode & LoRa/FSK selection
constexpr U8 FRF_MSB = 0x06;                //!< RF carrier frequency, MSB
constexpr U8 FRF_MID = 0x07;                //!< RF carrier frequency, middle byte
constexpr U8 FRF_LSB = 0x08;                //!< RF carrier frequency, LSB
constexpr U8 PA_CONFIG = 0x09;              //!< PA selection and output power control
constexpr U8 PA_RAMP = 0x0A;                //!< Control of PA ramp time, low phase noise PLL
constexpr U8 OCP = 0x0B;                    //!< Over current protection control
constexpr U8 LNA = 0x0C;                    //!< LNA settings
constexpr U8 FIFO_ADDR_PTR = 0x0D;          //!< SPI interface address pointer in FIFO data buffer
constexpr U8 FIFO_TX_BASE_ADDR = 0x0E;      //!< Write base address in FIFO data buffer for TX modulator
constexpr U8 FIFO_RX_BASE_ADDR = 0x0F;      //!< Read base address in FIFO data buffer for RX demodulator
constexpr U8 FIFO_RX_CURRENT_ADDR = 0x10;   //!< Start address (in data buffer) of last packet received
constexpr U8 IRQ_FLAGS_MASK = 0x11;         //!< Optional IRQ flag mask
constexpr U8 IRQ_FLAGS = 0x12;              //!< LoRa modem IRQ flags
constexpr U8 RX_NB_BYTES = 0x13;            //!< Number of payload bytes of latest packet received
constexpr U8 MODEM_STAT = 0x18;             //!< Live LoRa modem status
constexpr U8 PKT_SNR_VALUE = 0x19;          //!< Estimation of SNR on last packet received
constexpr U8 PKT_RSSI_VALUE = 0x1A;         //!< RSSI of the latest packet received
constexpr U8 RSSI_VALUE = 0x1B;             //!< Current RSSI value
constexpr U8 HOP_CHANNEL = 0x1C;            //!< PLL lock / CRC-on-payload / FHSS channel status
constexpr U8 MODEM_CONFIG_1 = 0x1D;         //!< Bw / CodingRate / ImplicitHeaderModeOn
constexpr U8 MODEM_CONFIG_2 = 0x1E;         //!< SpreadingFactor / TxContinuousMode / RxPayloadCrcOn
constexpr U8 SYMB_TIMEOUT_LSB = 0x1F;       //!< RX time-out LSB
constexpr U8 PREAMBLE_MSB = 0x20;           //!< Preamble length, MSB
constexpr U8 PREAMBLE_LSB = 0x21;           //!< Preamble length, LSB
constexpr U8 PAYLOAD_LENGTH = 0x22;         //!< Payload length in bytes
constexpr U8 MAX_PAYLOAD_LENGTH = 0x23;     //!< Maximum payload length
constexpr U8 MODEM_CONFIG_3 = 0x26;         //!< AgcAutoOn / low data rate optimize
constexpr U8 DIO_MAPPING_1 = 0x40;          //!< Mapping of pins DIO0 to DIO3
constexpr U8 DIO_MAPPING_2 = 0x41;          //!< Mapping of pins DIO4/DIO5
constexpr U8 VERSION = 0x42;                //!< Silicon version
constexpr U8 PA_DAC = 0x4D;                 //!< Higher power settings of the PA
}  // namespace Reg

//! RegOpMode bit fields (datasheet section 6.4). Mode occupies bits 2-0;
//! LongRangeMode is bit 7 and can only be changed while Mode == SLEEP.
namespace Mode {
constexpr U8 LONG_RANGE_MODE = 0x80;  //!< Bit 7: 0=FSK/OOK, 1=LoRa
constexpr U8 SLEEP = 0x00;
constexpr U8 STANDBY = 0x01;
constexpr U8 FSTX = 0x02;
constexpr U8 TX = 0x03;
constexpr U8 FSRX = 0x04;
constexpr U8 RX_CONTINUOUS = 0x05;
constexpr U8 RX_SINGLE = 0x06;
constexpr U8 CAD = 0x07;
constexpr U8 MASK = 0x07;  //!< Mode bits within RegOpMode
}  // namespace Mode

//! RegIrqFlags bit masks (datasheet section 6.4, address 0x12)
namespace IrqFlags {
constexpr U8 RX_TIMEOUT = 0x80;
constexpr U8 RX_DONE = 0x40;
constexpr U8 PAYLOAD_CRC_ERROR = 0x20;
constexpr U8 VALID_HEADER = 0x10;
constexpr U8 TX_DONE = 0x08;
constexpr U8 CAD_DONE = 0x04;
constexpr U8 FHSS_CHANGE_CHANNEL = 0x02;
constexpr U8 CAD_DETECTED = 0x01;
}  // namespace IrqFlags

//! RegModemStat bit masks (datasheet section 6.4, address 0x18). Used as the
//! LoRa channel-busy signal (RxOngoing/SignalDetected), replacing FSK's
//! IrqFlags1.SyncAddressMatch: LoRa has no equivalent IRQ flag, but
//! RegModemStat reports live demodulator activity.
namespace ModemStat {
constexpr U8 MODEM_CLEAR = 0x10;
constexpr U8 HEADER_INFO_VALID = 0x08;
constexpr U8 RX_ONGOING = 0x04;
constexpr U8 SIGNAL_SYNCHRONIZED = 0x02;
constexpr U8 SIGNAL_DETECTED = 0x01;
}  // namespace ModemStat

//! RegDioMapping1 Dio0Mapping field values (bits 7-6), datasheet Table 63
namespace Dio0Mapping {
constexpr U8 RX_DONE = 0x00;
constexpr U8 TX_DONE = 0x40;
constexpr U8 CAD_DONE = 0x80;
constexpr U8 MASK = 0xC0;
}  // namespace Dio0Mapping

//! SPI address byte: write access flag (wnr bit)
constexpr U8 SPI_WRITE_FLAG = 0x80;
//! SPI address byte: register address mask
constexpr U8 SPI_ADDRESS_MASK = 0x7F;

//! Expected RegVersion value for the SX1276-based RFM9X (datasheet section 6.1)
constexpr U8 VERSION_VALUE = 0x11;
//! LoRa FIFO size in bytes, shared between TX and RX via base-address pointers
//! (datasheet section 4.1.2, LoRa Mode FIFO Data Buffer)
constexpr FwSizeType FIFO_SIZE = 256;
//! RFM9X LoRa packet payload maximum (RegPayloadLength is one byte)
constexpr FwSizeType MAX_PACKET_PAYLOAD = 255;
//! Frf register step size: 32 MHz crystal / 2^19 (datasheet section 6.4, RegFrLsb)
constexpr U32 CRYSTAL_HZ = 32000000;
constexpr U32 FRF_DIVISOR = 524288;  //!< 2^19

//! Fixed preamble length (symbols) programmed into RegPreambleMsb/Lsb.
//! The datasheet default length is 8 symbols (reset values 0x00/0x08);
//! kept as the invariant native profile value.
constexpr U16 FIXED_PREAMBLE_LENGTH = 8;

//! Translate the SPREADING_FACTOR parameter to RegModemConfig2.SpreadingFactor
//! (datasheet section 6.4: field value equals SF number, 6..12).
inline bool getSpreadingFactorReg(const SpreadingFactor& value, U8& reg) {
    switch (value) {
        case SpreadingFactor::SF6:
        case SpreadingFactor::SF7:
        case SpreadingFactor::SF8:
        case SpreadingFactor::SF9:
        case SpreadingFactor::SF10:
        case SpreadingFactor::SF11:
        case SpreadingFactor::SF12:
            reg = static_cast<U8>(value.e);
            return true;
        default:
            return false;
    }
}

//! Translate the BANDWIDTH parameter to RegModemConfig1.Bw (bits 7-4),
//! already left-shifted into position (datasheet section 6.4).
inline bool getBandwidthReg(const Bandwidth& value, U8& reg) {
    if (value == Bandwidth::BW_7_8_KHZ) {
        reg = 0x00;
    } else if (value == Bandwidth::BW_10_4_KHZ) {
        reg = 0x10;
    } else if (value == Bandwidth::BW_15_6_KHZ) {
        reg = 0x20;
    } else if (value == Bandwidth::BW_20_8_KHZ) {
        reg = 0x30;
    } else if (value == Bandwidth::BW_31_25_KHZ) {
        reg = 0x40;
    } else if (value == Bandwidth::BW_41_7_KHZ) {
        reg = 0x50;
    } else if (value == Bandwidth::BW_62_5_KHZ) {
        reg = 0x60;
    } else if (value == Bandwidth::BW_125_KHZ) {
        reg = 0x70;
    } else if (value == Bandwidth::BW_250_KHZ) {
        reg = 0x80;
    } else if (value == Bandwidth::BW_500_KHZ) {
        reg = 0x90;
    } else {
        return false;
    }
    return true;
}

//! Translate the CODING_RATE parameter to RegModemConfig1.CodingRate
//! (bits 3-1), already left-shifted into position (datasheet Table 60).
inline bool getCodingRateReg(const CodingRate& value, U8& reg) {
    switch (value) {
        case CodingRate::CR_4_5:
        case CodingRate::CR_4_6:
        case CodingRate::CR_4_7:
        case CodingRate::CR_4_8:
            reg = static_cast<U8>(static_cast<U8>(value.e) << 1);
            return true;
        default:
            return false;
    }
}

//! Frf = freqHz * 2^19 / 32 MHz (datasheet section 6.4, RegFrLsb)
inline U32 frequencyToFrf(U32 freqHz) {
    return static_cast<U32>((static_cast<U64>(freqHz) * FRF_DIVISOR) / CRYSTAL_HZ);
}

//! RFM9X transmitter output power settings on the PA_BOOST pin. On PA_BOOST (datasheet
//! section 6.4, RegPaConfig): Pout = 17 - (15 - OutputPower) = 2 + OutputPower
//! dBm for OutputPower 0..15, independent of the MaxPower field. +20 dBm needs
//! RegPaDac = 0x87 (vs. default 0x84, datasheet Table 78) with OutputPower=15
//! and an OCP trim increase: measured PA_BOOST current at +20 dBm is 120 mA
//! (datasheet Table "IDDT", RFOP=+20dBm row); RegOcp's OcpTrim formula for
//! OcpTrim 16..27 is Imax = -30 + 10*OcpTrim (datasheet Table 81), so
//! OcpTrim=16 gives Imax=130 mA, comfortably above the measured draw.
struct TxPowerSetting {
    U8 paConfig;  //!< RegPaConfig: PaSelect=1 (PA_BOOST), MaxPower=default 0x04, OutputPower
    U8 ocp;       //!< RegOcp: OcpOn=1, OcpTrim per the margin above
    U8 paDac;     //!< RegPaDac: 0x84 normal, 0x87 enables the +20 dBm option
};

inline bool getTxPowerSetting(const Rfm9XTxPower& value, TxPowerSetting& setting) {
    // RegPaConfig = PaSelect(0x80) | MaxPower(0x04<<4 = 0x40) | OutputPower
    if (value == Rfm9XTxPower::DBM_5) {
        setting = {0xC3, 0x2B, 0x84};  // OutputPower=3  -> 2+3=5 dBm
    } else if (value == Rfm9XTxPower::DBM_10) {
        setting = {0xC8, 0x2B, 0x84};  // OutputPower=8  -> 2+8=10 dBm
    } else if (value == Rfm9XTxPower::DBM_13) {
        setting = {0xCB, 0x2B, 0x84};  // OutputPower=11 -> 2+11=13 dBm
    } else if (value == Rfm9XTxPower::DBM_17) {
        setting = {0xCF, 0x2B, 0x84};  // OutputPower=15 -> 2+15=17 dBm
    } else if (value == Rfm9XTxPower::DBM_20) {
        setting = {0xCF, 0x30, 0x87};  // OutputPower=15, PaDac high-power, OcpTrim=16
    } else {
        return false;
    }
    return true;
}

}  // namespace Rfm9X

#endif  // Rfm9X_Rfm9XRadio_HPP