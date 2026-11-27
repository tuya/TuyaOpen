# GD32H779I DESK BUDDY PSRAM 适配设计

- 状态：间接访问模式已通过 Desk Buddy 实板全容量验收；memory-mapped 模式仍未验收
- 适用板型：`GD32H779I_DESK_BUDDY`
- 适用芯片：`GD32H779IW`
- PSRAM 器件：`APS6404L-3SQR-SN`
- 文档日期：2026-11-23
- 关联文档：
  - `PSRAM_ALLOCATION.md`
  - `PIN_BRINGUP.md`
  - `../doc/C5333729_PSRAM(伪静态)_APS6404L-3SQR-SN_规格书_WJ1953228(1).PDF`
  - `../doc/H779_TY1  SCH 20261116.pdf`

## 0. 当前状态（2026-11-23）

已完成：

- GD32H77X 平台 PSRAM Kconfig 和引脚/AF 配置生成。
- OSPI/OSPIM 控制器通用层。
- PSRAM 设备抽象层和 `APS6404L-3SQR` profile。
- 间接访问模式：初始化后不进入 memory-mapped，公开
  `gd32h77x_psram_read/write/fill(offset, ...)` API。
- 间接传输按 1 KiB 页边界和 tCEM 预算分块。
- 4 MiB 偏移量动态池，元数据位于内部 SRAM，支持 32 Byte 对齐、拆分、合并、
  generation 防过期句柄和 mutex 并发保护。
- 资源缓存层，支持 24 个资源条目、流式 `begin/append/commit`、随机分块读取、
  abort、remove 和 hit/miss 统计。
- PSRAM 环形缓冲层，支持跨回卷读写、部分写入、水位统计和 mutex 保护。
- 语音上行 PCM 生产者/消费者路径，以及下行 MP3 ring 到 `tdl_audio_play()` 路径。
- Desk Buddy 板级默认使用 `GD32H77X_PSRAM_ACCESS_INDIRECT`。
- `ENABLE_EXT_RAM` 与 mapped 模式绑定；间接模式不创建普通指针堆。
- 独立测试工程 `examples/system/psram`。

实板已验证：

- MFID=`0x0D`、KGD=`0x5D`，OSPI 未进入 memory-mapped 模式。
- 非对齐 1/3/13/139/141/257 Byte 读写。
- 1 KiB 页边界、8 MiB 容量末尾和相邻区域保护。
- `fill`、NULL、越界、溢出和零长度参数。
- 8 MiB 全容量分块写入和完整回读。
- 4 MiB 动态池基础分配/释放、统计、重复释放和过期句柄拒绝。
- 600 次随机碎片压力，释放后恢复完整 4 MiB 和单一空闲块。
- 双任务并发分配、读写和释放。
- 真实 `lvgl1.jpg` 161627 Byte 流式装载到缓存并逐块回读一致。
- 双任务并发缓存读取。
- 16 kHz/1ch/16bit PCM 上行 100 帧、64000 Byte，消费者并行排空并通过顺序校验。
- `hello_tuya.mp3` 25533 Byte 经 PSRAM ring 分块送入 MP3 播放设备；下行按
  文件码率约 57.8 kbps 限速，并在尾帧后等待 300 ms 再停止，实听无截断破音。
- 1 KiB 分块缓存读取约 3180 KiB/s，4 KiB 分块约 3478 KiB/s。
- `examples/system/psram` 输出 `[PSRAM] RESULT: PASS`（128 项 PASS）。
- `platform/GD32H77X/tests/test_platform_config.py` 全部通过。

仍未验证：

- memory-mapped byte/half-word 写和通用 PSRAM 指针堆。
- DMA 直接访问 `0x9000_0000`。
- 长时间刷新压力和整板业务回归。

### 实板诊断记录（2026-11-18）

- OSPI 初始化阶段停在 `PROBE`，读回 KGD 为 `0x00`。
- 已按 APS6404L 协议修正：`0x9F` 后需要 3 字节地址，再读取 MFID/KGD/EID。
- 软件 SPI 绕开 OSPI 验证，`SYSCFG_PMCFG0=0x0B000000`：
  - `PA1SWON=1`，普通 PA1 数字 I/O 有效。
  - `PC2SWON=0`，PC2_C 数字 I/O 有效。
- 软件 SPI 两种 SI/SO 方向均测试：
  - `PA2-MOSI / PB0-MISO`
  - `PB0-MOSI / PA2-MISO`
- 两种方向下 ID 均为全 `FF`，地址 0 的 `0x02` 写入后 `0x03` 读回也全为 `FF`。

上述早期软件 SPI 结论已被后续 OSPI 总线抓包修正。真正的 MCU 侧问题有两处：

- PSRAM 六个引脚的 AF 值最初按表格视觉对齐，除了 CS 外出现错位；已按数据手册
  实板 BGA 球号和逐脚波形最终确认：`PE11=AF11`、`PB2=AF9`、`PA2=AF6`、
  `PB0=AF4`、`PA3=AF6`、`PA1=AF9`。
- OSPI 间接读在“带地址的读命令”路径中没有更新 `OSPI_INS`，导致 `0x9F`
  命令实际沿用上一帧的 `0x99`；已在 OSPI 通用层修复并增加回归检查。

