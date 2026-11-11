####
# FPrime CMakeLists.txt:
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/cmake-api/
#
####

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Rfm9XManagerCfg")

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XManager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XHelpers.cpp"
    DEPENDS
        Drv_Ports
        fprime-sensors_Rfm9X_Components_Rfm9XManager_Rfm9XManagerCfg
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Rfm9XManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Rfm9XManagerTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../Rfm9XSim/Rfm9XSimModel.cpp"
    DEPENDS
        fprime-sensors_Rfm9X_Components_Rfm9XManager_Rfm9XManagerCfg
    UT_AUTO_HELPERS
)
