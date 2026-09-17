set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_NAME AC7916A)

add_library(${MODULE_NAME} INTERFACE)
target_include_directories(${MODULE_NAME} INTERFACE ${MODULE_PATH})

add_subdirectory(
    "${TOP_SOURCE_DIR}/platform/JIELI/tuyaos/tuyaos_adapter"
    "${CMAKE_CURRENT_BINARY_DIR}/jieli_tuya_adapter"
)
list(APPEND COMPONENT_LIBS jieli_tuya_adapter)
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