修复后 START 的 CS、SCK、IO0 帧已经正确。Desk Buddy 的 U7 装配、供电、
连通性和 ID/KGD 返回值仍需在目标板上继续验收。

需要检查：

- U7 是否实装，丝印是否为 `APS6404L-3SQR-SN`。
- U7 pin8 VDD 对 pin4 GND 是否为 3.3 V。
- pin7 SIO3 和 pin3 SIO2 在 SPI 模式未使用并为高阻；QPI 模式下分别作为 IO3/IO2，检查无外部强驱动冲突。
- pin1 CE 是否连接到 PE11，并在访问时拉低。
- pin6 SCK 是否连接到 PB2，并有时钟。
- pin5 SI/IO0、pin2 SO/IO1 是否分别连接到 PA2、PB0。
- PA3/PA1 对应的实际 BGA 焊盘和 PCB 网络是否与实板映射一致。

## 1. 目标与范围

本设计用于给 Desk Buddy 增加 8 MiB 外置 PSRAM 支持，并向 TuyaOpen 提供以下 TKL 能力：

```c
void *tkl_system_psram_malloc(size_t size);
void tkl_system_psram_free(void *ptr);
void *tkl_system_psram_calloc(size_t nitems, size_t size);
void *tkl_system_psram_realloc(void *ptr, size_t size);
int tkl_system_psram_get_free_heap_size(void);
```

范围包括：

- OSPI0/OSPIM 引脚、时钟和 PSRAM 底层驱动
- PSRAM 间接模式初始化和 memory-mapped 模式
- 独立 PSRAM 堆及 TKL 内存接口
- 板级 `BOARD_CONFIG` 声明、平台 PSRAM 参数配置、初始化和错误处理
- bring-up、回归和验收要求

不在本阶段范围内：

- 把代码、`.data` 或 `.bss` 静态链接进 PSRAM
- XIP 执行
- OTA、文件系统或预加载资源的业务实现
- 未经验证就把 DMA 目标缓冲放入 PSRAM
- 未经验证就启用 Cortex-M7 D-Cache

## 2. 硬件事实

### 2.1 器件规格

`APS6404L-3SQR-SN` 的关键参数：

| 项目 | 参数 |
|---|---|
| 容量 | 64 Mbit / 8 MiB |
| 电压 | 2.7 V 至 3.6 V，板上为 3.3 V |
| 接口 | SPI/QPI，SDR |
| 地址范围 | A[22:0] |
| 页大小 | 1024 Byte |
| 线性 burst 最高频率 | 84 MHz |
| 32 Byte wrap burst 最高频率 | 3.3 V 下 109 MHz |
| CE# 最大连续低电平时间 | 标准温度等级 8 us；扩展等级 4 us |
| CE# 两次 burst 最小高电平时间 | 18 ns |
| 刷新 | 自管理，但 CE# 长时间保持低会阻塞刷新 |

本板器件为 `SN` 标准温度封装，没有 `X` 扩展温度后缀，按 `tCEM <= 8 us` 设计。

### 2.2 原理图与 PCB 引脚

实板 BGA 球号和逐脚波形确认，PSRAM 接在 OSPI0/OSPIM Port0：

| APS6404 信号 | MCU 引脚 | OSPIM 功能 | AF | 备注 |
|---|---|---|---|---|
| CE# | PE11 | OSPIM_P0_CSN | AF11 | 硬件片选 |
| SCK | PB2 | OSPIM_P0_SCK | AF9 | 时钟 |
| SI/SIO0 | PA2 | OSPIM_P0_IO0 | AF6 | 数据 0 |
| SO/SIO1 | PB0 | OSPIM_P0_IO1 | AF4 | 数据 1 |
| SIO2（QPI IO2） | PA3 | OSPIM_P0_IO2 | AF6 | SPI 模式未使用；QPI 模式双向数据 |
| SIO3（QPI IO3） | PA1 | OSPIM_P0_IO3 | AF9 | SPI 模式未使用；QPI 模式双向数据，必须启用普通 PA1 数字 I/O |
| VDD | M_3V3 | - | - | 板上 3.3 V |
| VSS | GND | - | - | 地 |

模拟开关要求：

- 本板不使用 `PC2_C`。
- `PA1SWON=1`：让普通 PA1 的数字 I/O 有效，禁止 PA1_C 数字功能。
- 修改模拟开关时必须使用读改写，保留其他模拟开关位。

### 2.3 GPIO 表勘误

旧原理图/网表字符串存在 PSRAM 引脚命名与实际封装不一致的问题。2026-11-18
按 U7 物理引脚和 MCU BGA 球号逐脚验证，最终映射为：

- `U7 pin3 = N5 = PA3 = OSPIM_P0_IO2 (AF6)`
- `U7 pin6 = P7 = PB2 = OSPIM_P0_SCK (AF9)`

因此旧资料中的 `PA3/OSPIM_SCK`、`PC2C/OSPIM_D2` 不能用于本板最终软件配置。

### 2.4 与 LCD 的关系

LCD 和 PSRAM 使用不同控制器：

- LCD：SPI3，PE12/PE14/PE13/PE9/PE10，CS 为 PC4。
- PSRAM：OSPI0/OSPIM，PB2/PA2/PB0/PA3/PA1，CS 为 PE11。

因此：

