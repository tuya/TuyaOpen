# ultra_lowpower_demo — WiFi 超低功耗示例

演示 TuyaOpen 的 WiFi 超低功耗（ULP / DTIM）功能：设备保持连云在线的同时，通过 WiFi DTIM 省电 + CPU 低压深睡，把稳态平均电流压到 **~300µA**（T5AI 实测）。

> 注意：这是“保持在线”的省电（随时能收下行），天生是 **µA~mA** 级；真正的 **深睡/待机（关 WiFi、离线）** 是另一套方案，不在本示例范围。

## 原理

基于 `switch_demo`，额外接入 `src/tal_wifi_ulp/` 的 lpmgr 引用计数功耗管理器，移植自 tuyaos-ai 的成熟架构：

- **lpmgr 生命周期锁**：配网期保持全速（NETCFG/LONG_NETCFG），MQTT 连上后释放，进入 dtim10 基线。
- **dev_evt 逐操作锁**：SDK 在每个网络操作（DNS/TCP/MQTT connect/publish、DP、OTA、STA connect、reset）前后 `tuya_dev_evt_notify()`，`__dev_evt_cb` 据此在操作期间短暂保持唤醒、操作完立即回到深睡。
- **BLE 断电**：T5 的 BT 控制器由 CP 核开机上电，必须在上线后 `tuya_ble_deinit()`（→ `tkl_hci_deinit` 发 IPC 通知 CP 核）才能真正断电，否则基线卡在 ~1.5mA。本示例在 `TUYA_EVENT_MQTT_CONNECTED` 一次性 deinit（仅 ULP 构建生效）。

关键接线见 `src/tuya_main.c`：
```c
tuya_wifi_ulp_init();               // 初始化 lpmgr + 注册 dev_evt 回调 + 生命周期锁
lpmgr_set_lps_dtim(TY_LP_DITM_10);  // 目标 DTIM，可选 10/20/30（越大越省、下行延迟越大）
lpmgr_register(TY_LP_APP_USED);     // APP 基线锁
```

## 配置 / 编译 / 烧录

1. 填入授权码：把 `src/tuya_config.h` 里的 `TUYA_OPENSDK_UUID` / `TUYA_OPENSDK_AUTHKEY` 换成你自己的（或用 `tuya_config_secrets.h`）。
2. 编译烧录：
   ```bash
   cd apps/tuya_cloud/ultra_lowpower_demo
   tos build
   tos flash
   ```
   目标板：T5AI（`TUYA_T5AI_BOARD`）。

## 测功耗

- 测点：模组供电轨总电流，设备**已入网、空闲**。
- 用带平均功能的功耗仪（如 Nordic PPK2）。
- ⚠️ 测之前把日志级别调高（`tuya_main.c` 里 `tal_log_init` 的 `TAL_LOG_LEVEL_DEBUG` → `NOTICE`），否则串口持续打日志会让 CPU 醒着、读数假性偏高。
- 预期：MQTT 连上后日志出现 `WiFi DTIM changed 0 -> 10`、`CPU sleep changed ... -> 30000ms`、`deinit BLE`；稳态基线进深睡，平均 ~300µA，周期性 beacon 唤醒尖峰。

## 调 DTIM

`lpmgr_set_lps_dtim(TY_LP_DITM_10 | TY_LP_DITM_20 | TY_LP_DITM_30)`：dtim 越大唤醒越稀、越省电，但下行（APP 控制/推送）延迟越大。注意实际唤醒周期还受路由器 beacon 间隔 × DTIM 广播周期限制。
