##
# @file CMakeLists.txt
# @brief Ubuntu board configuration
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# MODULE_NAME
set(MODULE_NAME "Ubuntu")

# LIB_SRCS - only compile what's needed
set(LIB_SRCS ${MODULE_PATH}/board_com_api.c)

# KWS/VAD stub (Ubuntu has no on-device KWS/VAD)
list(APPEND LIB_SRCS ${MODULE_PATH}/tkl_kws_vad_stub.c)

if (CONFIG_ENABLE_KEYBOARD_INPUT STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_button_keyboard.c)
endif()

if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_disp_sdl.c)

    # Find SDL2 for compilation
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH})


########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}
        ${TOP_SOURCE_DIR}/src/peripherals/button/tdl_button/include
        ${TOP_SOURCE_DIR}/src/peripherals/button/tdd_button/include
        ${TOP_SOURCE_DIR}/src/peripherals/display/tdl_display/include
        ${TOP_SOURCE_DIR}/src/peripherals/display/tdd_display/include
        ${TOP_SOURCE_DIR}/src/peripherals/tp/tdl_tp/include
        ${SDL2_INCLUDE_DIRS}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

# Link system audio libraries for Ubuntu
if (CONFIG_ENABLE_AUDIO_ALSA STREQUAL "y")
    target_link_libraries(${MODULE_NAME} PUBLIC asound)
    message(STATUS "Ubuntu board: Linking ALSA library")
endif()

if (CONFIG_ENABLE_TUYA_CODEC_OPUS STREQUAL "y")
    target_link_libraries(${MODULE_NAME} PUBLIC opus)
    message(STATUS "Ubuntu board: Linking Opus library")
endif()


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
