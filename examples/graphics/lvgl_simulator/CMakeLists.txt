cmake_minimum_required(VERSION 3.14)
project(LVGLSimulator C)

# 加载用户配置（修改 sim_config.cmake 切换 App）
include(${CMAKE_CURRENT_SOURCE_DIR}/sim_config.cmake)

# 定位 TuyaOpen 根目录（simulator 在 examples/graphics/lvgl_simulator/）
get_filename_component(TUYAOPEN_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

set(LVGL_DIR     "${TUYAOPEN_ROOT}/src/liblvgl/v9")
set(PLATFORM_DIR "${TUYAOPEN_ROOT}/platform/LINUX/tuyaos_adapter")
set(TAL_DIR      "${TUYAOPEN_ROOT}/src/tal_system")
set(COMMON_DIR   "${TUYAOPEN_ROOT}/src/common")

# 可执行文件输出到 dist/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist)

# ─────────────────────────────────────────────────────────────────────────────
# 生成 main.c（注入入口函数名和分辨率）
# ─────────────────────────────────────────────────────────────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_main.c.in
    ${CMAKE_BINARY_DIR}/sim_main.c
    @ONLY
)

# ─────────────────────────────────────────────────────────────────────────────
# 源文件收集
# ─────────────────────────────────────────────────────────────────────────────

# LVGL 核心源码（含内置 SDL 驱动；排除 port/ 的嵌入式端口文件）
file(GLOB_RECURSE LVGL_SRCS "${LVGL_DIR}/lvgl/src/*.c")

# UI 业务代码
set(UI_SRCS "")
foreach(dir ${SIM_UI_SRC_DIRS})
    file(GLOB_RECURSE _srcs "${dir}/*.c")
    list(APPEND UI_SRCS ${_srcs})
endforeach()

# platform/LINUX 工具库（链表、哈希表等）
file(GLOB UTIL_SRCS "${PLATFORM_DIR}/include/utilities/src/*.c")

# platform/LINUX TKL 适配层（仅选取 TAL 依赖的核心模块）
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

# TAL 抽象层（含 tal_log.c 提供 PR_xxx 宏实现；排除依赖网络/UART/KV 头的 tal_event/fs）
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
# 可执行目标
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
# 编译宏
# ─────────────────────────────────────────────────────────────────────────────
target_compile_definitions(lvgl_sim PRIVATE
    LVGL_PC_SIMULATOR           # 总开关：控制 lv_conf.h / ENABLE_LVGL_HARDWARE
    LV_CONF_INCLUDE_SIMPLE      # LVGL 使用 lv_conf.h 而非相对路径查找
    LV_LVGL_H_INCLUDE_SIMPLE    # 内部头以 "lvgl.h" 方式 include
    OPERATING_SYSTEM=100        # SYSTEM_LINUX：激活 tuya_cloud_types.h 的 Linux 分支
)

# ─────────────────────────────────────────────────────────────────────────────
# Include 路径（顺序关键：stub 头必须在最前面）
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(lvgl_sim PRIVATE
    # 1. platform/LINUX 真实头文件（优先于 stub，提供 tuya_cloud_types.h 和 tkl_*.h / tal_*.h）
    ${PLATFORM_DIR}/include/utilities/include   # tuya_cloud_types.h 及工具类头
    ${PLATFORM_DIR}/include/system              # tkl_*.h
    ${TAL_DIR}/include                          # tal_*.h（含 tal_log.h / PR_xxx）
    ${COMMON_DIR}/include                       # tuya_error_code.h, tuya_iot_config.h

    # 2. Stub 头（屏蔽硬件 SDK 专属头：lv_vendor.h 等）
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # 3. LVGL 驱动路径 trick：使 "../lvgl/lvgl.h" 解析为 lvgl/lvgl.h
    ${LVGL_DIR}/lvgl

    # 4. LVGL conf（含 LVGL_PC_SIMULATOR 条件分支的 lv_conf.h）
    ${LVGL_DIR}/conf

    # 5. 生成的 main.c 所在目录
    ${CMAKE_BINARY_DIR}

    # 6. App 专属头（game_pet.h 等）
    ${SIM_UI_INC_DIRS}

    # 7. SDL2
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(lvgl_sim PRIVATE
    ${SDL2_LIBRARIES}
    m
    pthread
)

# ─────────────────────────────────────────────────────────────────────────────
# 调试信息
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "=== LVGL Simulator ===")
message(STATUS "App:        ${SIM_APP_NAME}")
message(STATUS "Resolution: ${SIM_SCREEN_W}x${SIM_SCREEN_H}")
message(STATUS "Entry func: ${SIM_ENTRY_FUNC}()")
message(STATUS "UI sources: ${SIM_UI_SRC_DIRS}")
message(STATUS "Output:     ${CMAKE_CURRENT_SOURCE_DIR}/dist/lvgl_sim")
