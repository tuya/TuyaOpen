##
# @file CMakeLists.txt
# @brief M5Stack StickS3 hardware bring-up example
#/

if (CONFIG_BOARD_CHOICE_M5STACK_STICKS3 STREQUAL "y")
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)
aux_source_directory(${APP_PATH}/src APP_SRCS)

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )
else()
message(FATAL_ERROR "m5stack_sticks3_hardware example requires BOARD_CHOICE_M5STACK_STICKS3")
endif()
