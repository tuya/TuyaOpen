##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS 
    "${MODULE_PATH}/lan/*.c"
    "${MODULE_PATH}/netmgr/netmgr.c"
    # Unconditional, both of them. netconn_table.c is the registry itself: its
    # rows are #ifdef'd per technology inside the file, so the file always
    # compiles even when no link driver is enabled. netmgr_cli.c is generic over
    # the registry and needs no per-technology gate either.
    "${MODULE_PATH}/netmgr/netconn_table.c"
    "${MODULE_PATH}/netmgr/netmgr_cli.c"
    # netmgr_retry.c is unconditional for a stronger reason than convenience:
    # netconn_wifi.c references it unconditionally, so gating it would be an
    # undefined reference rather than a smaller image. It is technology-free by
    # construction - pure arithmetic over its arguments, no driver header.
    "${MODULE_PATH}/netmgr/netmgr_retry.c"
    "${MODULE_PATH}/netmgr/netmgr_policy.c"
    "${MODULE_PATH}/protocol/*.c"
    "${MODULE_PATH}/schema/*.c" 
    "${MODULE_PATH}/cloud/*.c"
    "${MODULE_PATH}/file_storage/*.c"
    "${MODULE_PATH}/tls/*.c"
    "${MODULE_PATH}/transport/*.c"
    "${MODULE_PATH}/authorize/*.c"
    "${MODULE_PATH}/weather/*.c")

if(NOT CONFIG_ENABLE_DEVICE_TIMER STREQUAL "y")
    list(REMOVE_ITEM LIB_SRCS "${MODULE_PATH}/cloud/tuya_device_timer.c")
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/cloud)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/file_storage)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/protocol)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/schema)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/lan)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/tls)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/netmgr)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/transport)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/authorize)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/weather)

if(CONFIG_ENABLE_WIFI STREQUAL "y")
    file(GLOB_RECURSE WIFI_SRCS 
        "${MODULE_PATH}/netmgr/netconn_wifi.c" 
        "${MODULE_PATH}/netcfg/*.c")
    list(APPEND LIB_SRCS ${WIFI_SRCS})
    list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/netcfg)
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    file(GLOB_RECURSE WIRED_SRCS 
        "${MODULE_PATH}/netmgr/netconn_wired.c")
    list(APPEND LIB_SRCS ${WIRED_SRCS})
endif()

if (CONFIG_ENABLE_CELLULAR STREQUAL "y")
    file(GLOB_RECURSE WIRED_SRCS 
        "${MODULE_PATH}/netmgr/netconn_cellular.c")
    list(APPEND LIB_SRCS ${WIRED_SRCS})
endif()

if(CONFIG_ENABLE_BLUETOOTH STREQUAL "y")
    file(GLOB_RECURSE BLE_SRCS  
        "${MODULE_PATH}/ble/*.c")
    list(APPEND LIB_SRCS ${BLE_SRCS})
    list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/ble) 
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

    INTERFACE

    )


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

