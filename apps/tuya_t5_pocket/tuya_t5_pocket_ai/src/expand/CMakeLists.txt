##
# @file CMakeLists.txt
# @brief CMake configuration for expand module
#/

set(EXPAND_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# Collect source files
aux_source_directory(${EXPAND_MODULE_PATH}/src EXPAND_SRCS)

# Add sources to main library
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${EXPAND_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${EXPAND_MODULE_PATH}
        ${EXPAND_MODULE_PATH}/inc
    )