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


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

