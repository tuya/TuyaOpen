# ultra_lowpower_demo — WiFi Ultra Low Power example

Demonstrates TuyaOpen's WiFi ultra-low-power (ULP / DTIM) feature: the device
stays connected to the cloud while saving power via WiFi DTIM + CPU low-voltage
deep sleep, bringing the steady-state average current down to **~550µA** (measured
on T5AI).

> Note: this is "stay-online" power saving (downlink always receivable), which is
> inherently in the µA~mA range. True deep-sleep/standby (WiFi off, offline) is a
> different mechanism and out of scope here.

## How it works

Based on `switch_demo`, plus the `src/tal_wifi_ulp/` lpmgr reference-counting power
manager (ported from the tuyaos-ai architecture):

- **lpmgr lifecycle locks**: stay fully awake during netcfg (NETCFG/LONG_NETCFG),
  released once MQTT connects, then settle to the dtim10 baseline.
- **dev_evt per-operation locks**: the SDK calls `tuya_dev_evt_notify()` before/after
  each network operation (DNS/TCP/MQTT connect/publish, DP, OTA, STA connect, reset);
  `__dev_evt_cb` briefly holds a wakelock for the operation and drops back to deep
  sleep right after.
- **BLE teardown**: on T5 the BT controller is powered by the CP core at boot and only
  `tuya_ble_deinit()` (→ `tkl_hci_deinit`, an AP→CP IPC) actually powers it down;
  otherwise the idle floor stays at ~1.5mA. This demo deinits BLE once on
  `TUYA_EVENT_MQTT_CONNECTED` (ULP builds only).

Key wiring in `src/tuya_main.c`:
```c
tuya_wifi_ulp_init();               // init lpmgr + register dev_evt cb + lifecycle locks
lpmgr_set_lps_dtim(TY_LP_DITM_10);  // target DTIM: 10/20/30 (larger = lower power, higher downlink latency)
lpmgr_register(TY_LP_APP_USED);     // app baseline lock
```

## Build / flash

1. Set your license: replace `TUYA_OPENSDK_UUID` / `TUYA_OPENSDK_AUTHKEY` in
   `src/tuya_config.h` (or use `tuya_config_secrets.h`).
2. Build & flash (target board: T5AI / `TUYA_T5AI_BOARD`):
   ```bash
   cd apps/tuya_cloud/ultra_lowpower_demo
   tos build
   tos flash
   ```

## Measuring power

- Measure total module-rail current with the device connected and idle, using an
  averaging power analyzer (e.g. Nordic PPK2).
- ⚠️ Raise the log level first (`TAL_LOG_LEVEL_DEBUG` → `NOTICE` in `tal_log_init`),
  otherwise continuous UART logging keeps the CPU awake and inflates the reading.
- Expect `WiFi DTIM changed 0 -> 10`, `CPU sleep changed ... -> 30000ms`, `deinit BLE`
  after MQTT connects; steady-state baseline in deep sleep, ~300µA average with
  periodic beacon-wake spikes.
