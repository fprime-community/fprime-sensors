# **RFM9X (SX1276/8) F´ Component Development & OTA GDS Integration Plan**

* **Baseline Status:** Directory cloned from `fprime-sensors/Rfm69` to `fprime-sensors/Rfm9X` with global symbol/file renaming (`69` → `9X`) completed.
* **Target Hardware (Flight):** **STM32H753XI-EVAL2** + Adafruit RFM9X LoRa Breakout (SPI5 + GPIO CS/RST/DIO0)
* **Target Hardware (Ground):** **SAMD21 / STM32 Nucleo** + Adafruit RFM9X LoRa Breakout (USB-UART to PC)
* **Frequency Band:** **434.0 MHz** (LoRa, Bandwidth 125 kHz, Spreading Factor SF7, Coding Rate 4/5)
* **Execution Model:** Bare-Metal Cyclic Executive (`Os::Baremetal`) on Flight / Transparent Serial Bridge on Ground

---

## **Milestone Snapshot**
* **Week 1:** FPP & Register Map Adaptation (FSK → LoRa), Dual-Port FIFO Pointer Logic, Host Unit Tests (`fprime-util check`), and Raw ASCII Over-the-Air Test.
* **Week 2:** F´ Framing Pipeline Integration (`Svc::Framer` / `Svc::Deframer`), Ground Gateway Firmware, and Full OTA F´ GDS Telemetry Session.

---

## **Week 1: FSK-to-LoRa Driver Conversion, Unit Tests & Raw ASCII OTA Test**

### **Task Checklist**
* [x] **Task 1.1: Initial Directory & File Refactoring**
  * Cloned `fprime-sensors/Rfm69` into `fprime-sensors/Rfm9X`.
  * Performed global renaming of files, component names, FPP module names, and variable definitions from `69` to `9X`.

* [ ] **Task 1.2: FPP Types Adaptation for LoRa**
  * Remove legacy FSK bit-rate enums (`Rfm9XDataRate`).
  * Add LoRa configuration enums in `Rfm9X.fpp`:
    * `SpreadingFactor`: `SF6` through `SF12`
    * `Bandwidth`: `BW_125KHZ`, `BW_250KHZ`, `BW_500KHZ`
    * `CodingRate`: `CR_4_5`, `CR_4_6`, `CR_4_7`, `CR_4_8`

* [ ] **Task 1.3: Register Map & SX1276/8 LoRa Engine Conversion**
  * Replace RFM69 (SX1231) register addresses in `Rfm9XRegisters.hpp` with SX1276/8 register definitions (`RegOpMode`, `RegFrf`, `RegFifoAddrPtr`, `RegFifoTxBaseAddr`, `RegFifoRxBaseAddr`, `RegModemConfig1/2/3`, `RegIrqFlags`, `RegDioMapping1`).
  * **LoRa Mode Enforcement**: Modify initialization logic to enter `SLEEP` mode prior to toggling the `LongRangeMode` bit in `RegOpMode`.
  * **Dual-Port FIFO Pointer Logic**: Replace linear FIFO accesses with SX1276 pointer offsets (`RegFifoAddrPtr` and `RegFifoRxCurrentAddr`).
  * **DIO0 Mapping**: Configure `RegDioMapping1` for `RxDone` (`00`) in RX mode and `TxDone` (`01`) in TX mode.

* [ ] **Task 1.4: Commands, Telemetry Channels & Events Adaptation**
  * Update commands: `SetFrequency(freqHz)`, `SetRadioConfig(sf, bw, cr)`, `SetTxPower(powerDbm)`.
  * Update telemetry formulas: `LastPacketRssi` (calculated as `-137 + RegPktRssiValue`), `LastPacketSnr` (`RegPktSnrValue`), `TxPackets`, `RxPackets`, `CrcErrorCount`.
  * Update events: `RadioInitialized`, `TxTimeout`, `CrcErrorDetected`, `InvalidConfiguration`.

