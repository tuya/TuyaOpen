##
# @file CMakeLists.txt
# @brief compat：交付仓兼容层的函数级实现（tal_compat.h 的补充）。
#        当前提供上游 tal_psram_* 系列（平台预编译库按旧名引用，须有同名真实符号，
#        见 src/tal_compat.c 头注释）；宏级重定向仍集中在
#        src/tuya_service/include/tal_compat.h。
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
