##
# @file CMakeLists.txt
# @brief Raspberry Pi board configuration
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# MODULE_NAME
set(MODULE_NAME "Raspberry_Pi")

# LIB_SRCS
# set(LIB_SRCS
#     ${MODULE_PATH}/board_com_api.c
#     ${MODULE_PATH}/keyboard_input.c
# )
file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/*.c")

# Enable V4L2 camera driver if camera support is enabled
if (CONFIG_ENABLE_CAMERA STREQUAL "y" AND CONFIG_ENABLE_CAMERA_V4L2 STREQUAL "y")
    message(STATUS "Raspberry Pi board: Enabling V4L2 camera driver")
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
    message(STATUS "Raspberry Pi board: Linking ALSA library")
endif()

# Link SDL2 library if SDL display support is enabled
if (CONFIG_ENABLE_SDL_DISPLAY STREQUAL "y")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)

    target_include_directories(${MODULE_NAME} PUBLIC ${SDL2_INCLUDE_DIRS})
    target_compile_options(${MODULE_NAME} PUBLIC ${SDL2_CFLAGS_OTHER})
    target_link_libraries(${MODULE_NAME} PUBLIC ${SDL2_LIBRARIES})

    message(STATUS "Raspberry Pi board: Linking SDL2 for desktop window display")
endif()

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

