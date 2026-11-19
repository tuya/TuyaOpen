##
# @file CMakeLists.txt
# @brief tal_sleep：CPU 睡眠/低功耗管理（上游 tal_system 的一部分，core 有意
#        未纳入——tal_wifi 的低功耗接口（tal_wifi_lp_enable/disable）依赖其
#        tal_cpu_lp_enable/disable，故随 tal_wifi 一并恢复；实现走平台
#        tkl_cpu_sleep_mode_set（platform/LINUX 已有）
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

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
