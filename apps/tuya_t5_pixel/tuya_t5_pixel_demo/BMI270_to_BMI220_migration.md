# BMI270 到 BMI220 迁移总结

## 背景

TUYA_T5AI_PIXEL 开发板上实际搭载的 IMU 芯片为 **BMI220**（chip_id = `0x26`），而非 BMI270（chip_id = `0x24`）。原 BMI270 驱动初始化时 chip_id 校验失败，即使绕过校验，BMI270 的配置固件与 BMI220 不兼容，导致传感器数据全部为零。

## 问题根因

BMI2xx 系列传感器在上电后必须上传一份 **8192 字节的配置固件（config file）**，芯片内部微引擎才能正常工作。每个型号的固件不通用：

| 芯片     | Chip ID | 配置固件数组名           | 固件来源 |
|----------|---------|--------------------------|----------|
| BMI270   | 0x24    | `bmi270_config_file`     | Bosch BMI270 SDK |
| BMI220   | 0x26    | `bmi260_config_file`     | ChromeOS EC `third_party/bmi220` (v2.47.1) |

上传错误的固件后，`INTERNAL_STATUS` 寄存器（0x21）返回 `0x00`（未初始化），加速度计和陀螺仪输出全为零。

## 修改清单

### 1. 新增文件

| 文件 | 说明 |
|------|------|
| `boards/T5AI/TUYA_T5AI_PIXEL/board_bmi220_api.h` | BMI220 驱动头文件，定义 `bmi220_dev_t`、`bmi220_sensor_data_t` 结构体及 API |
| `boards/T5AI/TUYA_T5AI_PIXEL/board_bmi220_api.c` | BMI220 驱动实现，基于 Bosch BMI2 库，使用 `bmi260_config_file` 固件 |
| `src/peripherals/imu/bmi270/bmi260_config_file.c` | BMI220 专用配置固件（8192 字节），数组名 `bmi260_config_file[]` |

### 2. 修改文件

#### `src/peripherals/imu/bmi270/bmi2.c`（第 1907 行）

```c
// 修改前
if (chip_id == dev->chip_id)

// 修改后
if (chip_id == dev->chip_id || chip_id == 0x26)
```

> 允许 BMI220 的 chip_id `0x26` 通过 Bosch BMI2 库的校验。

#### `boards/T5AI/TUYA_T5AI_PIXEL/board_bmi220_api.c`

```c
// 修改前
extern const uint8_t bmi270_config_file[];
bmi2_dev_220.config_file_ptr = bmi270_config_file;

// 修改后
extern const uint8_t bmi260_config_file[];
bmi2_dev_220.config_file_ptr = bmi260_config_file;
```

> 指向 BMI220 专用的配置固件。

#### `apps/tuya_t5_pixel/tuya_t5_pixel_demo/src/tuya_main.c`

- 新增 `#include "board_bmi220_api.h"`
- 新增 IMU 抽象层：`imu_type_t` 枚举（`IMU_TYPE_NONE` / `IMU_TYPE_BMI220` / `IMU_TYPE_BMI270`）
- 新增 `imu_read_data()` 和 `imu_is_ready()` 统一接口
- `user_main()` 中优先初始化 BMI220，失败则回退到 BMI270
- `sand_update_physics()` 使用 `imu_read_data()` 替代直接调用 BMI270 API

#### `src/peripherals/imu/Kconfig`

新增 `ENABLE_IMU_BMI220` 配置选项。

#### `boards/T5AI/TUYA_T5AI_PIXEL/Kconfig`

新增 `select ENABLE_IMU_BMI220`，与 `ENABLE_IMU_BMI270` 同时启用。

## 架构设计

```
tuya_main.c
    |
    |-- imu_read_data() / imu_is_ready()    ← 统一抽象层
    |       |
    |       |-- g_imu_type == IMU_TYPE_BMI220 → board_bmi220_read_data()
    |       |-- g_imu_type == IMU_TYPE_BMI270 → board_bmi270_read_data()
    |
    |-- user_main()
            |-- board_bmi220_register()      ← 优先尝试 BMI220
            |-- board_bmi270_register()      ← 回退 BMI270
```

BMI270 原有代码完整保留，两个驱动通过不同接口独立初始化，运行时根据 `g_imu_type` 选择对应驱动读取数据。

## BMI220 驱动配置参数

| 参数 | 值 |
|------|----|
| I2C 端口 | `TUYA_I2C_NUM_0` |
| I2C 地址 | `0x68`（SDO = GND） |
| I2C 速率 | 400 kHz |
| SCL / SDA | GPIO20 / GPIO21 |
| 加速度计 | 16G 量程，200Hz ODR |
| 陀螺仪 | 2000 dps 量程，200Hz ODR |

## 验证结果

```
IMU data: acc(7.32, -5.68, -3.85) gyr(-7.93, 9.58, 61.40)
IMU data: acc(7.19, -5.01, -4.19) gyr(-2.08, 2.38, 0.18)
IMU data: acc(6.57, -3.36, -3.98) gyr(19.04, -80.81, -57.80)
```

加速度计和陀螺仪数据正常输出，沙子物理模式中重力方向响应正确。
