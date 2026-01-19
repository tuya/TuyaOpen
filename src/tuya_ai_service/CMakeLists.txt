##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(LIB_SRCS)
set(LIB_PUBLIC_INC)
set(LIB_PRIVATE_INC)

# tuya ai port start
list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/port/include)
file(GLOB_RECURSE AI_PORT_SRC "${MODULE_PATH}/port/src/*.c")
list(APPEND LIB_SRCS ${AI_PORT_SRC})
# tuya ai port end

# svc_ai_agent start
file(GLOB_RECURSE AI_AGENT_SRC "${MODULE_PATH}/svc_ai_agent/src/*.c")
set(AI_AGENT_INC ${MODULE_PATH}/svc_ai_agent/include)
list(APPEND LIB_SRCS ${AI_AGENT_SRC})
list(APPEND LIB_PUBLIC_INC ${AI_AGENT_INC})
# svc_ai_agent end

# svc_ai_basic start
file(GLOB_RECURSE AI_BASIC_SRC "${MODULE_PATH}/svc_ai_basic/src/*.c")
set(AI_BASIC_INC ${MODULE_PATH}/svc_ai_basic/include)
list(APPEND LIB_SRCS ${AI_BASIC_SRC})
list(APPEND LIB_PUBLIC_INC ${AI_BASIC_INC})
# svc_ai_basic end

# svc_ai_codec start
if (CONFIG_ENABLE_TUYA_CODEC)
    set(AI_CODEC_INC ${MODULE_PATH}/svc_ai_codec/include)
    list(APPEND LIB_PUBLIC_INC ${AI_CODEC_INC})
    
    # 基础编码器
    list(APPEND LIB_SRCS ${MODULE_PATH}/svc_ai_codec/src/tuya_ai_encoder.c)
    
    # OPUS 编码器
    if (CONFIG_ENABLE_TUYA_CODEC_OPUS)
        list(APPEND LIB_SRCS ${MODULE_PATH}/svc_ai_codec/src/tuya_ai_encoder_opus.c)
    endif()
    
    # OPUS IPC 编码器
    if (CONFIG_ENABLE_TUYA_CODEC_OPUS_IPC)
        list(APPEND LIB_SRCS ${MODULE_PATH}/svc_ai_codec/src/tuya_ai_encoder_opus_ipc.c)
        list(APPEND LIB_SRCS ${MODULE_PATH}/svc_ai_codec/src/tuya_ai_encoder_opus_ipc_server.c)
    endif()
    
    # SPEEX 编码器
    if (CONFIG_ENABLE_TUYA_CODEC_SPEEX)
        list(APPEND LIB_SRCS ${MODULE_PATH}/svc_ai_codec/src/tuya_ai_encoder_speex.c)
    endif()
endif()
# svc_ai_codec end

# svc_ai_monitor start
file(GLOB_RECURSE AI_MONITOR_SRC "${MODULE_PATH}/svc_ai_monitor/src/*.c")
set(AI_MONITOR_INC ${MODULE_PATH}/svc_ai_monitor/include)
list(APPEND LIB_SRCS ${AI_MONITOR_SRC})
list(APPEND LIB_PUBLIC_INC ${AI_MONITOR_INC})
# svc_ai_monitor end

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
