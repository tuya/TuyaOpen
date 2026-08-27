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
    "${MODULE_PATH}/netmgr/src/netmgr.c"
    # Four unconditional; netmgr_probe.c is globbed here and removed again below
    # when the board did not ask for it, which is the ENABLE_DEVICE_TIMER pattern.
    #
    # netconn_table.c is the registry itself: its rows are #ifdef'd per technology
    # inside the file, so the file always compiles even when no link driver is
    # enabled. netmgr_cli.c is generic over the registry and needs no
    # per-technology gate either.
    #
    # netmgr_policy.c and netmgr_retry.c are unconditional for a stronger reason
    # than convenience. netmgr.c's whole link-selection path goes through
    # netmgr_policy_select_default(), so gating netmgr_policy.c would not shrink
    # the image, it would mean reintroducing the pre-M3 list walk as a SECOND
    # implementation of link selection - two rankings to keep in agreement, for a
    # few hundred bytes. netmgr_retry.c backs the wifi association back-off, which
    # predates M3 and is not optional. Both are also technology-free by
    # construction, pure arithmetic over their arguments with no driver header
    # included, so there is nothing for an ENABLE_<TECH> gate to protect either.
    "${MODULE_PATH}/netmgr/src/conn/netconn_table.c"
    "${MODULE_PATH}/netmgr/src/cli/netmgr_cli.c"
    "${MODULE_PATH}/netmgr/src/policy/netmgr_policy.c"
    "${MODULE_PATH}/netmgr/src/probe/netmgr_probe.c"
    "${MODULE_PATH}/netmgr/src/policy/netmgr_retry.c"
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

# The passive reachability backend, dropped when the board did not ask for it.
#
# This is the M4 "degrade policy/probe on small-flash platforms" item, and the
# decision it records is: gate the PROBE, do not gate the policy, and gate on the
# feature switch rather than on a flash-size threshold. The reasoning, written
# here because this line is where the next reader will meet it:
#
#   - what it saves, measured rather than estimated. netmgr_probe.c.o built for
#     T5AI with the tree's own platform/tools/gcc-arm-none-eabi-10.3-2021.10:
#     408 B of .text across its four functions, 272 B of PR_* format strings,
#     12 B for the netmgr_probe_backend_mqtt descriptor and 8 B of .bss. About
#     692 B of flash. On T5AI that is noise; on a 2 MB part it is still 692 B a
#     wifi-only product has no use for;
#   - the size is not the main argument, which is why the gate is worth the
#     #ifdef it costs in netmgr.c. With CONFIG_ENABLE_NETMGR_PROBE off the policy
#     has probe_enable FALSE, and netmgr_probe_report() drops every verdict at
#     that gate. A linked-in backend would hold two tal_event subscriptions and
#     run two callbacks per MQTT session transition to produce values that are
#     thrown away. Gating it out makes "off" mean off rather than "on and
#     ignored";
#   - a flash-size threshold was the alternative and it is worse: it would make
#     the same board behave differently depending on the part it is fitted to,
#     which is exactly the surprise a netmgr default must not spring on anyone.
#     ENABLE_NETMGR_PROBE is one decision, taken once, by the product;
#   - netmgr_policy.c is deliberately NOT gated. See the comment above.
#
# netmgr.c guards its single reference to netmgr_probe_backend_mqtt with the same
# symbol; without that guard this removal is an undefined reference rather than a
# smaller image. A product that installs its own backend through
# netmgr_probe_backend_set() is unaffected, because that path never names the
# built-in one.
if(NOT CONFIG_ENABLE_NETMGR_PROBE STREQUAL "y")
    list(REMOVE_ITEM LIB_SRCS "${MODULE_PATH}/netmgr/src/probe/netmgr_probe.c")
endif()

# LIB_PRIVATE_INC
#
# netmgr's implementation root, so its own sources can include netmgr_priv.h
# while nothing outside the library can.
#
# It points at netmgr/src, not at the module root, on purpose: the module root
# also contains include/, so a source could reach a public header as
# "include/netmgr.h" - a spelling that should not exist. netmgr/src holds
# nothing but implementation, so this entry grants exactly what it says.
#
# netmgr_priv.h sits directly in netmgr/src while the sources sit one level
# further down (conn/, policy/, probe/, cli/), so this one entry serves all of
# them and no subdirectory needs an include path of its own.
#
# This list was referenced by target_include_directories() below and never set,
# which is why the netmgr directory had to be PUBLIC for its own .c files to
# compile - a PUBLIC include applies to the target itself, so making the sources
# build also published every header beside them.
set(LIB_PRIVATE_INC ${MODULE_PATH}/netmgr/src)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/cloud)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/file_storage)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/protocol)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/schema)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/lan)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/tls)
# netmgr exports include/ and not the module root, which is what keeps
# netmgr_priv.h private. include/ deliberately does NOT mirror the source
# subdirectories - netmgr_policy.h lives here while netmgr_policy.c lives in
# netmgr/src/policy/. Splitting the headers to match would mean exporting four
# directories instead of one, and the single exported directory is the whole
# mechanism behind the previous sentence. The mismatch is the price, and it is
# the cheaper side of the trade. Everything else in that directory is a contract: the
# link/policy/probe/registry headers an app or a board includes, plus
# netmgr_retry.h, which netmgr_policy.h embeds by value and so cannot be private
# while the policy header is public.
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/netmgr/include)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/transport)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/authorize)
list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/weather)

if(CONFIG_ENABLE_WIFI STREQUAL "y")
    file(GLOB_RECURSE WIFI_SRCS 
        "${MODULE_PATH}/netmgr/src/conn/netconn_wifi.c" 
        "${MODULE_PATH}/netcfg/*.c")
    list(APPEND LIB_SRCS ${WIFI_SRCS})
    list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/netcfg)
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    file(GLOB_RECURSE WIRED_SRCS 
        "${MODULE_PATH}/netmgr/src/conn/netconn_wired.c")
    list(APPEND LIB_SRCS ${WIRED_SRCS})
endif()

if (CONFIG_ENABLE_CELLULAR STREQUAL "y")
    file(GLOB_RECURSE WIRED_SRCS 
        "${MODULE_PATH}/netmgr/src/conn/netconn_cellular.c")
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

