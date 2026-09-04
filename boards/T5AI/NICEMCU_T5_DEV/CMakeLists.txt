##
# @file CMakeLists.txt
# @brief NiceMCU-T5-DEV board build
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(LIB_SRCS ${MODULE_PATH}/nicemcu_t5_dev.c)
set(LIB_PUBLIC_INC ${MODULE_PATH})
set(LIB_PRIVATE_INC)

# Optional INMP441 + MAX98357 driver; bk_i2s headers need sdkconfig.h
if (CONFIG_NICEMCU_T5_DEV_I2S_AUDIO STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_audio_i2s_ext.c)
    list(APPEND LIB_PRIVATE_INC
        ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/build/bk7258/tuya_app/bk7258_ap/config
        ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/components/bk_libs/bk7258_ap/config
    )
endif()

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
