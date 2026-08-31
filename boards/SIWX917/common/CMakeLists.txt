set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

# add audio source files
file(GLOB_RECURSE AUDIO_SRCS "${COMMON_PATH}/audio/*.c")

# add display source files
file(GLOB_RECURSE DISPLAY_SRCS "${COMMON_PATH}/display/*.c")

set(BOARD_SRC "")
set(BOARD_INC "${COMMON_PATH}")

if (CONFIG_ENABLE_DISPLAY)
    list(APPEND BOARD_SRC
        "${COMMON_PATH}"
        "${DISPLAY_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/display"
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
