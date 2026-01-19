##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_AI_PLAYER STREQUAL "y")
# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
# Root source files
file(GLOB ROOT_SRCS "${MODULE_PATH}/src/*.c")

# Consumer files
file(GLOB CONSUMER_SRCS "${MODULE_PATH}/src/consumer/*.c")

# Datasink files
file(GLOB DATASINK_SRCS "${MODULE_PATH}/src/datasink/*.c")

# Decoder files
if (CONFIG_ENABLE_AI_PLAYER_DECODER_OPUS STREQUAL "y")
file(GLOB DECODER_OPUS_SRCS "${MODULE_PATH}/src/decoder/opus/*c")
endif()

if (CONFIG_ENABLE_AI_PLAYER_DECODER_OGGOPUS STREQUAL "y")
file(GLOB DECODER_OPUS_SRCS "${MODULE_PATH}/src/decoder/ogg/src/*c")
endif()

file(GLOB DECODER_SRCS "${MODULE_PATH}/src/decoder/*.c" ${DECODER_OPUS_SRCS})

# Mixer files
file(GLOB MIXER_SRCS "${MODULE_PATH}/src/mixer/*.c")

# Resample files
file(GLOB RESAMPLE_SRCS "${MODULE_PATH}/src/resample/*.c")

# Combine all source files
set(LIB_SRCS
    ${ROOT_SRCS}
    ${CONSUMER_SRCS}
    ${DATASINK_SRCS}
    ${DECODER_SRCS}
    ${MIXER_SRCS}
    ${RESAMPLE_SRCS}
)

# LIB_PUBLIC_INC
if (CONFIG_ENABLE_AI_PLAYER_DECODER_OGGOPUS STREQUAL "y")
set(OGG_INC ${MODULE_PATH}/src/decoder/ogg/include)
endif()

set(LIB_PUBLIC_INC ${MODULE_PATH}/include ${OGG_INC})


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


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
endif()