# GD32H77X board configuration

本目录只放“板级选择入口”和“该板对 GD32H77X 平台的输入”。平台驱动、芯片几何和公共 TKL 行为仍归 `platform/GD32H77X`。

## 目录约定

```text
boards/GD32H77X/
├── Kconfig                    # 平台族公共配置入口
├── config/                    # config choice 使用的板型选择片段
│   ├── GD32H779I_DESK_BUDDY.config
│   └── GD32H779V.config
└── <BOARD>/
    ├── Kconfig                # 必须留在板级根目录，框架入口
    └── platform_config/       # 可选，本板对平台的 GD32H77 BSP 集成输入
        ├── gd32h77x.config    # 可选，覆盖芯片/平台 Kconfig 默认值
        ├── partitions.csv     # 可选，覆盖芯片默认布局
        └── ospi_profiles.csv  # 可选，仅外置 OSPI + UART 烧录
```

`Kconfig` 不放进 `platform_config/`，因为 TuyaOpen 的板级发现链路固定从
`boards/<PLATFORM>/<BOARD>/Kconfig` 进入。其它平台相关文件全部收口到
`<BOARD>/platform_config/`，不再散落在板级根目录。

## 配置形式和职责

| 配置 | 形式 | 是否必须 | 作用域 | 消费方 |
|---|---|---|---|---|
| `<BOARD>/Kconfig` | Kconfig | 必须 | 板级身份、`CHIP_CHOICE`、`BOARD_CHOICE`、板级能力默认值 | TuyaOpen Kconfig、构建参数 |
| `config/<CHIP_VARIANT>.config` | config fragment | 必须 | 只保存平台和板型 choice，供 `tos.py config choice -d` 使用 | TuyaOpen 配置选择 |
| `platform_config/gd32h77x.config` | config fragment | 可选 | 选择/覆盖平台 Kconfig 中的硬件绑定和平台差异 | TuyaOpen 前端 staging 为 `project/tuya/project.config` |
| `platform_config/partitions.csv` | CSV | 可选；省略时使用芯片默认布局 | 逻辑分区、地址、大小、设备归属 | `resolve_memory_layout.py` |
| `platform_config/ospi_profiles.csv` | CSV | 可选；存在外置 OSPI 时必须 | 外置 OSPI 的 reset/config/pin records；resolver 同时从中推导 OSPI 接线 | layout resolver、UART ROM-ISP loader |

### 当前板配置

- `Kconfig`：提供唯一的 `CHIP_CHOICE`、`BOARD_CHOICE` 和 `BOARD_CONFIG`。
- `config/<CHIP_VARIANT>.config`：只保存 `BOARD_CHOICE_GD32H77X` 和具体板型 choice，不复制平台默认值。
- `gd32h77x.config`：可选；只有板级取值需要偏离芯片默认时才创建。
- `partitions.csv`：可选；当前 `GD32H779V_START` 与 `GD32H779VW` 芯片默认布局完全一致，因此不保留板级覆盖。
- 当前 H779 板没有外置 Flash，partitions 只引用芯片内部设备；resolver 自动选择 `GD32H779VW_CNVM`/`GD32H779VW_ECNVM`。
- 当前 `GD32H779V_START` 不再包含 `platform_config/`，直接使用 `GD32H779VW` 芯片默认配置和布局。

### 可选配置

- `partitions.csv` 可省略并使用芯片默认分区。`GD32H779I_DESK_BUDDY` 不保留板级
  分区覆盖，直接使用 `GD32H779IW` 与 H779VW 对齐的 CNVM+ECNVM 默认布局。外置
  OSPI 仅通过 `partitions.csv` 和 `ospi_profiles.csv` 描述，不再维护独立设备表。
- `ospi_profiles.csv` 只在外置 OSPI 且使用 UART ROM-ISP 时需要。
- 烧录后端由 `ENABLE_GD32H77X_FLASH_BACKEND_*` choice 管理，不使用 `flash.cfg`。芯片默认来自 `platform/GD32H77X/gd32h77x_os/chips/<canonical_variant>/<canonical_variant>.config`，板级差异写入可选的 `platform_config/gd32h77x.config`。target 和 SWD target 从 `CHIP_CHOICE` 对应的 canonical chip 自动生成。
- SWD Pack 路径和 probe ID 属于主机运行环境，使用 `GD32H77X_DFP_PACK` 与 `tos.py flash -p`，不写入板级配置。
- 板级功能 `select`（例如 `ENABLE_WIFI`、`ENABLE_UART`、`ENABLE_GPIO`）可按板能力配置；应用也可以在自己的 `app_default.config` 选择具体板和应用功能，但不要把板级引脚写进应用配置。

