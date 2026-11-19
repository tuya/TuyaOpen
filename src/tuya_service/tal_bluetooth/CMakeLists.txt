##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_BLUETOOTH STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# Only the bundled NimBLE host and its TKL port are built here. The legacy
# src/tal_bluetooth.c wrapper duplicates the core BLE API and its headers
# conflict with the core aliases, so it stays out of the build.
set(LIB_SRCS "")
set(LIB_PUBLIC_INC "")
set(LIB_PRIVATE_INC "")

if (CONFIG_ENABLE_NIMBLE STREQUAL "y")

set(NIMBLE nimble)
file(GLOB_RECURSE  NIMBLE_SRCS  "${MODULE_PATH}/${NIMBLE}/*.c")
list(APPEND LIB_SRCS ${NIMBLE_SRCS})

list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/${NIMBLE}/include)
set(LIB_PRIVATE_INC ${MODULE_PATH}/${NIMBLE}/host)

endif()

########################################
# Target Configure
########################################
if (NOT LIB_SRCS)
    return()
endif()

add_library(${MODULE_NAME})

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

# Bundled NimBLE uses the BSD LIST_HEAD macro, which collides with the Tuya
# list helper force-included by tal_compat.h. Keep warnings but do not fail
# this vendored stack on them.
target_compile_options(${MODULE_NAME} PRIVATE -Wno-error)


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()

