add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Rfm9XConfig")

register_fprime_module(
    INTERFACE
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Subtopology.fpp"
    EXCLUDE_FROM_ALL
    DEPENDS
        fprime-sensors_Rfm9X_Subtopology_Rfm9XConfig
)
