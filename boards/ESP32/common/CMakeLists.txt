set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

# add audio source files
file(GLOB_RECURSE AUDIO_SRCS "${COMMON_PATH}/audio/*.c")

# add lcd source files
file(GLOB_RECURSE LCD_SRCS "${COMMON_PATH}/lcd/*.c")

# add io_expander source files
file(GLOB_RECURSE IO_EXPANDER_SRCS "${COMMON_PATH}/io_expander/*.c")

# add touch source files
file(GLOB_RECURSE TOUCH_SRCS "${COMMON_PATH}/touch/*.c")

set(BOARD_SRC "")
set(BOARD_INC "${COMMON_PATH}")

if (CONFIG_ENABLE_DISPLAY)
    list(APPEND BOARD_SRC
        "${COMMON_PATH}/display/board_lvgl.c"
        "${LCD_SRCS}"
        "${IO_EXPANDER_SRCS}"
        "${TOUCH_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/lcd"
        "${COMMON_PATH}/io_expander"
        "${COMMON_PATH}/touch"
    )
endif()

if (CONFIG_ENABLE_AUDIO)
    list(APPEND BOARD_SRC
        "${AUDIO_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/audio"
    )
endif()
