set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_NAME AC79_DevKitBoard)

add_library(${MODULE_NAME} INTERFACE)
target_include_directories(${MODULE_NAME} INTERFACE ${MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/..")

add_subdirectory(
    "${TOP_SOURCE_DIR}/platform/JIELI/tuyaos/tuyaos_adapter"
    "${CMAKE_CURRENT_BINARY_DIR}/jieli_tuya_adapter"
)
list(APPEND COMPONENT_LIBS jieli_tuya_adapter)
add_library(jieli_board_api STATIC "${CMAKE_CURRENT_LIST_DIR}/../board_com_api.c")
target_include_directories(jieli_board_api PRIVATE ${CMAKE_CURRENT_LIST_DIR})
list(APPEND COMPONENT_LIBS jieli_board_api)
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC "${CMAKE_CURRENT_LIST_DIR}/..")
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
