####
# FPrime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
# UT_SOURCE_FILES: list of source files for unit tests
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/cmake-api/
#
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ImuManager.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ImuStateMachine.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ImuManager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ImuHelpers.cpp"
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ImuManager.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ImuStateMachine.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ImuManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ImuManagerTester.cpp"
    DEPENDS
        STest
    UT_AUTO_HELPERS
)