## 分区 CSV 格式

修改或生成 `partitions.csv` 后，使用平台格式化器统一列宽：

```bash
./.venv/bin/python platform/GD32H77X/gd32h77x_os/tools/firmware/format_layout_csv.py platform/GD32H77X/gd32h77x_os/chips/<CHIP>/partitions.csv
# 如存在板级覆盖：
./.venv/bin/python platform/GD32H77X/gd32h77x_os/tools/firmware/format_layout_csv.py boards/GD32H77X/<BOARD>/platform_config/partitions.csv
```

CI 可用 `--check` 只检查不写回。当前板和所有 `gd32h77x_os/chips/*/partitions.csv` 都受 CTest 对齐检查约束。

## 两套独立配置

`boards/GD32H77X/Kconfig` 和 `platform/GD32H77X/Kconfig` 是两套独立配置树，
彼此不 `source`，也不共享生成头文件。

TuyaOpen 侧负责：

```text
boards/Kconfig
→ boards/GD32H77X/Kconfig
→ <BOARD>/Kconfig
→ CHIP_CHOICE / BOARD_CHOICE
→ using.config / using.cmake / tuya_kconfig.h
```

平台侧从构建参数接收：

```text
PLATFORM_CHIP
PLATFORM_BOARD       # 仅用于布局解析和生成元数据
GD32H77X_PROJECT_CONFIG
```

其中平台 Kconfig 的通用解析器只接收 chip 和 project config：

```text
platform/GD32H77X/Kconfig
→ gd32h77x_os/chips/<canonical_variant>/<canonical_variant>.config
→ project/<project>/project.config（可选）
→ gd32h77x_platform.config
→ gd32h77x_config.h / gd32h77x_config.cmake
```

TuyaOpen project 的 `project.config` 在 build 目录中按需生成。板级存在覆盖时：

```text
boards/GD32H77X/<BOARD>/platform_config/gd32h77x.config
→ <build>/gd32h77x/project/tuya/project.config
```

板级没有 `platform_config/` 时，例如当前 `GD32H779V_START`，不生成 project override，
直接使用 `gd32h77x_os/chips/GD32H779VW/GD32H779VW.config`。

拷贝/staging 由 `platform/GD32H77X/tuya_project_config.py` 和 TuyaOpen 平台前端负责，
不在 `gd32h77x_os` 体系内。

例如：

```text
PLATFORM_CHIP="GD32H779VW"
    → chips/GD32H779VW/GD32H779VW.config

PLATFORM_CHIP="GD32H77DIW"
    → chips/GD32H77DIW/GD32H77DIW.config
```

平台配置符号如 `ENABLE_GD32H77X_FLASH_BACKEND_*` 和硬件绑定只属于平台配置，不写入
`app_default.config`。应用代码放 CNVM 还是 ECNVM 由板级
当前芯片默认或板级覆盖的 `partitions.csv` 中是否存在 `ecnvm_code` 分区决定。
应用配置只保留 TuyaOpen 自己的通用符号和板型选择。

## 硬件绑定

如存在板级 `platform_config/`，其中不放 C 头文件。Console 和 VW553 的外设、GPIO 端口、引脚、AF、VW553 波特率及 IRQ 优先级都在 `platform/GD32H77X/Kconfig` 中定义，`gd32h77x.config` 只选择芯片或板级取值。

`platform_config.py` 会把选择映射成 `gd32h77x_config.h` 中的厂商 token；UART clock、IRQn、IRQHandler 和 GPIO clock 均由外设/端口选择派生。Console 与 VW553 不得使用同一 UART，任何两个绑定不得占用同一 GPIO port/pin，冲突会在构建前报错。

H779 当前板级选择示例：

