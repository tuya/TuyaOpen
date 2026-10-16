##
# @file CMakeLists.txt
# @brief netmgr（网络管理）+ netconn_*（wifi/wired 连接体）+ netcfg（配网框架）
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
# netmgr 本体无条件编入；连接体与配网按平台能力门控（与上游 tuya_cloud_service
# 的 GLOB 规则一致）：
#   CONFIG_ENABLE_WIFI=y  → netconn_wifi.c + netcfg.c + ap_netcfg.c（AP 配网）+ ap_pbkdf2.c
#   CONFIG_ENABLE_WIRED=y → netconn_wired.c
# 注：cellular 连接体（netconn_cellular.c）本仓未恢复，暂不编入。
set(LIB_SRCS ${MODULE_PATH}/src/netmgr.c)

if(CONFIG_ENABLE_WIFI STREQUAL "y")
    list(APPEND LIB_SRCS
        "${MODULE_PATH}/src/netconn_wifi.c"
        "${MODULE_PATH}/src/netcfg.c"
        "${MODULE_PATH}/src/ap_netcfg.c"
        "${MODULE_PATH}/src/ap_pbkdf2.c"
    )
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    list(APPEND LIB_SRCS
        "${MODULE_PATH}/src/netconn_wired.c"
    )
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)
set(LIB_PRIVATE_INC)

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