- 不能复用当前 `tkl_qspi` 后端实现 PSRAM；该后端映射到 SPI3，是 LCD 控制器。
- PSRAM 需要使用平台私有 OSPI0 驱动。
- OSPI0 PSRAM 与 SPI3 LCD 可并行存在，但必须做联合回归。

## 3. 总体软件架构

### 3.1 分层设计

不能只有“TKL malloc -> OSPI 驱动”两层，建议明确分为五层：

```text
业务 / TAL
  |
  +-- tkl_system_malloc/free --------> 内部 SRAM / FreeRTOS heap_4
  |
  +-- tkl_system_psram_malloc/free --> PSRAM 堆服务层
                                         |
                                         v
                                   PSRAM 设备抽象层
                                         |
                                         v
                                   PSRAM 芯片 profile 驱动
                                         |
                                         v
                                   GD32 OSPI 控制器通用层
                                         |
                                         v
                                   GD32 SDK OSPI/OSPIM HAL
```

各层职责：

| 层 | 职责 | 是否感知具体 PSRAM |
|---|---|---|
| PSRAM 堆服务层 | malloc/free/calloc/realloc、对齐、锁、统计 | 否，只要求已映射的 base/size |
| PSRAM 设备抽象层 | 设备生命周期、能力、状态、profile ops | 否，按接口调用 |
| PSRAM 芯片 profile | APS6404L 命令、dummy、复位、ID、最大时钟、tCEM | 是 |
| OSPI 控制器通用层 | OSPIM port、GPIO/RCC、indirect transfer、memory-mapped、abort、等待、互斥 | 否，只执行通用 OSPI 事务 |
| GD32 SDK HAL | GD32H77X 标准外设和寄存器操作 | 否 |

### 3.2 为什么要控制器通用层

OSPI0/OSPIM 的共性问题不应写进 PSRAM 芯片 profile：

- SCK/CS/IO0-IO3 引脚配置
- OSPIM Port0/Port1 选择
- OSPI 时钟源和分频
- 有界等待、超时、abort 和状态清理
- indirect read/write 事务
- memory-mapped 进入、退出和 timeout
- 多个 Flash/PSRAM 客户端之间的 OSPI0 互斥

这些能力放到控制器通用层后，PSRAM profile 只描述：

- 复位序列
- ID 读取和期望值
- 读/写命令
- dummy cycle
- 地址宽度和线宽
- burst/wrap 模式
- 最大时钟
- CS 最大低电平时间
- memory-mapped read/write 帧格式

后续如果其他器件也使用 OSPI0，可以共享控制器层，但 NOR、PSRAM 等协议仍各自独立。

### 3.3 抽象接口建议

PSRAM 设备抽象只覆盖控制路径，不覆盖 memory-mapped 数据热路径：

```c
typedef struct psram_device psram_device_t;

typedef struct {
    const char *name;
    uint32_t capacity_bytes;
    uint32_t max_clock_hz;

    OPERATE_RET (*probe)(psram_device_t *dev);
    OPERATE_RET (*reset)(psram_device_t *dev);
    OPERATE_RET (*configure_memory_mapped)(psram_device_t *dev);
    OPERATE_RET (*selftest)(psram_device_t *dev);
} psram_profile_ops_t;

struct psram_device {
    const psram_profile_ops_t *ops;
    void *controller;
    uintptr_t mapped_base;
    uint32_t capacity_bytes;
    uint32_t clock_hz;
    bool ready;
};
```

OSPI 控制器通用层建议提供类似接口：

```c
OPERATE_RET gd32h77x_ospi_controller_init(
    gd32h77x_ospi_controller_t *controller,
    const gd32h77x_ospi_controller_cfg_t *cfg);

OPERATE_RET gd32h77x_ospi_indirect_transfer(
    gd32h77x_ospi_controller_t *controller,
    const gd32h77x_ospi_command_t *command);

OPERATE_RET gd32h77x_ospi_enter_memory_mapped(
    gd32h77x_ospi_controller_t *controller,
    const gd32h77x_ospi_memory_mapped_cfg_t *cfg);
```

抽象原则：

- init、probe、reset、memory-mapped 配置等控制路径可以走 ops。
- `tkl_system_psram_malloc` 返回的指针在运行期直接做 CPU load/store，不经过函数指针。
- 第一版单板通常只有一个 PSRAM，可以使用编译期选择的 profile 和静态实例，避免动态注册开销。
- profile 必须按器件实现，不能只靠修改引脚就假设其他 PSRAM 完全兼容。

设计原则：

- `tkl_system_malloc/free` 继续管理内部 SRAM，不与 PSRAM 堆混用。
- PSRAM 使用独立堆，避免 FreeRTOS 全局堆跨越不同访问属性的内存。
- `tkl_system_psram_free` 只接受 PSRAM 堆返回的指针。
- 初始化前或初始化失败后，PSRAM malloc 返回 `NULL`，不静默回退到 SRAM。
- 初始化失败时，`board_register_hardware()` 返回错误，防止后续依赖 PSRAM 的业务带病运行。

## 4. OSPI0 驱动设计

建议新增平台私有模块：

- `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_ospi.c/.h`
  - OSPI/OSPIM 控制器通用层
- `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_psram.c/.h`
  - PSRAM 设备抽象层和公共状态管理
- `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_psram_aps6404l.c/.h`
  - APS6404L profile 驱动

