// ----------------------------------------------------------------------
// TestMain.cpp
// ----------------------------------------------------------------------

#include "XBeeManagerTester.hpp"

TEST(Nominal, Initial) {
    XBee::XBeeManagerTester tester;
    tester.test_initial();
}

TEST(Nominal, BasicIo) {
    XBee::XBeeManagerTester tester;
    tester.test_basic_io();
}

TEST(Nominal, Fail) {
    XBee::XBeeManagerTester tester;
    tester.test_fail();
}

TEST(OffNominal, Retry) {
    XBee::XBeeManagerTester tester;
    tester.test_retry();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
