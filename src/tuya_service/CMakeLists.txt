##
# @file CMakeLists.txt
# @brief Service components not covered by tuyaopen-core, built by the platform.
#/

# lib_kv: KV storage (LittleFS medium + AES-128-CBC encryption) and the ops card
# consumed by core's tal_kv dispatcher. The directory name intentionally has no
# tal_ prefix to avoid confusion with core's tal_kv module.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib_kv)

# liblwip: bundled lwIP for platforms with ENABLE_LIBLWIP.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/liblwip)

# tal_driver: TAL driver layer (uart/dma2d). tal_cli depends on tal_uart_*.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_driver)

# tal_image: image utilities (jpeg codec, scale, rotate, dither, yuv422) used by
# tdl_display and app_printer.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_image)

# tal_cli: serial command line, enabled by the application via tal_cli_init()
# (CONFIG_ENABLE_SERIAL_CLI_CMD).
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_cli)

# netmgr: network manager and netcfg/ap_netcfg framework; connection bodies are
# gated by CONFIG_ENABLE_WIFI/WIRED.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/netmgr)

# tal_sleep: CPU sleep / low-power management; tal_wifi LP interfaces use
# tal_cpu_lp_*.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_sleep)

# authorize: device license read/write (KV + platform OTP).
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/authorize)

# tal_wifi / tal_wired: TAL network layers under netmgr, gated by
# CONFIG_ENABLE_WIFI/WIRED.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_wifi)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_wired)

# tal_bluetooth: bundled NimBLE host + TKL BLE port (ENABLE_NIMBLE).
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_bluetooth)

# tal_wifi_ulp: Wi-Fi ultra-low-power/lpmgr backend used by tuya_pm.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_wifi_ulp)

# tuya_pm: device power-level policy manager.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tuya_pm)

# core_port: shared core ops card (port/tuyaopen/tuyaopen-core_ops.c).
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/core_port)

# Compatibility layer (tal_compat.h): redirects upstream tal_* interfaces to
# this repository's equivalents; the include directory is globally visible.
list(APPEND COMPONENT_PUBINC "${CMAKE_CURRENT_LIST_DIR}/include")

# compat: function-level implementations of the compatibility layer (e.g.
# tal_psram_*), required as real symbols by prebuilt platform libraries.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/compat)

# Propagate libraries and include paths collected by the subdirectories.
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
