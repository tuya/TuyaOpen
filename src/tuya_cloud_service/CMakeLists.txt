##
# @file CMakeLists.txt
# @brief Container for the cloud-service subsystems. It builds nothing itself.
#/

# This directory used to declare one library and list every subdirectory's
# sources, includes and feature gates in this file. Each subsystem now owns its
# own CMakeLists.txt, following the pattern src/ai_components uses: the child
# declares its library, appends it to COMPONENT_LIBS and COMPONENT_PUBINC, and
# this file only re-propagates the result upward.
#
# Why splitting one library into twelve is safe here: the top-level CMakeLists
# does not link these against each other. It collects $<TARGET_OBJECTS:> from
# every entry in COMPONENT_LIBS and re-archives them into a single `tuyaos`
# static library, so there is no link order to get wrong and no cycle to break -
# and deal_with_components() hands every component the full public include set
# afterwards, so cross-subsystem headers keep resolving exactly as before.
#
# The children name their targets tcs_<dir> rather than <dir>: CMake target
# names are global and src/peripherals/transport already builds a `transport`.
#
# Adding a subsystem means adding a directory with its own CMakeLists.txt and
# one add_subdirectory() line here. Nothing else in this file should grow again.

set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${COMP_MODULE_PATH}/authorize)
add_subdirectory(${COMP_MODULE_PATH}/ble)
add_subdirectory(${COMP_MODULE_PATH}/cloud)
add_subdirectory(${COMP_MODULE_PATH}/file_storage)
add_subdirectory(${COMP_MODULE_PATH}/lan)
add_subdirectory(${COMP_MODULE_PATH}/netcfg)
add_subdirectory(${COMP_MODULE_PATH}/netmgr)
add_subdirectory(${COMP_MODULE_PATH}/protocol)
add_subdirectory(${COMP_MODULE_PATH}/schema)
add_subdirectory(${COMP_MODULE_PATH}/tls)
add_subdirectory(${COMP_MODULE_PATH}/transport)
add_subdirectory(${COMP_MODULE_PATH}/weather)

########################################
# Layer Configure
########################################
# Re-propagate what the subdirectories appended (each via PARENT_SCOPE) up to
# the top-level scope.
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
