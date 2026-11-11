// ----------------------------------------------------------------------
// Rfm9XManagerTestMain.cpp
// ----------------------------------------------------------------------

#include "Rfm9XManagerTester.hpp"

TEST(Nominal, Initialization) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_initialization();
}

TEST(Nominal, Transmit) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit();
}

TEST(OffNominal, TransmitZeroSize) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_zero_size();
}

TEST(OffNominal, TransmitOversize) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_oversize();
}

TEST(Nominal, TransmitLarge) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_large();
}

TEST(Nominal, ReceiveLarge) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive_large();
}

TEST(Nominal, DefaultRegisterImage) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_default_register_image();
}

TEST(Nominal, LongRangeModeSleepGated) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_long_range_mode_sleep_gated();
}

TEST(Nominal, FifoPointerManagement) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_fifo_pointer_management();
}

TEST(Nominal, RssiSnrTelemetry) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_rssi_snr_telemetry();
}

TEST(Nominal, Dio0Mapping) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_dio0_mapping();
}

TEST(Nominal, SpreadingFactorRegisterMap) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_spreading_factor_register_map();
}

TEST(Nominal, LowDataRateOptimize) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_low_data_rate_optimize();
}

TEST(Nominal, ConfigureTickConversion) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_configure_tick_conversion();
}

TEST(Nominal, BandwidthUpdateReconfigure) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_bandwidth_update_reconfigure();
}

TEST(Nominal, BandwidthRegisterMap) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_bandwidth_register_map();
}

TEST(Nominal, CodingRateRegisterMap) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_coding_rate_register_map();
}

TEST(Nominal, ParameterUpdateReconfigure) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_parameter_update_reconfigure();
}

TEST(Nominal, ResetRecovery) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_reset_recovery();
}

TEST(OffNominal, TransmitDroppedWhenBusy) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_dropped_when_busy();
}

TEST(OffNominal, DownlinkStarvationRecovery) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_downlink_starvation_recovery();
}

TEST(Nominal, Receive) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive();
}

TEST(Nominal, DataReturn) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_data_return();
}

TEST(OffNominal, DetectionRetry) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_detection_retry();
}

TEST(OffNominal, TransmitNotReady) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_not_ready();
}

TEST(OffNominal, TransmitSpiFailure) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_spi_failure();
}

TEST(OffNominal, TransmitTimeoutRecovery) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_timeout_recovery();
}

TEST(OffNominal, ReceiveAllocationFailure) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive_allocation_failure();
}

TEST(OffNominal, ReceiveCrcDrop) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive_crc_drop();
}

TEST(OffNominal, DetectionRadioAbsent) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_detection_radio_absent();
}

TEST(OffNominal, TransmitChannelBusy) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_channel_busy();
}

TEST(Nominal, TransmitModeSettle) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_mode_settle();
}

TEST(Nominal, TransmitDisabled) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_disabled();
}

TEST(Nominal, TransmitEnableIdempotent) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_enable_idempotent();
}

TEST(OffNominal, ResetDuringTransmit) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_reset_during_transmit();
}

TEST(Nominal, LocalHoldOrdering) {
    Rfm9X::Rfm9XManagerTester tester(true);
    tester.test_local_hold_ordering();
}

TEST(Nominal, SendStringCommand) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_send_string_command();
}

TEST(OffNominal, SendStringZeroSize) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_send_string_zero_size();
}

TEST(OffNominal, SendStringNotReady) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_send_string_not_ready();
}

TEST(OffNominal, SendStringBusy) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_send_string_busy();
}

TEST(Nominal, SendStringNoComSideEffects) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_send_string_no_com_side_effects();
}

TEST(Nominal, SingleSpiRequestInFlight) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_single_spi_request_in_flight();
}

TEST(OffNominal, StaleCompletionAfterResetDropped) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_stale_completion_after_reset_dropped();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