并在 `components/drivers/CMakeLists.txt` 中加入对应源文件。

不要直接把 PSRAM 的复位、读、写和 memory-mapped 时序加入 `gd32h77x_nor.c`。
`gd32h77x_nor.c` 当前绑定 `GD25Q64ESIG` 的 JEDEC ID、状态轮询、页编程和 4 KiB 擦除
接口，而 PSRAM 没有这些 NOR 操作，命令、时钟限制和刷新约束也不同。把两套协议混在
同一个文件里，容易导致设备类型误判、错误擦写和 OSPI0 状态互相污染。

推荐分两层实现：

- 如果确有共用需求，先抽取公共 OSPI/OSPIM helper，用于时钟、引脚、互斥、有界轮询、
  abort、错误码转换和 memory-mapped 状态管理。
- 协议层保持独立：NOR 继续放在 `gd32h77x_nor.c`，PSRAM 放在
  `gd32h77x_psram.c`，由各自文件管理设备专用命令和操作流程。

在没有抽取公共 helper 前，可以复用 `gd32h77x_nor.c` 中的以下工程模式，但应复制到
PSRAM 驱动或后续抽成公共模块，不能把 PSRAM 分支加入 NOR 设备协议实现：

- OSPI 有界轮询
- abort 和状态清理
- 互斥访问
- OSPIM/GPIO 时钟初始化
- OSPI 错误码转换

### 4.1 建议初始化参数

| 项目 | 建议值 |
|---|---|
| 控制器 | OSPI0 |
| I/O manager | OSPIM Port0 |
| 设备大小 | `OSPI_MESZ_8_MBS` |
| 存储类型 | `OSPI_STANDARD_MODE` |
| 初始化时钟 | 20 至 25 MHz |
| 目标时钟 | ≤84 MHz，建议约 75 MHz |
| 时钟源 | 优先 `RCU_OSPI_AHB`，按实际时钟动态分频 |
| DQS | 关闭 |
| Dual-quad | 关闭 |
| 数据采样 | 半周期采样，再按实板调节 |
| CS 高电平时间 | 3 个 SCK 周期 |
| Memory-mapped timeout | 建议 256 个 SCK 周期 |
| D-Cache | 第一阶段保持关闭 |

分频必须按实际 `rcu_clock_freq_get()` 结果计算，保证 SCK 不超过 84 MHz，不依赖固定的 AHB 频率假设。

### 4.2 OSPIM 配置

必须配置：

- Port0 SCK 使能，源选择 OSPI0_SCK。
- Port0 CSN 使能，源选择 OSPI0_CSN。
- Port0 IO[3:0] 使能，源选择 OSPI0 IO low。
- Port0 IO[7:4] 关闭。
- Port0 DQS 关闭。
- 所有 OSPIM 引脚使用表中对应 AF，并配置为推挽、无上下拉、高速模式。

### 4.3 PSRAM 命令

建议使用 SPI mode 下的 0xEB/0x38，不进入 QPI 模式。

读命令：

- 命令：`0xEB`
- 命令阶段：1 线
- 地址阶段：4 线，24 bit
- dummy：6 个 SCK
- 数据阶段：4 线，SDR
- 指令发送策略：每次命令都发送（`OSPI_SIOO_INST_EVERY_CMD`）

写命令：

- 命令：`0x38`
- 命令阶段：1 线
- 地址阶段：4 线，24 bit
- dummy：0
- 数据阶段：4 线，SDR
- 指令发送策略：每次命令都发送

不使用 `0x0B` 作为主要读命令。`0x0B` 在 QPI 模式下最高只有 66 MHz，低于 0xEB。

### 4.4 初始化流程

建议按以下顺序实现：

1. 打开 GPIOA/GPIOB/GPIOC/GPIOE、OSPIM、OSPI0 时钟。
2. 设置 `PA1SWON=1`、`PC2SWON=0`。
3. 配置 PE11、PB2、PA2、PB0、PA3、PA1 的复用功能。
4. 配置 OSPIM Port0。
5. 确保 PSRAM VDD 已稳定，并额外等待至少 1 ms。
6. 在间接模式下发送 Reset Enable `0x66`。
7. 紧接着发送 Reset `0x99`。
8. 等待至少 50 ns，再执行下一条命令。
9. 通过 `0x9F` 读取 ID/KGD，PASS 值为 `0x5D`。
10. 低速执行 8 MiB 范围和地址边界读写测试。
11. 执行单字节、半字、字和非对齐访问能力测试。
12. 配置 OSPI0 read/write memory-mapped 命令参数。
13. 打开 memory-mapped timeout，并进入 `OSPI_MEMORY_MAPPED`。
14. 创建 PSRAM 堆，标记 PSRAM 可用。

PSRAM 初始化后如需从 memory-mapped 切换回间接模式，必须先完成同步、abort 所有未完成事务并清理 TCC/TERRC，不能在业务并发访问时切换。

### 4.5 CE# 和刷新约束

memory-mapped 模式下必须打开 timeout：

- 建议 timeout 为 256 个 SCK 周期。
- 75 MHz 时约 3.4 us。
- 84 MHz 时约 3.0 us。
- 必须小于标准器件 `tCEM=8 us`。

