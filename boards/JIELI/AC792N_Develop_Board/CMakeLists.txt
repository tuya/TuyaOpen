set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(MODULE_NAME AC792N_Develop_Board)

add_library(${MODULE_NAME} INTERFACE)
target_include_directories(${MODULE_NAME} INTERFACE ${MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/..")

# The wl83 audio adapter's capture path reads this board's JIELI_AUDIO_* mic
# routing macros; the staged vendor Makefile stages this header as
# tuya_board_audio_config.h and force-includes it for tkl_audio.c. Hand the
# CMake adapter target the same header so both builds compile the source with
# identical board data.
set(JIELI_BOARD_AUDIO_CONFIG "${CMAKE_CURRENT_LIST_DIR}/audio_config.h")

# The TKL adapter sources are shared across Jieli chips; per-chip deltas are
# funnelled through tuyaos_adapter/include/chip_conf.h.
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
