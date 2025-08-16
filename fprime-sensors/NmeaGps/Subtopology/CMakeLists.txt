add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config")

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Subtopology.fpp"
    EXCLUDE_FROM_ALL
)