CS 高电平时间配置为 3 个 SCK 周期，以满足 `tCPH >= 18 ns`。不允许通过长时间保持 CE# 低来提升连续读吞吐。

## 5. PSRAM 地址与堆规划

OSPI0 固定映射窗口：

- OSPI0：`0x9000_0000 - 0x9FFF_FFFF`
- 本板有效 PSRAM：`0x9000_0000 - 0x907F_FFFF`

建议第一版地址划分：

| 区域 | 地址范围 | 大小 | 用途 |
|---|---:|---:|---|
| PSRAM 保留区 | `0x9000_0000 - 0x903F_FFFF` | 4 MiB | 后续图片、音频、字体等固定预加载资源 |
| PSRAM 动态堆 | `0x9040_0000 - 0x907F_FFFF` | 4 MiB | `tkl_system_psram_malloc/free` |

底层 PSRAM bring-up 测试可覆盖完整 8 MiB，但正式对业务开放的 malloc 堆先限制在 4 MiB，避免提前占用预加载资源区。

如果后续决定当前阶段不使用预加载区，可将动态堆扩展为完整 8 MiB，但必须同步修改 `PSRAM_ALLOCATION.md` 和 Kconfig/常量。

不修改链接脚本。PSRAM 只作为运行时地址窗口和堆使用。

## 6. TKL PSRAM 堆设计（仅 mapped 模式，当前不启用）

### 6.1 分配器

平台内已有 `tuya_mem_heap`，优先复用它创建独立堆。

推荐：

- 在 `tkl_memory.c` 中创建固定 `HEAP_HANDLE`。
- 初始化时调用 `tuya_mem_heap_init()` 安装线程保护回调，再调用 `tuya_mem_heap_create()`。
- 使用 FreeRTOS mutex 实现 `enter_critical/exit_critical`。
- PSRAM malloc/free 只允许任务上下文调用，不承诺 ISR 安全。
- 返回地址建议统一按 32 Byte 对齐，便于后续 D-Cache 和 DMA 验证。
- 如果底层堆只能保证 4/8 Byte 对齐，在 TKL 层增加固定大小对齐头，保存 magic、原始堆指针和请求大小。

分配器需要满足：

- `malloc(0)` 返回 `NULL`。
- `calloc` 检查 `nitems * size` 溢出。
- `realloc` 保存并复制原请求大小，不能按新大小读取旧块。
- `free` 校验 magic、地址范围和重复释放。
- `get_free_heap_size` 返回当前可用字节数，使用 `int` 时注意 4 MiB 可安全表示。

### 6.2 线程安全

PSRAM 分配和释放可能被音频、网络、图像和 UI 任务并发调用，必须加锁。

建议：

- 使用独立的 PSRAM heap mutex。
- 不在持锁期间执行耗时 OSPI 命令或打印大段日志。
- 初始化前调用 API 返回 `NULL`。
- 初始化失败后 API 保持失败状态，不反复尝试未完成的硬件初始化。

### 6.3 TKL 接口

在 `platform/GD32H77X/tuyaos/tuyaos_adapter/include/system/tkl_memory.h` 增加五个 PSRAM 接口声明，并在 `tkl_memory.c` 实现。

`src/tal_system/src/tal_system.c` 已有 `ENABLE_EXT_RAM` 分支，不需要再复制一套 TAL 接口。

## 7. Kconfig 与板级使能

### 7.1 配置职责划分

配置分为三层，不能把 PSRAM 硬件细节放进 TuyaOpen 板级可见开关：

1. **TuyaOpen 通用能力**
   - `ENABLE_EXT_RAM` 表示该板需要使用外部 RAM。
   - 该符号由板级 `BOARD_CONFIG` 选择，不作为用户可见板级开关。

2. **GD32H77X 平台 PSRAM 配置**
   - 是否启用 PSRAM 外设。
   - PSRAM 器件 profile。
   - OSPIM port。
   - CS/SCK/IO0-IO3 引脚。
   - 工作时钟、器件容量和运行时堆范围。
   - 因为不是所有 GD32H77X 板子都有 PSRAM，且不同板子的 OSPIM port 和引脚不同，
     这些参数必须属于平台配置。

3. **具体板级参数**
   - Desk Buddy 在 `platform_config/gd32h77x.config` 中填写本板 PSRAM 参数。
   - 不在 `boards/GD32H77X/GD32H779I_DESK_BUDDY/Kconfig` 新增用户可见的
     板级 PSRAM 开关。

### 7.2 通用 ENABLE_EXT_RAM

当前 GD32H77X 的 TKL Kconfig 没有定义 `ENABLE_EXT_RAM`。在：

`boards/GD32H77X/TKL_Kconfig`

增加隐藏能力符号：

```kconfig
config ENABLE_EXT_RAM
    bool
    default n
```

该符号由具体板子的 `BOARD_CONFIG` 选择，不提供用户菜单开关：

```kconfig
config BOARD_CONFIG
    bool
    default y
    select ENABLE_EXT_RAM
```

Desk Buddy 的 `BOARD_CONFIG` 直接选择 `ENABLE_EXT_RAM`，不再增加板级 PSRAM 开关。

### 7.3 GD32H77X 平台 PSRAM 配置

在：

`platform/GD32H77X/Kconfig`

增加平台级 PSRAM 配置。建议至少包含：

