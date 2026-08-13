##
# @file CMakeLists.txt
# @brief NiceMCU-T5 board build
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

aux_source_directory(${MODULE_PATH} LIB_SRCS)
set(LIB_PUBLIC_INC ${MODULE_PATH})

# bk_i2s headers pull in sdkconfig.h via common/sys_config.h
set(LIB_PRIVATE_INC
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/build/bk7258/tuya_app/bk7258_ap/config
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/components/bk_libs/bk7258_ap/config
)

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

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
