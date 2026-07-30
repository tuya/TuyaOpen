##
# @file CMakeLists.txt
# @brief
#/

if (CONFIG_ENABLE_POWER STREQUAL "y")

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)
aux_source_directory(${APP_PATH}/src APP_SRCS)

add_definitions(-DSTATIC_IN_RELEASE=static)
add_definitions(-DMAJOR_VERSION=1 -DMINOR_VERSION=0 -DMICRO_VERSION=0 -DVERSION=\"1.0.0\")

add_library(${EXAMPLE_LIB})
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )
else()
message(FATAL_ERROR "power_selftest cannot work when CONFIG_ENABLE_POWER is not set")
endif()