* [ ] **Task 1.5: Host-Native Unit Testing (`fprime-util check`)**
  * Update `fprime-sensors/Rfm9X/test/ut` GTest fixtures to mock SX1276/8 SPI register responses.
  * Test register write/read helpers, LoRa mode switching, FIFO pointer arithmetic, and CRC error flags.

* [ ] **Task 1.6: Hardware Setup & Raw ASCII Over-the-Air Test**
  * Wire the Adafruit RFM9X breakout to the STM32H753XI-EVAL2 board (SPI5: `PF6` CS, `PF7` SCK, `PF8` MISO, `PF9` MOSI, `PG2` RST, `PG3` DIO0).
  * Flash a simple polling/loopback sketch on the SAMD21/Nucleo ground board configured for 434 MHz LoRa.
  * **ASCII Verification**: Transmit a `"HELLO Fprime LORA"` test string from the STM32H7 board over `sendData` and verify clean reception on the ground board terminal.

#### **Week 1 Exit Criterion**
Bi-directional raw ASCII text strings pass over the 434 MHz radio link between the STM32H753XI-EVAL2 board and the ground receiver with zero byte corruption, and `fprime-util check` passes 100% of host unit tests.

---

## **Week 2: F´ Framing Pipeline, Ground Gateway & OTA GDS Telemetry**

### **Task Checklist**
* [ ] **Task 2.1: F´ Topology Integration**
  * Instantiate `Rfm9X` in `instances.fpp` and wire it into `topology.fpp`.
  * Connect `sdr.sendData` to `framer.comBufferOut` and `framer.bufferDeallocate` to `sdr.sendBufferDeallocate`.
  * Connect `sdr.recvData` to `deframer.framedIn` and `deframer.framedBufferDeallocate` to `sdr.deallocateBuffer`.

* [ ] **Task 2.2: Ground Station UART-to-LoRa Gateway Firmware**
  * Develop/flash lightweight firmware on the SAMD21/Nucleo board that acts as a transparent byte pipe:
    * **Downlink (RF → UART)**: Receives LoRa packets over the air and forwards raw bytes directly to UART at 115200 baud.
    * **Uplink (UART → RF)**: Reads incoming UART bytes from `fprime-gds`, buffers them into CCSDS frame lengths, and transmits them over LoRa.

* [ ] **Task 2.3: Over-the-Air `fprime-gds` Verification**
  * Launch `fprime-gds` connected to the Ground Gateway board:
    ```bash
    fprime-gds --communication-selection uart --uart-device /dev/ttyUSB0 --uart-baud 115200 -d ./build-artifacts/stm32h7/ReferenceDeployment
    ```
  * **Command Uplink Test**: Send `cmdDisp.CMD_NO_OP` and `led.BLINKING_ON_OFF` from GDS over LoRa to the STM32 board and confirm execution.
  * **Telemetry Downlink Test**: Verify live streaming of `OsTime`, `LedTransitions`, `MpuImu`, `LastPacketRssi`, and `LastPacketSnr` on the GDS dashboard.

* [ ] **Task 2.4: RF Resilience, Range & CRC Stress Testing**
  * Verify that corrupt packets (injected CRC errors or forced attenuation) trigger the `CrcErrorCount` channel and are safely dropped by `Svc::Deframer` without crashing the bare-metal cyclic executive.
  * Measure link budget and RSSI/SNR bounds under continuous 1 Hz telemetry streaming.

* [ ] **Task 2.5: Documentation & Upstream PR Preparation**
  * Author `fprime-sensors/Rfm9X/docs/sdd.md` detailing the register map, FPP ports, and LoRa configuration instructions.
  * Update `lib/fprime-sensors/README.md` and verify that the module compiles cleanly for both Linux (`Drv::LinuxSpiDriver`) and Bare-Metal (`Stm32SpiDriver`).

#### **Week 2 Exit Criterion**
A continuous 15+ minute `fprime-gds` flight software session running purely over the 434 MHz LoRa link (Commanding, Telemetry, and Event logging verified with zero frame drops or system hangs).
