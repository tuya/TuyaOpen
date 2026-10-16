##
# @file CMakeLists.txt
# @brief 交付仓保留的服务组件（core 未覆盖的部分），由平台构建统一编入
#/

# lib_kv：KV 存储实现组件（目录名刻意不带 tal_ 前缀，避免与 core 的 tal_kv
# 模块（iot-core/tal_system/tal_kv.c 分发器 + tal_kv.h）混淆）。
# 本组件提供：core 分发器所需的 ops 卡（LittleFS 介质 + AES-128-CBC 加密）、
# 组件侧扩展（tal_kv_free / tal_kv_cmd / tal_kv_serialize_* / tal_lfs_get，
# 见 lib_kv/src/lib_kv.c）与 littlefs 介质本身。
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib_kv)

# tal_driver：TAL 驱动层（uart/dma2d）——tal_cli 的串口控制台依赖其 tal_uart_*，
# 由本组件提供（core 无 tal_uart；实现走平台 tkl_uart）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_driver)

# tal_image：图像工具层（jpeg 编解码 tjpgd / 缩放 / 旋转 / dither / yuv422），
# 供 src/peripherals 的 tdl_display、app_printer 等使用（core 无此层）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_image)

# tal_cli：串口命令行（sys_*/kv_* 命令组；netmgr/fs 组已按本仓适配摘除/门控），
# 由应用调用 tal_cli_init() 启用（开关 CONFIG_ENABLE_SERIAL_CLI_CMD）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_cli)

# netmgr：网络管理（netmgr + netconn_wifi/wired + netcfg/ap_netcfg 配网框架），
# 连接体按 CONFIG_ENABLE_WIFI/WIRED 门控（见组件内 CMakeLists）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/netmgr)

# tal_sleep：CPU 睡眠/低功耗管理（tal_wifi 的 lp 接口依赖 tal_cpu_lp_*）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_sleep)

# authorize：设备授权码读写（KV + 平台 OTP），上游随 tuya_cloud_service 编译，
# 本仓 tuya_cloud_service 已被 core 覆盖，故独立成组件（apps 的
# tuya_authorize_init/read 调用面不变）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/authorize)

# tal_wifi / tal_wired：netmgr 连接体的下层 TAL 层（各自按 CONFIG_ENABLE_WIFI/
# WIRED 门控，组件自身 CMakeLists 已带条件）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_wifi)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_wired)

# 兼容层（tal_compat.h）：把上游 tal_* 接口重定向到本仓等价实现（tkl/契约），
# 减少 apps/ 应用改动；目录随服务组件统一 include 路径全局可见
list(APPEND COMPONENT_PUBINC "${CMAKE_CURRENT_LIST_DIR}/include")

# compat：兼容层的函数级实现（上游 tal_psram_* 等——平台预编译库按旧名引用，
# 必须提供同名真实符号；宏级重定向见 include/tal_compat.h）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/compat)

# 子目录把各自的库/include 追加到本作用域，这里再上抛给顶层
# （与 src/ai_components/CMakeLists.txt 的传播方式一致）
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
