##
# @file CMakeLists.txt
# @brief Jieli board library; self-contained like the other platform boards.
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH} LIB_SRCS)

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

# The TKL adapter target lives under the platform tree and force-includes this
# board's audio_config.h for the wl83 capture path, so it is built from here
# where the board can export the header.
set(JIELI_BOARD_AUDIO_CONFIG "${MODULE_PATH}/audio_config.h")
add_subdirectory(
    "${TOP_SOURCE_DIR}/platform/JIELI/tuyaos/tuyaos_adapter"
    "${CMAKE_CURRENT_BINARY_DIR}/jieli_tuya_adapter"
)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME} jieli_tuya_adapter)
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
