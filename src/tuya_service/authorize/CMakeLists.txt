##
# @file CMakeLists.txt
# @brief tuya_authorize：设备授权码（UUID/AUTHKEY）读写——KV 存储 + 平台 OTP
#        （tuyaopen_license）双通道；应用经 tuya_authorize_read() 取凭据，
#        取不到时回退编译期 TUYA_OPENSDK_UUID/AUTHKEY。
#        上游由 tuya_cloud_service 一并编译（glob），本仓 tuya_cloud_service
#        已被 core 覆盖，故独立成组件；tuya_authorize_init() 同时向 tal_cli
#        注册 auth/auth-read/auth-reset/read_mac 命令组。
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS（本组件沿用上游扁平布局：.c/.h 直接位于组件根，无 src/ 子目录）
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


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
