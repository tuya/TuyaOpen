##
# @file CMakeLists.txt
# @brief Xteink X4 LVGL-on-EPD board example
#/

if (CONFIG_BOARD_CHOICE_XTEINK_X4 STREQUAL "y")
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)
aux_source_directory(${APP_PATH}/src APP_SRCS)

set(APP_OPTIONS "-W")
list(APPEND APP_OPTIONS "-Wall" "-DLV_LVGL_H_INCLUDE_SIMPLE")

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )
else()
message(FATAL_ERROR "xteink_x4_lvgl example requires BOARD_CHOICE_XTEINK_X4")
endif()
