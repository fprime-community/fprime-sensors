register_fprime_config(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Rfm9XManagerCfg.fpp"
    DEPENDS
        Fw_Types
    EXCLUDE_FROM_ALL
)

# fprime__internal_process_configuration_sources() only adds
# "<build dir>/.." to this module's include path (one level up from this
# module's own build-cache directory), but the generated FppConstantsAc.cpp
# quote-includes its sibling FppConstantsAc.hpp via the full library-relative
# path (fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManagerCfg/...), not
# a single-level-relative one -- so one "/.." of headroom is not enough at
# this module's nesting depth. Every other register_fprime_config() module in
# this codebase only declares HEADERS (no AUTOCODER_INPUTS), so this gap was
# never exercised before. Add the project binary root explicitly instead of
# patching the shared CMake macro.
if (DEFINED FPRIME_BINARY_DIR)
    set(RFM9X_MANAGER_CFG_BUILD_ROOT "${FPRIME_BINARY_DIR}")
else()
    set(RFM9X_MANAGER_CFG_BUILD_ROOT "${CMAKE_BINARY_DIR}")
endif()
fprime_target_include_directories(fprime-sensors_Rfm9X_Components_Rfm9XManager_Rfm9XManagerCfg
    PUBLIC "${RFM9X_MANAGER_CFG_BUILD_ROOT}"
)
