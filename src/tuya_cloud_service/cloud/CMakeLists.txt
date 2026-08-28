##
# @file CMakeLists.txt
# @brief The cloud client: activation, MQTT session, OTA, timers, device meta.
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_DIR_NAME ${MODULE_PATH} NAME)
# See the note on the prefix in any sibling CMakeLists here.
set(MODULE_NAME tcs_${MODULE_DIR_NAME})

file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/*.c")

# The cloud timer service, dropped when the board did not ask for it. This is
# the same shape as the netmgr probe gate: glob everything, then remove the one
# file the feature switch owns, so the exception is visible next to the rule.
if(NOT CONFIG_ENABLE_DEVICE_TIMER STREQUAL "y")
    list(REMOVE_ITEM LIB_SRCS "${MODULE_PATH}/tuya_device_timer.c")
endif()

set(LIB_PUBLIC_INC ${MODULE_PATH})

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
