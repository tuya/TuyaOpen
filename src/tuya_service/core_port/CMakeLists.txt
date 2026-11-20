##
# @file CMakeLists.txt
# @brief Builds core's shared ops card (port/tuyaopen/tuyaopen-core_ops.c).
#/

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(TUYAOPEN_CORE_PORT_DIR "${MODULE_PATH}/../../../tuyaopen-core/port/tuyaopen")

add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${TUYAOPEN_CORE_PORT_DIR}/tuyaopen-core_ops.c
    )

# The ops contract headers (core_api_ops.h, core_kv_ops.h, core_netif_ops.h,
# ...) live in core's port directory; expose them from this single place so no
# platform has to add the include path itself.
target_include_directories(${MODULE_NAME}
    PUBLIC
        ${TUYAOPEN_CORE_PORT_DIR}/interface
    )

# TKL returns int32_t where the core contracts use int.
target_compile_options(${MODULE_NAME} PRIVATE
    -Wno-error
    -Wno-error=incompatible-pointer-types
)

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