```kconfig
config ENABLE_GD32H77X_PSRAM
    bool "Enable external PSRAM on OSPI0"
    default n
    help
      Enable the GD32H77X OSPI0 PSRAM driver and TKL PSRAM heap.

config GD32H77X_PSRAM_PROFILE
    string "PSRAM device profile"
    depends on ENABLE_GD32H77X_PSRAM
    default "APS6404L-3SQR"

choice
    prompt "PSRAM OSPIM port"
    depends on ENABLE_GD32H77X_PSRAM
    default GD32H77X_PSRAM_OSPIM_PORT0

    config GD32H77X_PSRAM_OSPIM_PORT0
        bool "OSPIM Port 0"

    config GD32H77X_PSRAM_OSPIM_PORT1
        bool "OSPIM Port 1"
endchoice

config GD32H77X_PSRAM_CS_PIN
    string "PSRAM CS pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_SCK_PIN
    string "PSRAM SCK pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_IO0_PIN
    string "PSRAM IO0 pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_IO1_PIN
    string "PSRAM IO1 pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_IO2_PIN
    string "PSRAM IO2 pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_IO3_PIN
    string "PSRAM IO3 pin"
    depends on ENABLE_GD32H77X_PSRAM

config GD32H77X_PSRAM_CLOCK_HZ
    int "PSRAM clock in Hz"
    depends on ENABLE_GD32H77X_PSRAM
    range 1000000 84000000
    default 75000000

config GD32H77X_PSRAM_SIZE_MB
    int "PSRAM capacity in MiB"
    depends on ENABLE_GD32H77X_PSRAM
    range 1 8
    default 8

config GD32H77X_PSRAM_HEAP_OFFSET_KB
    int "PSRAM runtime heap offset in KiB"
    depends on ENABLE_GD32H77X_PSRAM
    range 0 8192
    default 4096

config GD32H77X_PSRAM_HEAP_SIZE_KB
    int "PSRAM runtime heap size in KiB"
    depends on ENABLE_GD32H77X_PSRAM
    range 1 8192
    default 4096
```

说明：

- PSRAM 控制器固定为 OSPI0，因为 GD32H77X 只有 OSPI0 支持 PSRAM/HyperRAM。
- 可通过 OSPIM Port0/Port1 和六根引脚适配不同板级布局。
- `platform_config.py` 应根据 pin 字符串和信号类型解析合法 AF，并生成
  `GD32H77X_PSRAM_*_AF` 宏。
- 生成阶段必须拒绝重复引脚、非法 pin、非法 AF 和超出芯片封装的 pin。
- 如果 pin 为 `PC2_C` 或 `PA1`，生成配置必须同时记录所需模拟开关状态，
  初始化代码据此设置 `PC2SWON`/`PA1SWON`。
- 平台配置校验应保证 `HEAP_OFFSET_KB + HEAP_SIZE_KB <= SIZE_MB * 1024`。

### 7.4 Desk Buddy 板级参数

Desk Buddy 在：

`boards/GD32H77X/GD32H779I_DESK_BUDDY/platform_config/gd32h77x.config`

填写：

```text
# APS6404L-3SQR-SN on OSPI0 / OSPIM Port0.
CONFIG_ENABLE_GD32H77X_PSRAM=y
CONFIG_GD32H77X_PSRAM_PROFILE="APS6404L-3SQR"
CONFIG_GD32H77X_PSRAM_OSPIM_PORT0=y
CONFIG_GD32H77X_PSRAM_CS_PIN="PE11"
CONFIG_GD32H77X_PSRAM_SCK_PIN="PB2"
CONFIG_GD32H77X_PSRAM_IO0_PIN="PA2"
CONFIG_GD32H77X_PSRAM_IO1_PIN="PB0"
CONFIG_GD32H77X_PSRAM_IO2_PIN="PA3"
CONFIG_GD32H77X_PSRAM_IO3_PIN="PA1"
CONFIG_GD32H77X_PSRAM_CLOCK_HZ=75000000
CONFIG_GD32H77X_PSRAM_SIZE_MB=8
CONFIG_GD32H77X_PSRAM_HEAP_OFFSET_KB=4096
CONFIG_GD32H77X_PSRAM_HEAP_SIZE_KB=4096
```

其他 GD32H77X 板子如果不接 PSRAM：

- 不选择 `ENABLE_EXT_RAM`。
- 保持 `ENABLE_GD32H77X_PSRAM=n`。
- 平台不实例化 PSRAM 驱动和 TKL PSRAM 堆。

### 7.5 配置一致性检查

必须增加编译期检查，避免“板子选择了外部 RAM，但平台没有 PSRAM 配置”：

```c
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#if !defined(ENABLE_GD32H77X_PSRAM) || (ENABLE_GD32H77X_PSRAM == 0)
#error "ENABLE_EXT_RAM requires ENABLE_GD32H77X_PSRAM on GD32H77X"
#endif
#endif
```

同时在初始化阶段检查：

- PSRAM 驱动已启用
- profile 已识别
- OSPIM port 合法
- 六根引脚与 AF 已生成
- 堆范围在 PSRAM 容量内
- memory-mapped 初始化成功

任一检查失败时，`board_register_hardware()` 返回错误，不允许继续启动依赖 PSRAM 的业务。

### 7.6 初始化位置

`board_register_hardware()` 中按以下顺序执行：

