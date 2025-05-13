set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

# add font, emoji source files
file(GLOB_RECURSE FONT_SRCS "${COMMON_PATH}/display/font/*.c")

# add image source files
file(GLOB_RECURSE IMAGE_SRCS "${COMMON_PATH}/display/image/*.c")

# add ui source files
file(GLOB_RECURSE UI_SRCS "${COMMON_PATH}/display/ui/*.c")

# add common source files
file(GLOB OTHER_SRCS "${COMMON_PATH}/display/*.c")

# add audio source files
file(GLOB_RECURSE AUDIO_SRCS "${COMMON_PATH}/audio/*.c")

# add lcd source files
file(GLOB_RECURSE LCD_SRCS "${COMMON_PATH}/lcd/*.c")

# add io_expander source files
file(GLOB IO_EXPANDER_SRCS "${COMMON_PATH}/io_expander/*.c")

set(BOARD_SRC
    "${AUDIO_SRCS}"
    "${FONT_SRCS}"
    "${IMAGE_SRCS}"
    "${UI_SRCS}"
    "${OTHER_SRCS}"
    "${LCD_SRCS}"
    "${IO_EXPANDER_SRCS}"
)

set(BOARD_INC
    "${COMMON_PATH}"
    "${COMMON_PATH}/audio"
    "${COMMON_PATH}/display"
    "${COMMON_PATH}/display/font"
    "${COMMON_PATH}/display/ui"
    "${COMMON_PATH}/lcd"
    "${COMMON_PATH}/io_expander"
)
