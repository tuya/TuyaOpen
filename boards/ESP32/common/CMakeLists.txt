set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

set(FONT_SRC 
    "${COMMON_PATH}/display/font/font_awesome_14_1.c"
    "${COMMON_PATH}/display/font/font_awesome_30_1.c"
    "${COMMON_PATH}/display/font/font_puhui_14_1.c"
    )

set(BOARD_SRC
    "${FONT_SRC}"
    "${COMMON_PATH}/audio/tdd_audio_no_codec.c"
    "${COMMON_PATH}/display/oled_display.c"
    )

set(BOARD_INC
    "${COMMON_PATH}/audio"
    "${COMMON_PATH}/display"
    "${COMMON_PATH}/display/font"
    )