```text
board_register_hardware()
  -> PSRAM low-level init
  -> PSRAM heap create
  -> display register
  -> camera register
  -> touch/motion/encoder register
```

PSRAM 初始化失败时直接返回错误，不继续注册依赖外部 RAM 的业务。

## 8. 风险门禁

### 8.1 Memory-mapped sub-word 写能力

这是开放普通 malloc 前的最高优先级门禁。

GD32H77x 用户手册存在两处需要实板确认的描述：

- 一处描述 memory-mapped 数据阶段可按 8/16/32 bit AXI 访问。
- 另一处明确 memory-mapped 写只保证 word、double-word 或 burst。

普通 C 代码经常产生 `STRB/STRH`：

```c
p[0] = 1;
p[1] = 2;
*(uint16_t *)(p + 4) = 0x1234;
```

必须实板验证：

1. 在 4 字节对齐地址写入 `0x5A5A5A5A`。
2. 对其中一个 byte 写 `0xA5`。
3. 读回整个 word，确认只有目标 byte 改变。
4. 对 half-word 重复测试。
5. 测试连续地址、非对齐地址和跨 32 Byte 边界地址。
6. 记录是否触发 AXI error、HardFault、总线阻塞或相邻数据损坏。

如果 byte/half-word 写不可用：

- 不能把该区域作为普通可写 heap 对业务开放。
- 备选方案是启用 D-Cache，把 PSRAM 配置为 write-back cacheable，让 byte 写先命中 cache，回写时形成 burst。
- 该备选方案需要额外完成 MPU 配置、DMA coherency 和全平台回归，不能与本适配混为一个小改动。

### 8.2 D-Cache

当前 `main.c` 只启用 I-Cache，D-Cache 保持关闭。

第一阶段：

- 依赖 D-Cache 关闭条件完成 PSRAM bring-up。
- 不添加 PSRAM cache clean/invalidate。
- 不把 PSRAM 用作 DMA 缓冲，除非完成额外验证。

后续如果启用 D-Cache：

- 使用 MPU 把 PSRAM 配置为 Normal、non-shareable、write-back/write-allocate。
- 所有使用 PSRAM 的 DMA 路径必须 clean/invalidate。
- LCD、camera、audio、AI、网络路径必须逐项回归。

### 8.3 DMA 可访问性

不能仅因为 PSRAM 有地址就认定 DMA 可访问。

第一阶段约束：

- PSRAM 默认只保证 CPU 读写。
- LVGL draw buffer 继续放 AXISRAM。
- 摄像头 raw frame 继续使用板级内部 SRAM 池。
- QSPI/DVP/SPI/UART DMA staging 继续放 AXISRAM 或 SRAM0/1。
- 只有实板验证 DMA 到 `0x9000_0000` 地址空间后才能放开对应用途。

### 8.4 `ENABLE_EXT_RAM` 的全局影响

打开 `ENABLE_EXT_RAM` 后，公共代码中的：

- `Malloc/Free/Realloc` 宏
- LVGL v9 `lv_malloc_core`
- 部分显示、摄像头、音频和 AI 大缓冲

会跟随切换到 PSRAM。

Desk Buddy 首版要求：

- 保持 LVGL partial flush。
- 设置 `CONFIG_ENABLE_LVGL_DRAW_BUF_PSRAM` 为 `n`，draw buffer 留在 AXISRAM。
- 如果 LVGL 对象/样式热数据不应进入 PSRAM，需要将 LVGL heap 与 `ENABLE_EXT_RAM` 解耦，并增加独立配置项。
- 摄像头帧缓冲继续使用内部 SRAM 池，直到 DMA 验证通过。

### 8.5 时钟和刷新

- 第一版不使用 32 Byte wrap 模式，不追求 109/133 MHz。
- 线性 burst 最大 84 MHz，建议目标 75 MHz。
- 如果实板在 75 MHz 出现数据错误，先降到 50 或 25 MHz。
- 每次提速都要重新执行全范围 pattern、边界和长时间读写测试。
- 必须确认 CE# 低电平时间不超过 8 us。

## 9. 实施清单

