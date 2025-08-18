register_fprime_config(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/MpuImuSubtopologyConfig.fpp"
    DEPENDS
        Fw_Types
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/MpuImuSubtopologyConfig.hpp"
)