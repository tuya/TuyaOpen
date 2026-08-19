# XTEINK_X4_PRO (ESP32-S3)

Board support for the **Xteink X4 Pro** e-paper reader hardware (ESP32-S3,
16 MB flash, 8 MB octal PSRAM).

This is the *Pro* revision of the Xteink X4 (ESP32-C3, `XTEINK_X4`). Pro
additions compared to the base board:

| Feature | Pro revision |
|---|---|
| Frontlight | dual PWM warm/cold white LEDs |
| Touchscreen | GT911 on the display + one capacitive Home key |
| Buttons | power + left + right (digital, active-LOW) |
| Battery | CW2017 I2C fuel gauge (not an ADC pin) |
| SoC | ESP32-S3 (was ESP32-C3) |

Hardware facts (pin map, power sequencing, waveforms) are taken from the
crosspoint-reader / FreeInk sources vendored in this workspace.

## Files

| File | Purpose |
|---|---|
| `board_config.h` | pin numbers and bus constants (`X4PRO_*`) |
| `board_com_api.h` | public board API (`board_x4pro_*`) |
| `xteink_x4_pro.c` | rails, `board_register_hardware()`, wrappers, shutdown |
| `xteink_x4_pro_epd.c/.h` | SSD1677 800x480 e-paper driver (tkl_spi/tkl_gpio) |
| `xteink_x4_pro_touch.c/.h` | GT911 touchscreen + Home key (tkl_i2c/tkl_gpio) |
| `xteink_x4_pro_frontlight.c/.h` | warm/cold frontlight (tkl_pwm) |
| `xteink_x4_pro_buttons.c/.h` | power/left/right keys (tkl_gpio) |
| `xteink_x4_pro_battery.c/.h` | CW2017 gauge with BATINFO profile (tkl_i2c) |
| `xteink_x4_pro_sdcard.c/.h` | SDMMC 1-bit mount + POSIX helpers |
| `example/lvgl_demo/` | LVGL demo app (display, touch, IO, SD, settings UI) |

## Hardware map

- **EPD (SSD1677, 800x480, SPI @ 20 MHz)**: SCLK=12, MOSI=11, CS=13, DC=18,
  RST=14, BUSY=6 (active-HIGH).
- **Touch (GT911, I2C0 @ 400 kHz)**: SDA=39, SCL=38, INT=10, RST=4, power
  GPIO2 active-LOW, address 0x5D (fallback 0x14). Panel is portrait-mounted:
  driver swaps X/Y and flips Y. Status register 0x814E bit 0x10 = Home key.
- **Frontlight**: cool=GPIO8, warm=GPIO9, 10 kHz PWM, active-HIGH.
  `warm_duty = total * warmth / 100; cool_duty = total - warm_duty`.
- **Buttons**: left=GPIO0, right=GPIO7, power=GPIO3 (pull-up, active-LOW).
- **Battery**: CW2017 at I2C 0x63; needs the 80-byte BATINFO profile and
  reset sequence at init (see `xteink_x4_pro_battery.c`).
- **SD card**: native SDMMC slot 1, 1-bit (CLK=41, CMD=42, DAT0=40), power
  GPIO5 active-LOW. Mount pulses GPIO5 HIGH 80 ms then LOW; it must stay LOW
  afterwards (HIGH breaks block reads, err 0x107). Mounted at `/sdcard`.
- **Power rails**: GPIO1 is the master peripheral rail — drive HIGH first
  (10 ms before the rest). GPIO2 = GT911 rail (active-LOW), GPIO5 = SD rail.

## TAL-only policy

All peripheral init uses the TuyaOpen wrappers only (`tkl_gpio`, `tkl_spi`,
`tkl_i2c`, `tkl_pwm`, `tkl_io_pinmux_config`, `tal_system_sleep`, ...).
**No interrupts are used anywhere**: buttons, touch, the EPD BUSY pin and the
GT911 INT pin are all polled through `tkl_gpio`, so no ISR registration
(ESP or tkl) exists in this BSP.

There are exactly **two ESP-IDF exceptions**, each confined to one file and
commented in source, because TuyaOpen has no wrapper for them:

1. `xteink_x4_pro_sdcard.c` — reduced to the irreducible three calls:
   `esp_vfs_fat_sdmmc_mount()` / `sdmmc_read_sectors()` / `esp_vfs_fat_info()`.
   TuyaOpen's `tkl_fs_mount()` mounts SD over SPI only, and this card is
   silent to SPI-mode CMD0 (hardware-confirmed), so native SDMMC is
   unavoidable; the ESP newlib ships no `sys/statvfs.h`, so the disk-usage
   query also stays on the ESP VFS path. Unmount uses `tkl_fs_unmount()`,
   all file ops POSIX, the SD rail `tkl_gpio`.
2. `xteink_x4_pro.c` — `esp_deep_sleep_*` in `board_x4pro_power_shutdown()`
   (no TAL deep-sleep wrapper).

Notable adapter details used by this BSP:

- The S3 FSPI host's default miso/cs pins (13/10) would collide with EPD CS
  and GT911 INT, so the EPD bus init pinmuxes GPIO0 to `TUYA_SPI0_MISO`/`CS`
  which maps them to -1 (disabled).
- Touch and battery gauge share I2C0; the tkl_i2c adapter reuses the bus
  handle, so both `board_x4pro_touch_init()` and
  `board_x4pro_battery_init()` can be called independently.

## Build

```bash
cd boards/ESP32/XTEINK_X4_PRO/example/lvgl_demo
tos.py build
```

Output goes to `example/lvgl_demo/dist/`.
