// ======================================================================
// \title  BmpManagerTestMain.cpp
// \author Generated
// \brief  test main for BmpManager component
// ======================================================================

#include "BmpManagerTester.hpp"

TEST(Nominal, Test) {
    Bmp280::BmpManagerTester tester;
    tester.test_nominal();
}

TEST(Error, Test) {
    Bmp280::BmpManagerTester tester;
    tester.test_error();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}