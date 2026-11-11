####
# FPrime CMakeLists.txt:
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/user-manual/build-system/cmake-api/
#
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XSim.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XSim.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XSimModel.cpp"
    DEPENDS
        fprime-sensors_Rfm9X_Components_Rfm9XManager
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XSim.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Rfm9XSimModelTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XSimModel.cpp"
    DEPENDS
        fprime-sensors_Rfm9X_Components_Rfm9XManager
)
