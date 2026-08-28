##
# @file CMakeLists.txt
# @brief Network manager: link selection, the route it publishes, and the
#        drivers behind it. Layout is documented in docs/netmgr/extension_guide.md 1.3.
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_DIR_NAME ${MODULE_PATH} NAME)
# See the note on the prefix in any sibling CMakeLists here.
set(MODULE_NAME tcs_${MODULE_DIR_NAME})

# Listed file by file rather than globbed, because half of them are gated and a
# glob would hide which half. The directories mirror the extension paths:
# src/conn, src/policy, src/probe, src/cli.
#
# Four unconditional; netmgr_probe.c is listed here and removed again below when
# the board did not ask for it, which is the ENABLE_DEVICE_TIMER pattern.
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
set(LIB_SRCS
    "${MODULE_PATH}/src/netmgr.c"
    "${MODULE_PATH}/src/conn/netconn_table.c"
    "${MODULE_PATH}/src/cli/netmgr_cli.c"
    "${MODULE_PATH}/src/policy/netmgr_policy.c"
    "${MODULE_PATH}/src/policy/netmgr_retry.c"
    "${MODULE_PATH}/src/probe/netmgr_probe.c"
    )

# One driver per link technology, each behind the switch that says the product
# has that hardware. src/conn/netconn_table.c above stays in regardless: it is
# the table these rows register into, and it #ifdefs its own rows.
if(CONFIG_ENABLE_WIFI STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/src/conn/netconn_wifi.c")
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/src/conn/netconn_wired.c")
endif()

if(CONFIG_ENABLE_CELLULAR STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/src/conn/netconn_cellular.c")
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
    list(REMOVE_ITEM LIB_SRCS "${MODULE_PATH}/src/probe/netmgr_probe.c")
endif()

# netmgr's implementation root, so its own sources can include netmgr_priv.h
# while nothing outside the library can.
#
# It points at src/ and not at the module root on purpose: the module root also
# contains include/, so a source could reach a public header as
# "include/netmgr.h" - a spelling that should not exist. src/ holds nothing but
# implementation, so this entry grants exactly what it says.
#
# netmgr_priv.h sits directly in src/ while the sources sit one level further
# down (conn/, policy/, probe/, cli/), so this one entry serves all of them and
# no subdirectory needs an include path of its own.
set(LIB_PRIVATE_INC ${MODULE_PATH}/src)

# netmgr exports include/ and not the module root, which is what keeps
# netmgr_priv.h private. include/ deliberately does NOT mirror the source
# subdirectories - netmgr_policy.h lives there while netmgr_policy.c lives in
# src/policy/. Splitting the headers to match would mean exporting four
# directories instead of one, and the single exported directory is the whole
# mechanism behind the previous sentence. The mismatch is the price, and it is
# the cheaper side of the trade.
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
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
