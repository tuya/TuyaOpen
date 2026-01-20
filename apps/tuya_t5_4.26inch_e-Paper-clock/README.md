# 4.26" e-Paper Clock (Tuya IoT)

This example builds a simple e-Paper clock and integrates Tuya IoT features:

- BLE/AP network provisioning (Tuya APP pairing)
- Time sync (Tuya cloud timestamp, optional NTP fallback)
- DP settings:
  - `time_mode` (`24h` / `12h`)
  - `night_mode` (mapped to 2 themes: dark/light)
- A hardware key to re-enter pairing mode (clear netinfo + restart pairing flow)

## Hardware

### Display wiring (example)

The default pins are defined in `lib/Config/DEV_Config.h` / `lib/Config/DEV_Config.c`. Typical SPI wiring:

|Signal|Default|
|---|---|
|SCLK|`TUYA_GPIO_NUM_2`|
|MOSI|`TUYA_GPIO_NUM_4`|
|CS|`TUYA_GPIO_NUM_3`|
|DC|`TUYA_GPIO_NUM_7`|
|RST|`TUYA_GPIO_NUM_8`|
|BUSY|`TUYA_GPIO_NUM_6`|
|PWR|`TUYA_GPIO_NUM_28`|

If your hardware differs, update the pin definitions in `lib/Config/DEV_Config.h` (or override via compile definitions).

### Key button (re-pair)

By default, the example assumes a key on `TUYA_GPIO_NUM_12` (active-low). Configure in `examples/tuya_config.h`:

- `EPD_CLOCK_NETCFG_KEY_ENABLE`
- `EPD_CLOCK_NETCFG_KEY_PIN`
- `EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL`
- `EPD_CLOCK_NETCFG_KEY_LONGPRESS_MS`

Pressing the key triggers:

1. Clear saved WiFi (`netinfo`)
2. `tuya_iot_reset()` to re-enter pairing flow

## Tuya IoT Configuration

Edit `examples/tuya_config.h`:

- `TUYA_PRODUCT_ID`: your PID from Tuya IoT Platform
- `TUYA_OPENSDK_UUID` / `TUYA_OPENSDK_AUTHKEY`: only used as a fallback when OTP/KV license read fails
- `EPD_CLOCK_TZ_SECONDS`: timezone offset in seconds (default `UTC+8`)

Note: Seeing `OTP license read failed` in logs is normal if your module is not OTP-burned; the demo will use the fallback UUID/AUTHKEY in `examples/tuya_config.h`.

## Build / Flash / Monitor

From this directory:

```powershell
python ..\..\..\tos.py build
python ..\..\..\tos.py flash -p COM3
python ..\..\..\tos.py monitor
```

Tips:

- Some boards expose two UARTs (one for download, one for logs). If logs are garbled, try the other COM port in the monitor tool.
- If Windows reports the COM device is not functioning (e.g. error 31), replug the board and check the USB-to-UART driver in Device Manager.

## Pairing / Provisioning

1. Power on the device.
2. Use Tuya Smart / Smart Life APP to add a BLE/AP device.
3. If you need to re-pair, press the key to clear stored WiFi and restart pairing.

## DP: Time Mode & Night Mode

This example listens for `TUYA_EVENT_DP_RECEIVE_OBJ` and updates local settings.

### DPIDs

The code assumes the following DPIDs:

- `time_mode` DPID = `18`
- `night_mode` DPID = `28`

If your product uses different DPIDs, update them in `examples/EPD_clock.c`.

### `time_mode` mapping

- `24h` -> enum `0`
- `12h` -> enum `1`

### `night_mode` mapping (2 themes)

Your product schema may define `night_mode` with a wider range (e.g. `mode_1`..`mode_5`). This demo maps it to 2 UI themes:

- `mode_1` (enum `0`) -> **dark theme** (black background, white text)
- any other value -> **light theme** (white background, black text)

### Persistence

Settings are stored in KV so they survive reboot:

- `clock.time_mode`
- `clock.theme`

## Files to read first

- `examples/EPD_clock.c`: Tuya IoT init, DP handling, key handling, main UI loop
- `examples/clock_ui.c`: theme + 12/24-hour rendering
- `examples/tuya_config.h`: product/license config and key config
