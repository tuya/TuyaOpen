set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

# add audio source files
file(GLOB_RECURSE AUDIO_SRCS "${COMMON_PATH}/audio/*.c")

# add display source files
file(GLOB_RECURSE DISPLAY_SRCS "${COMMON_PATH}/display/*.c")

# add lcd source files
file(GLOB_RECURSE LCD_SRCS "${COMMON_PATH}/lcd/*.c")

# add io_expander source files
file(GLOB_RECURSE IO_EXPANDER_SRCS "${COMMON_PATH}/io_expander/*.c")

# add touch source files
file(GLOB_RECURSE TOUCH_SRCS "${COMMON_PATH}/touch/*.c")

set(BOARD_SRC "")
set(BOARD_INC "${COMMON_PATH}")

if (CONFIG_ENABLE_ESP_DISPLAY)
    list(APPEND BOARD_SRC
        "${COMMON_PATH}"
        "${DISPLAY_SRCS}"
        "${LCD_SRCS}"
        "${IO_EXPANDER_SRCS}"
        "${TOUCH_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/lcd"
        "${COMMON_PATH}/io_expander"
        "${COMMON_PATH}/touch"
        "${COMMON_PATH}/display"
    )
    
    set_property(SOURCE ${DISPLAY_SRCS}
                 APPEND PROPERTY COMPILE_DEFINITIONS LV_LVGL_H_INCLUDE_SIMPLE)
endif()

if (CONFIG_ENABLE_AUDIO)
    list(APPEND BOARD_SRC
        "${AUDIO_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/audio"
    )
endif()