```text
CONFIG_ENABLE_GD32H77X_CONSOLE_UART_USART5=y
CONFIG_ENABLE_GD32H77X_CONSOLE_GPIO_PORT_C=y
CONFIG_GD32H77X_CONSOLE_TX_PIN_NUMBER=6
CONFIG_GD32H77X_CONSOLE_RX_PIN_NUMBER=7
CONFIG_GD32H77X_CONSOLE_AF_NUMBER=7

CONFIG_ENABLE_GD32H77X_VW553_UART_UART4=y
CONFIG_ENABLE_GD32H77X_VW553_GPIO_PORT_B=y
CONFIG_GD32H77X_VW553_TX_PIN_NUMBER=13
CONFIG_GD32H77X_VW553_RX_PIN_NUMBER=12
CONFIG_GD32H77X_VW553_AF_NUMBER=14
CONFIG_GD32H77X_VW553_BAUD=115200
CONFIG_GD32H77X_VW553_IRQ_PRIORITY=6
CONFIG_ENABLE_GD32H77X_VW553_RESET_GPIO_PORT_C=y
CONFIG_GD32H77X_VW553_RESET_PIN_NUMBER=5
```

`*_PIN_NUMBER` 和 `*_AF_NUMBER` 是 Kconfig 数值；生成的 `GD32H77X_CONSOLE_TX_PIN`、`VW553_UART_TX_PIN` 等宏才是 `GPIO_PIN_n` / `GPIO_AF_n` 厂商 token。VW553 的 RX DMAMUX request 也由 UART choice 派生。

## Kconfig 层级

```text
TuyaOpen:
boards/Kconfig
└── boards/GD32H77X/Kconfig
    └── <BOARD>/Kconfig

GD32H77X Platform:
platform/GD32H77X/Kconfig
```

- `boards/GD32H77X/Kconfig`：只定义 TuyaOpen 通用能力和板卡 choice。
- `<BOARD>/Kconfig`：只放 `CHIP_CHOICE`、`BOARD_CHOICE` 和通用能力 select。
- `platform/GD32H77X/Kconfig`：只定义平台内部配置。
- `platform_config/gd32h77x.config`：可选，覆盖平台 Kconfig 的默认硬件绑定。
- `apps/<app>/app_default.config`：TuyaOpen 应用差分，不承载平台配置。

## 当前板

| 板卡 | 芯片 | 控制台 | VW553 | 主要布局 | 默认烧录 |
|---|---|---|---|---|---|
| `GD32H779I_DESK_BUDDY` | `GD32H779IW` | USART0，PF4/PF5，AF4 | SPI1，待 transport 适配 | 使用 `GD32H779IW` 芯片默认布局，KV 位于 ECNVM 末尾 | DFU（CNVM+ECNVM） |
| `GD32H779V_START` | `GD32H779VW` | USART5，PC6/PC7，AF7 | UART4，PB13/PB12，AF14，NRST PC5 | 使用 `GD32H779VW` 芯片默认布局，KV 位于 ECNVM 末尾 | SWD |

### GD32H779I_DESK_BUDDY bring-up

选择板型并构建：

```bash
cd <应用目录>
tos.py config choice -c GD32H779I_DESK_BUDDY.config
tos.py build
```

把 BOOT 拉高并复位，确认枚举到 `28e9:0189` 后执行：

```bash
tos.py flash
```

当前板级没有 `partitions.csv` 覆盖，直接使用 `GD32H779IW` 的默认
CNVM+ECNVM 布局。DFU bridge 会先按 4 KiB 页擦写并回读 ECNVM `ecnvm_code`，
成功后再按 64 KiB 扇区擦写并回读 CNVM boot+primary；不会写 ECNVM 末尾的 KV、
授权或校准区。完整 QIO 固件可直接执行 `tos.py flash`。

## 生成文件

以下文件由 CSV 和芯片 registry 生成，不能手工修改：

- `gd32h77x_platform.config`
- `gd32h77x_config.h`
- `gd32h77x_config.cmake`
- `gd32h77x_memory.ldinc`
- `gd32h77x_memory_layout.h`
- `gd32h77x_memory_layout.c`
- `resolved_layout.json`
- `.tufw` 内的 layout report

修改芯片默认时改 `gd32h77x_os/chips/<CHIP>/`；只有需要板级偏离时才新增 `platform_config/gd32h77x.config` 或 CSV。重新构建后平台配置和内存布局均由对应生成器重建。
