##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
# [tuyaopen-core integration] utilities/backoffAlgorithm sources removed:
# their symbols are provided by tuyaopen-core (tuya_utilities / backoff_algorithm,
# 见 tuyaopen-core/third_party/CMakeLists.txt 的 BUILD_INTERFACE 头路径)；
# 本地副本已删除，本模块只保留 qrcode（按 CONFIG_ENABLE_QRCODE 编入）。
set(LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC
    ${MODULE_PATH}/include)

if (CONFIG_ENABLE_QRCODE STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/qrcode/qrcodegen.c ${MODULE_PATH}/qrcode/qrencode_print.c)
    list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/qrcode)
endif()

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


