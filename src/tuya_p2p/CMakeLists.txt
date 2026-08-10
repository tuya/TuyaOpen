##
# @file CMakeLists.txt
# @brief tuya_p2p — unified TAL/TKL path for LINUX and MCU (zero OS-core fork)
#/

# MODULE_PATH
if (CONFIG_ENABLE_TUYA_P2P STREQUAL "y")

# pjlib treats ARM/aarch64 as bi-endian and requires these macros before
# including pj/config.h (config_site.h is included too late for that check).
if (CONFIG_LITTLE_END STREQUAL "1")
    add_compile_definitions(PJ_IS_LITTLE_ENDIAN=1 PJ_IS_BIG_ENDIAN=0)
else()
    add_compile_definitions(PJ_IS_LITTLE_ENDIAN=0 PJ_IS_BIG_ENDIAN=1)
endif()

# One profile for all platforms: pj OS core / ICE use TAL → TKL.
add_compile_definitions(PJ_TUYAOS=1)

add_compile_options(
    -Wno-unused-const-variable
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-missing-field-initializers
    -Wno-comment
)

# MCU: lwIP socket headers from chip tree; arch/cc.h comes from OpenSDK overlay
# (no log / no bk_uart) placed BEFORE t5_os port — t5_os itself is never patched.
# gettimeofday/clock_gettime/gethostname come from platform TKL / T5 OS.
if (NOT CONFIG_PLATFORM_CHOICE STREQUAL "LINUX")
    add_compile_definitions(
        CONFIG_MSDU_RESV_HEAD_LENGTH=0
        CONFIG_MSDU_RESV_DESC_LENGTH=0
        CONFIG_MSDU_RESV_TAIL_LENGTH=0
    )
    if (DEFINED PLATFORM_PATH)
        include_directories(
            ${PLATFORM_PATH}/tuyaos/tuyaos_adapter/include/lwip_arch_overlay
            ${PLATFORM_PATH}/t5_os/ap/components/lwip_intf_v2_1/lwip-2.1.2/src/include
            ${PLATFORM_PATH}/t5_os/ap/components/lwip_intf_v2_1/lwip-2.1.2/port
            ${PLATFORM_PATH}/t5_os/ap/components/lwip_intf_v2_1/lwip-2.1.2
            ${PLATFORM_PATH}/t5_os/ap/components/bk_libs/bk7258_ap/config
            ${PLATFORM_PATH}/t5_os/build/bk7258/tuya_app/bk7258_ap/config
            ${PLATFORM_PATH}/t5_os/ap/include
            ${PLATFORM_PATH}/t5_os/ap/include/common
        )
    endif()
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/base_ice)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib_rtp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pjproject)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/svc_ipc_core)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/svc_streaming_p2p)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/local_store)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/svc_ring_buffer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ipc_cloud_store)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
