##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
# 目录名用 lib_kv（避免与 core 的 tal_kv 模块混淆），但**目标名保持 tal_kv**：
# 应用层有按目标名的引用——apps/tuya.ai/your_robot_dog/CMakeLists.txt 的
# `if(TARGET tal_kv)` / get_target_property(tal_kv ...) / target_compile_definitions(tal_kv ...)
# （T5AI 构建时剔重 LittleFS 实现），改名会让应用层失效；保持目标名 = 应用层零改动。
set(MODULE_NAME tal_kv)

# LIB_SRCS
set(LIB_SRCS ${MODULE_PATH}/src/lib_kv.c ${MODULE_PATH}/src/kv_serialize.c)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

set(LIB_PRIVATE_INC  ${MODULE_PATH}/port)

# [交付仓收编] 存储后端固定为 TuyaOpen 自带 littlefs（随组件编入）：
# 本仓平台没有 tal_fs 卡，上游的 ENABLE_FILE_SYSTEM（键=平台文件系统里的文件）
# 分支不可用，tal_kv.c 只保留 littlefs 直连路径。
set(LITTLEFS ${MODULE_PATH}/littlefs/lfs_util.c ${MODULE_PATH}/littlefs/lfs.c)
list(APPEND LIB_SRCS ${LITTLEFS})
list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/littlefs)
add_definitions(-DLFS_CONFIG=lfs_config.h)


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