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

TEST(Nominal, TransmitFifoFit) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_transmit_fifo_fit();
}

TEST(Nominal, ReceiveLarge) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive_large();
}

TEST(Nominal, ReceiveFifoFit) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_receive_fifo_fit();
}

TEST(Nominal, DefaultRegisterImage) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_default_register_image();
}

TEST(Nominal, HighPowerBoostRecovery) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_high_power_boost_recovery();
}

TEST(Nominal, DataRateRegisterMap) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_data_rate_register_map();
}

TEST(Nominal, BandwidthUpdateReconfigure) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_bandwidth_update_reconfigure();
}

TEST(Nominal, BandwidthRegisterMap) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_bandwidth_register_map();
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

TEST(OffNominal, StaleSyncRecovery) {
    Rfm9X::Rfm9XManagerTester tester;
    tester.test_stale_sync_recovery();
}

TEST(Nominal, LocalHoldOrdering) {
    Rfm9X::Rfm9XManagerTester tester(true);
    tester.test_local_hold_ordering();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
