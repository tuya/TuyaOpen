##
# @file CMakeLists.txt
# @brief pjproject — single source set: TAL os_core, select ioqueue (no unix/epoll fork)
#/

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")

get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(LIB_SRCS "")

set(LIB_PUBLIC_INC
    ${MODULE_PATH}/pjlib/include
    ${MODULE_PATH}/pjlib-util/include
    ${MODULE_PATH}/pjmedia/include
    ${MODULE_PATH}/pjnath/include)

file(GLOB_RECURSE  PJPROJECT_SRCS
    "${MODULE_PATH}/pjlib/src/*.c"
    "${MODULE_PATH}/pjlib-util/src/*.c"
    "${MODULE_PATH}/pjmedia/src/*.c"
    "${MODULE_PATH}/pjnath/src/*.c")

list(REMOVE_ITEM PJPROJECT_SRCS
    "${MODULE_PATH}/pjlib/src/pj/os_rwmutex.c"
    "${MODULE_PATH}/pjlib/src/pj/ioqueue_common_abs.c"
    "${MODULE_PATH}/pjlib-util/src/pjlib-util/scanner_cis_bitwise.c"
    "${MODULE_PATH}/pjlib-util/src/pjlib-util/scanner_cis_uint.c"
    # Drop platform-specific forks — one TAL/select path for LINUX + MCU
    "${MODULE_PATH}/pjlib/src/pj/os_core_unix.c"
    "${MODULE_PATH}/pjlib/src/pj/ioqueue_epoll.c"
    # Socket layer runs on TAL: sock_tal.c / sock_select_tal.c /
    # addr_resolv_tal.c / ip_helper_tal.c replace the BSD implementations,
    # so pjlib needs no BSD socket headers and no chip network stack.
    "${MODULE_PATH}/pjlib/src/pj/sock_bsd.c"
    "${MODULE_PATH}/pjlib/src/pj/sock_select.c"
    "${MODULE_PATH}/pjlib/src/pj/addr_resolv_sock.c"
    "${MODULE_PATH}/pjlib/src/pj/ip_helper_generic.c"
    "${MODULE_PATH}/pjlib/src/pj/ssl_sock_ossl.c"
    "${MODULE_PATH}/pjlib/src/pj/ssl_sock_gtls.c"
    "${MODULE_PATH}/pjlib/src/pj/ssl_sock_dump.c"
    "${MODULE_PATH}/pjlib/src/pj/ssl_sock_imp_common.c"
    "${MODULE_PATH}/pjlib/src/pj/ssl_sock_common.c"
    "${MODULE_PATH}/pjlib/src/pj/os_time_bsd.c"
)

message("PJPROJECT_SRCS: ${PJPROJECT_SRCS}")
list(APPEND LIB_SRCS ${PJPROJECT_SRCS})

########################################
# Target Configure
########################################
message("LIB_SRCS: ${LIB_SRCS}")
add_library(${MODULE_NAME} STATIC ${LIB_SRCS})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    INTERFACE
        ${LIB_INTERFACE_INC}

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
