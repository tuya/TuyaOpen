##
# @file CMakeLists.txt
# @brief TaishanPi_3 board configuration
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# MODULE_NAME
set(MODULE_NAME "TaishanPi_3")

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/*.c")

# Enable V4L2 camera driver if camera support is enabled
if (CONFIG_ENABLE_CAMERA STREQUAL "y" AND CONFIG_ENABLE_CAMERA_V4L2 STREQUAL "y")
    message(STATUS "TaishanPi_3 board: Enabling V4L2 camera driver")
else()
    list(REMOVE_ITEM LIB_SRCS "${MODULE_PATH}/tdd_camera_v4l2.c")
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

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

# Link ALSA library if ALSA support is enabled
if (CONFIG_ENABLE_AUDIO_ALSA STREQUAL "y")
    target_link_libraries(${MODULE_NAME} PUBLIC asound)
    message(STATUS "TaishanPi_3 board: Linking ALSA library")
endif()

# Link SDL2 library if SDL display support is enabled
if (CONFIG_ENABLE_SDL_DISPLAY STREQUAL "y")
    set(SDL2_PREBUILT_DIR "${TOP_SOURCE_DIR}/platform/LINUX/tuyaos_adapter/src/tkl_display/libs/Raspberry_Pi/SDL2")
    target_include_directories(${MODULE_NAME} PUBLIC "${SDL2_PREBUILT_DIR}/include/SDL2")
    target_link_libraries(${MODULE_NAME} PUBLIC "${SDL2_PREBUILT_DIR}/libSDL2.a" pthread dl)
    message(STATUS "TaishanPi_3 board: Linking prebuilt ARM64 SDL2 static library (X11 via dlopen)")
endif()

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
