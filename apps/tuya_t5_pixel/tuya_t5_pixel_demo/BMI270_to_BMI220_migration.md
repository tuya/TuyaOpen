# BMI270 to BMI220 Migration Summary

## Background

The TUYA_T5AI_PIXEL board ships with a **BMI220** IMU (chip_id = `0x26`), not the BMI270 (chip_id = `0x24`). The original BMI270 driver failed during chip_id validation. Even when that check was bypassed, the BMI270 config firmware is incompatible with BMI220, causing all sensor outputs to read zero.

## Root Cause

BMI2xx sensors require an **8192-byte config firmware upload** after power-on for the internal micro-engine to function. Each variant needs its own firmware — they are not interchangeable:

| Chip   | Chip ID | Config Array Name        | Firmware Source |
|--------|---------|--------------------------|-----------------|
| BMI270 | 0x24    | `bmi270_config_file`     | Bosch BMI270 SDK |
| BMI220 | 0x26    | `bmi260_config_file`     | ChromeOS EC `third_party/bmi220` (v2.47.1) |

Uploading the wrong firmware results in `INTERNAL_STATUS` register (0x21) returning `0x00` (not initialized), with accelerometer and gyroscope outputs stuck at zero.

## Changes

### 1. New Files

| File | Description |
|------|-------------|
| `boards/T5AI/TUYA_T5AI_PIXEL/board_bmi220_api.h` | BMI220 driver header — `bmi220_dev_t`, `bmi220_sensor_data_t` structs and API |
| `boards/T5AI/TUYA_T5AI_PIXEL/board_bmi220_api.c` | BMI220 driver implementation using Bosch BMI2 library with `bmi260_config_file` |
| `src/peripherals/imu/bmi220/bmi260_config_file.c` | BMI220-specific config firmware (8192 bytes), array `bmi260_config_file[]` |

### 2. Modified Files

#### `apps/tuya_t5_pixel/tuya_t5_pixel_demo/src/tuya_main.c`

- Added `#include "board_bmi220_api.h"`
- Added IMU abstraction layer: `imu_type_t` enum (`IMU_TYPE_NONE` / `IMU_TYPE_BMI220` / `IMU_TYPE_BMI270`)
- Added unified `imu_read_data()` and `imu_is_ready()` interface
- `user_main()` tries BMI220 first, falls back to BMI270
- `sand_update_physics()` uses `imu_read_data()` instead of direct BMI270 calls

#### `src/peripherals/imu/Kconfig`

Added `ENABLE_IMU_BMI220` config option.

#### `boards/T5AI/TUYA_T5AI_PIXEL/Kconfig`

Added `select ENABLE_IMU_BMI220` alongside `select ENABLE_IMU_BMI270`.

#### `src/peripherals/imu/CMakeLists.txt`

Added BMI220 source directory glob under `CONFIG_ENABLE_IMU_BMI220`.

## Architecture

```
tuya_main.c
    |
    |-- imu_read_data() / imu_is_ready()    <-- unified abstraction
    |       |
    |       |-- g_imu_type == IMU_TYPE_BMI220 -> board_bmi220_read_data()
    |       |-- g_imu_type == IMU_TYPE_BMI270 -> board_bmi270_read_data()
    |
    |-- user_main()
            |-- board_bmi220_register()      <-- try BMI220 first
            |-- board_bmi270_register()      <-- fallback to BMI270
```

The BMI270 driver code is fully preserved. Both drivers initialize independently via separate interfaces. At runtime, `g_imu_type` selects the active driver.

### Why bmi270_* API functions appear in the BMI220 driver

`bmi270_get_sensor_config()`, `bmi270_set_sensor_config()`, and `bmi270_sensor_enable()` operate on generic BMI2 registers (ACC_CONF, GYR_CONF, POWER_CTRL) that are identical across BMI220/BMI260/BMI270. This is safe and intentional — these are not BMI270-specific functions despite their naming.

## BMI220 Driver Configuration

| Parameter | Value |
|-----------|-------|
| I2C Port | `TUYA_I2C_NUM_0` |
| I2C Address | `0x68` (SDO = GND) |
| I2C Speed | 400 kHz |
| SCL / SDA | GPIO20 / GPIO21 |
| Accelerometer | 16G range, 200Hz ODR |
| Gyroscope | 2000 dps range, 200Hz ODR |

## Verification

```
IMU data: acc(7.32, -5.68, -3.85) gyr(-7.93, 9.58, 61.40)
IMU data: acc(7.19, -5.01, -4.19) gyr(-2.08, 2.38, 0.18)
IMU data: acc(6.57, -3.36, -3.98) gyr(19.04, -80.81, -57.80)
```

Accelerometer and gyroscope data output correctly. Sand physics mode responds to gravity direction as expected.
