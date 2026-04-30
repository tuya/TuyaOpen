cmake_minimum_required(VERSION 3.14)
project(LVGLSimulator C)

# Load app config — edit sim_config.cmake to switch apps
include(${CMAKE_CURRENT_SOURCE_DIR}/sim_config.cmake)

# Locate TuyaOpen root (simulator lives at examples/graphics/lvgl_simulator/)
get_filename_component(TUYAOPEN_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

set(LVGL_DIR     "${TUYAOPEN_ROOT}/src/liblvgl/v9")
set(PLATFORM_DIR "${TUYAOPEN_ROOT}/platform/LINUX/tuyaos_adapter")
set(TAL_DIR      "${TUYAOPEN_ROOT}/src/tal_system")
set(COMMON_DIR   "${TUYAOPEN_ROOT}/src/common")

# Output executable to dist/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist)

# ─────────────────────────────────────────────────────────────────────────────
# Generate main.c (inject entry function name and screen resolution)
# ─────────────────────────────────────────────────────────────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_main.c.in
    ${CMAKE_BINARY_DIR}/sim_main.c
    @ONLY
)

# ─────────────────────────────────────────────────────────────────────────────
# Source file collection
# ─────────────────────────────────────────────────────────────────────────────

# LVGL core sources (includes built-in SDL driver; embedded port files excluded)
file(GLOB_RECURSE LVGL_SRCS "${LVGL_DIR}/lvgl/src/*.c")

# UI source files (SIM_UI_SRC_DIRS entries can be directories or individual .c file paths)
set(UI_SRCS "")
foreach(entry ${SIM_UI_SRC_DIRS})
    if(IS_DIRECTORY "${entry}")
        file(GLOB_RECURSE _srcs "${entry}/*.c")
    else()
        set(_srcs "${entry}")
    endif()
    list(APPEND UI_SRCS ${_srcs})
endforeach()

# platform/LINUX utility library (linked lists, hash tables, etc.)
file(GLOB UTIL_SRCS "${PLATFORM_DIR}/include/utilities/src/*.c")

# platform/LINUX TKL adapter layer (only the core modules required by TAL)
set(TKL_CORE_SRCS
    ${PLATFORM_DIR}/src/tkl_memory.c
    ${PLATFORM_DIR}/src/tkl_mutex.c
    ${PLATFORM_DIR}/src/tkl_semaphore.c
    ${PLATFORM_DIR}/src/tkl_queue.c
    ${PLATFORM_DIR}/src/tkl_thread.c
    ${PLATFORM_DIR}/src/tkl_system.c
    ${PLATFORM_DIR}/src/tkl_output.c
    ${PLATFORM_DIR}/src/tkl_fs.c
    ${PLATFORM_DIR}/src/tkl_ota.c
)

# TAL abstraction layer (tal_log.c provides PR_xxx macros;
# tal_event.c and tal_fs.c excluded — they depend on network/UART/KV headers)
set(TAL_SRCS
    ${TAL_DIR}/src/tal_api.c
    ${TAL_DIR}/src/tal_log.c
    ${TAL_DIR}/src/tal_sleep.c
    ${TAL_DIR}/src/tal_sw_timer.c
    ${TAL_DIR}/src/tal_system.c
    ${TAL_DIR}/src/tal_thread.c
    ${TAL_DIR}/src/tal_time_serivce.c
    ${TAL_DIR}/src/tal_workq_service.c
    ${TAL_DIR}/src/tal_workqueue.c
)

# ─────────────────────────────────────────────────────────────────────────────
# Executable target
# ─────────────────────────────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

add_executable(lvgl_sim
    ${CMAKE_BINARY_DIR}/sim_main.c
    ${LVGL_SRCS}
    ${UI_SRCS}
    ${UTIL_SRCS}
    ${TKL_CORE_SRCS}
    ${TAL_SRCS}
)

# ─────────────────────────────────────────────────────────────────────────────
# Compile-time definitions
# ─────────────────────────────────────────────────────────────────────────────

# Convert SIM_APP_NAME to a valid C identifier and inject as a preprocessor macro
# (read by tuya_kconfig.h to enable per-app feature macros)
string(MAKE_C_IDENTIFIER "${SIM_APP_NAME}" _sim_app_id)

target_compile_definitions(lvgl_sim PRIVATE
    LVGL_PC_SIMULATOR              # Master switch: identifies PC simulator environment
    LV_CONF_INCLUDE_SIMPLE         # Tell LVGL to locate its config via lv_conf.h
    LV_LVGL_H_INCLUDE_SIMPLE       # Tell LVGL internals to #include "lvgl.h"
    OPERATING_SYSTEM=100           # SYSTEM_LINUX: activates the Linux branch in tuya_cloud_types.h
    SIM_APP_${_sim_app_id}         # Per-app identifier, read by tuya_kconfig.h
)

# ─────────────────────────────────────────────────────────────────────────────
# Include paths (order matters: real headers before stub headers)
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(lvgl_sim PRIVATE
    # 1. Real platform/LINUX headers (provide tuya_cloud_types.h, tkl_*.h, tal_*.h)
    ${PLATFORM_DIR}/include/utilities/include   # tuya_cloud_types.h and utility headers
    ${PLATFORM_DIR}/include/system              # tkl_*.h
    ${TAL_DIR}/include                          # tal_*.h (including tal_log.h / PR_xxx)
    ${COMMON_DIR}/include                       # tuya_error_code.h, tuya_iot_config.h

    # 2. Stub headers (shadow hardware-only SDK headers such as lv_vendor.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # 3. LVGL driver path trick: resolves "../lvgl/lvgl.h" relative to the driver directory
    ${LVGL_DIR}/lvgl

    # 4. LVGL parent directory: resolves "lvgl/lvgl.h" (used by SquareLine Studio–generated code)
    ${LVGL_DIR}

    # 5. LVGL config directory (contains lv_conf.h with LVGL_PC_SIMULATOR conditionals)
    ${LVGL_DIR}/conf

    # 6. Directory of the generated main.c
    ${CMAKE_BINARY_DIR}

    # 7. App-specific headers
    ${SIM_UI_INC_DIRS}

    # 8. SDL2
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(lvgl_sim PRIVATE
    ${SDL2_LIBRARIES}
    m
    pthread
)

# ─────────────────────────────────────────────────────────────────────────────
# Status output
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "=== LVGL Simulator ===")
message(STATUS "App:        ${SIM_APP_NAME}")
message(STATUS "Resolution: ${SIM_SCREEN_W}x${SIM_SCREEN_H}")
message(STATUS "Entry func: ${SIM_ENTRY_FUNC}()")
message(STATUS "UI sources: ${SIM_UI_SRC_DIRS}")
message(STATUS "Output:     ${CMAKE_CURRENT_SOURCE_DIR}/dist/lvgl_sim")
