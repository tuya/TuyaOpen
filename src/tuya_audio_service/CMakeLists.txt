##
# @file CMakeLists.txt
# @brief 
#/
if(CONFIG_ENABLE_AUDIO_SERVICE STREQUAL "y")

# MODULE_PATH

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS 
    "${MODULE_PATH}/protobuf/*.c"
    "${MODULE_PATH}/websocket_client/*.c"
    "${MODULE_PATH}/tuya_voice_protocol/*.c"
    "${MODULE_PATH}/speaker_upload/src/*.c"
    "${MODULE_PATH}/encode_wav/src/*.c"
    "${MODULE_PATH}/codec_speex/src/*.c"
    "${MODULE_PATH}/speex/libspeex/*.c")

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/protobuf/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/websocket_client/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/tuya_voice_protocol/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/tuya_voice_protocol/src)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/speaker_upload/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/codec_speex/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/codec_speex/src)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/encode_wav/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/speex/port)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/speex/include)


set(LIB_PRIVATE_INC ${MODULE_PATH}/tuya_voice_protocol/src/stream_gw)
add_definitions(-DHAVE_CONFIG_H)

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

    INTERFACE

    )


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