| 项目 | 文件/位置 | 说明 |
|---|---|---|
| OSPI 控制器通用层 | `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_ospi.c/.h` | 新增，负责 OSPIM、GPIO、时钟、事务、memory-mapped 和 abort |
| PSRAM 设备抽象层 | `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_psram.c/.h` | 新增，定义设备、能力、状态和 profile ops |
| APS6404L profile | `platform/GD32H77X/gd32h77x_os/components/drivers/gd32h77x_psram_aps6404l.c/.h` | 新增，负责器件命令、dummy、ID、复位和时序 |
| 驱动构建 | `platform/GD32H77X/gd32h77x_os/components/drivers/CMakeLists.txt` | 加入 OSPI 控制器、PSRAM 抽象和 profile 源文件 |
| TKL 接口声明 | `platform/GD32H77X/tuyaos/tuyaos_adapter/include/system/tkl_memory.h` | 增加 5 个 PSRAM 接口 |
| TKL 堆实现 | `platform/GD32H77X/tuyaos/tuyaos_adapter/src/system/tkl_memory.c` | 独立堆、锁、对齐和诊断 |
| 通用 Kconfig | `boards/GD32H77X/TKL_Kconfig` | 增加隐藏 `ENABLE_EXT_RAM` 能力符号 |
| 板级 Kconfig | `boards/GD32H77X/GD32H779I_DESK_BUDDY/Kconfig` | `BOARD_CONFIG` 直接 `select ENABLE_EXT_RAM`，不增加板级 PSRAM 开关 |
| 平台 Kconfig | `platform/GD32H77X/Kconfig` | 增加 PSRAM enable、OSPIM port、引脚、时钟、容量和堆范围配置 |
| 平台配置生成 | `platform/GD32H77X/platform_config.py` | 解析并校验 OSPI/OSPIM 引脚与 AF，生成配置宏 |
| 板级参数 | `boards/GD32H77X/GD32H779I_DESK_BUDDY/platform_config/gd32h77x.config` | 填写 Desk Buddy 的 OSPI0/OSPIM Port0 参数 |
| 板级初始化 | `boards/GD32H77X/GD32H779I_DESK_BUDDY/board_com_api.c` | 在显示/摄像头前初始化 PSRAM |
| 分配规划 | `PSRAM_ALLOCATION.md` | 与本文地址规划保持一致 |
| 引脚记录 | `PIN_BRINGUP.md` | 记录 PSRAM 引脚和 Excel 勘误 |

## 10. 测试计划

### 10.1 主机侧

- Kconfig 编译测试：未选择 `ENABLE_EXT_RAM` 的板子不引用 PSRAM 实现。
- 配置一致性测试：`ENABLE_EXT_RAM` 打开但 `ENABLE_GD32H77X_PSRAM` 关闭时构建失败。
- 驱动命令序列和错误路径的 host fake 测试。
- TKL heap 在内部 SRAM 替身区域上的 malloc/free/calloc/realloc 测试。
- 对齐、溢出、重复释放、非法指针和并发压力测试。

### 10.2 实板低速 bring-up

在 20 至 25 MHz 下验证：

- Reset 命令序列成功。
- `0x9F` 返回 KGD PASS `0x5D`。
- `0x00000000`、`0x00100000`、`0x007FFFFF` 等地址读写正确。
- 1 KiB 页边界、4 KiB 边界和 8 MiB 末尾地址读写正确。
- 全 8 MiB 递增、递减和伪随机 pattern 测试。
- byte/half-word/word/非对齐写测试。
- 断电重启后重复测试。

### 10.3 目标时钟测试

- 50 MHz 稳定后提升到 75 MHz。
- 每个速率执行完整 pattern 和边界测试。
- 记录错误率、HardFault、AXI error 和 timeout。
- 检查 CE# 低电平时间不超过 8 us。
- 如果数据不稳定，调整采样延迟和 RX shift。

### 10.4 TKL 堆测试

- 32 Byte 对齐检查。
- 随机大小分配/释放。
- 碎片合并和长时间压力。
- `realloc` 扩大、缩小和失败保持原指针。
- `calloc` 清零和乘法溢出。
- 并发任务同时 malloc/free。
- 分配失败后系统行为可预期。

### 10.5 系统回归

在 PSRAM 开启后验证：

- LCD QSPI 显示正常，SPI3 未受 OSPI0 影响。
- 触摸、编码器、IMU 正常。
- 摄像头采集、预览和帧缓冲正常。
- TY1/AT、WiFi/BLE 正常。
- 音频播放、AI 输入输出、HTTP/TLS 正常。
- LVGL partial flush 正常。
- 长时间运行时 PSRAM free watermark 和失败日志正常。

## 11. 验收标准

间接模式验收已完成。以下条件用于后续把 Desk Buddy 的 memory-mapped 指针堆 `ENABLE_EXT_RAM` 作为正式板级能力合入：

1. PSRAM ID/KGD 读取通过。
2. 8 MiB 全范围读写和边界测试通过。
3. byte/half-word 写行为明确且符合 TKL malloc 语义。
4. 目标时钟下连续压力测试无错误。
5. CE# 低电平时间满足 `tCEM`。
6. `tkl_system_psram_*` 五个接口工作正常。
7. PSRAM 堆无泄漏、无越界、无重复释放。
8. DMA 可访问性结论明确；未验证的设备不得使用 PSRAM buffer。
9. LCD、摄像头、音频、网络和 LVGL 回归通过。
10. 未配置 PSRAM 的其他 GD32H77X 板子不引用 PSRAM 驱动，构建和运行行为与当前基线一致。

## 12. 实施状态与后续顺序

已完成：

1. OSPI0 间接模式底层驱动和 `read/write/fill` API。
2. ID/KGD、非对齐、页边界、容量末尾和 8 MiB 全容量测试。
3. 4 MiB 偏移量动态池及其随机/并发压力测试。
4. 资源缓存层、真实 JPEG 流式装载和吞吐基线。
5. 板级间接模式配置和启动初始化。

后续仅在需要普通指针语义时执行：

1. 解决 memory-mapped byte/half-word 写、缓存和容量末尾问题。
2. 配置并验证 memory-mapped 模式、D-Cache 和 DMA coherency。
3. 实现 PSRAM 独立堆和五个 TKL 接口。
4. 在 Desk Buddy 中选择 `ENABLE_EXT_RAM`，完成系统回归和长时间压力测试。
5. 更新本文状态并记录最终映射配置。
