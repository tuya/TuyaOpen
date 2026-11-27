# 原厂 PSRAM 驱动验证记录

源码流程、外设库版本差异和移植边界见 [原厂驱动梳理](VENDOR_DRIVER_NOTES.md)。

## 当前结论

2026-11-23：原包附带 HEX 在 Desk Buddy 上完成首次测试及三次复位重跑，四次原始完整测试均通过。现有 TuyaOpen PSRAM 实现暂不替换；普通堆所需访问语义仍需补充验证。

## 原始基线

- 工程：`GD32H779/Template/Keil_project/GD32H779.uvprojx`。
- 驱动：`GD32H779/Utilities/bsp_drive/aps6404l.c`、`aps6404l.h`。
- 测试入口：`GD32H779/Template/main.c`。
- 附带产物：`GD32H779/Template/Keil_project/Objects/Project.hex`、`Project.axf`。
- 工程目标为 GD32H779IW，宏为 `GD32H779XW`。启动源码选择外部 25 MHz 晶振的 600 MHz 配置。烧录前核对实板晶振及启动配置。
- 附带 HEX/AXF 的实际加载区间为 `[0x08010000, 0x080139A0)`，二者加载内容逐字节一致。启动源码 `VECT_TAB_OFFSET=0x10000`。实际固件从 `0x08010000` 运行，不能按芯片 Flash 基址 `0x08000000` 搬移烧录；独立复位能否启动还取决于板上的引导配置。
- 引脚：PE11/AF11、PB2/AF9、PA2/AF6、PB0/AF4、PA3/AF6、PA1/AF9，与当前 Desk Buddy 引脚记录一致。
- 默认运行 SPI 指令、四线地址/数据模式，不是 QPI 指令模式。

先解压原包独立运行，保留原始文件。任何为板子或工具链所作的修改均另存差异，不混入原始基线。

## 第一阶段：原厂工程原样上板

测试通过调试器观察全局变量，不依赖串口输出。映射未开启时不要让 Watch/Memory 窗口读取 `0x90000000`，原厂注释指出这种读取可能挂住总线。

运行到最终空循环后读取：

| 变量 | 含义 / 预期 |
|---|---|
| `psram_result` | 最终 `1` 为通过；`0` 可能仍在运行或已经卡住 |
| `psram_sck_hz` | 实际总线频率，记录实值，不以 84 MHz 宏当作实测值 |
| `psram_fail_offset` | 全容量测试的首个错误字节偏移 |
| `bw_indirect_wr/rd` | 间接写/读带宽，单位 KiB/s |
| `bw_mmap_wr/rd` | 映射写/读带宽，单位 KiB/s |

错误码：`0x10` 初始化，`0x20` 间接访问，`0x30` 映射读，`0x40` 映射写后间接读，`0x50` 映射读写，`0x60` 全容量测试。

现有测试顺序：初始化并检查 MFID=`0x0D`、KGD=`0x5D` → 多地址及跨 1 KiB 页间接测试 → 4 KiB 映射交叉读写 → 带宽测试 → 全 8 MiB 地址相关 pattern 写完再读、取反后重复。

注意：中途也会出现 `psram_result=1`，随后主函数会重置为 `0` 并执行带宽和全容量测试；必须在最终空循环处判断最终结果。卡住时记录 PC、调用栈、CFSR/HFSR/BFAR 和 OSPI 寄存器，不能只看结果变量。

## 第二阶段：补充验证（独立修改测试入口）

原样基线通过后再补充，保持 `aps6404l.c/.h` 不变以定位问题：

1. 记录完整 ID、实际 SCK、MPU 属性，重复冷启动及 MCU 单独复位测试。
2. 覆盖映射区 byte/half-word/word 写，验证邻接字节未被破坏；检查 1 KiB 边界和容量末尾。
3. 单独验证非对齐访问及实际 `memcpy/memset` 访问方式，捕获 Fault。原工程 MPU 把 PSRAM 配成 Device 内存，不能据此假定普通 C 堆的所有访问都受支持。
4. 增加映射区全容量校验、空闲后数据保持、反复进入/退出映射及长时间压力。原包全容量测试使用间接模式，映射模式正确性只覆盖 4 KiB。
5. 用逻辑分析仪核对 CE# 连续低电平和最小高电平时间。原厂使用 refresh 和 32-cycle idle timeout，配置存在不等于实测满足器件限制。
6. QPI、DMA、RTOS 并发及通用堆接口后续分别验证。原入口默认 SPI 模式，不能用其结果代替这些能力的验证。

原驱动部分等待没有超时；测试卡住属于待分析结果。测试入口没有检查所有映射切换返回值，带宽阶段也没有校验数据，带宽数值本身不能证明读写正确。

## 验收记录（待实测）

| 项目 | 结果 |
|---|---|
| 实板型号/PSRAM 丝印/供电 | 待记录 |
| 调试器/原包或重编译产物/改动差异 | 待记录 |
| 原厂最终结果/实际 SCK/完整 ID | 待记录 |
| 全容量及映射读写 | 待记录 |
| 小宽度/非对齐/内存函数 | 待记录 |
| 冷启动/热复位/压力/CE# 时序 | 待记录 |

完成基线和所需访问语义验证后，再以原厂驱动替换现有实现并接入 TKL；若 Device 属性限制普通堆访问，先解决访问属性和控制器兼容性，再开放 PSRAM malloc。

## 主机连接检查（2026-11-23）

主机 USB 已枚举到 `28e9:0189` GD32 DFU Bootloader；pyOCD 未发现调试器，也未发现 USB 串口。尚未烧录或运行测试。

后续重新连接后识别到 Sipeed RV CMSIS-DAP（序列号 `202608131200`）及 `/dev/ttyACM0`。使用本地 DFP、目标 `gd32h779iw` 尝试 1 MHz halt 连接，以及 100 kHz under-reset 连接，两次均在 DebugPortSetup 阶段报 SWD No ACK。尚未读取目标内存、完成备份或写入 Flash；需检查目标侧 SWDIO/SWCLK/GND、供电和复位接线。

原包 AXF 中结果变量地址如下（仅适用于本包附带产物，重编译后重新查符号）：

| 符号 | 地址 |
|---|---|
| `psram_result` | `0x24000088` |
| `psram_sck_hz` | `0x2400008C` |
| `psram_fail_offset` | `0x24000084` |
| `bw_indirect_rd` | `0x24000020` |
| `bw_indirect_wr` | `0x24000024` |
| `bw_mmap_rd` | `0x24000028` |
| `bw_mmap_wr` | `0x2400002C` |

## 原厂原始程序实测（2026-11-23）

后续重试标准 SWD attach 成功，CPUID=`0x411FC272`；专用 DFP 的 halt 连接也随后成功。此前无应答原因未确定，不据此判定接线或芯片包故障。

- 使用原包 `Project.hex`，未修改原厂源码。烧录前已备份 `[0x08010000, 0x08020000)` 完整 64 KiB 扇区，备份位于仓库 `.cache/psram-vendor-validation/flash-08010000-before.bin`，SHA256 为 `375dd4d3db98494e97af288467383d434eb5b5383bf7577c49575bcd5cd0bd29`。
- 使用 sector erase 烧录，14,752 字节回读与 HEX 一致。
- 通过 SWD reset-and-halt 后设置 VTOR、MSP、PC 等寄存器，从原包 `0x08010000` 向量表启动。此方式不验证脱离调试器后的上电引导。
- 运行后 `psram_result=1`，PC=`0x080117E4`，反汇编确认是全容量测试返回后的最终空循环，排除了中间阶段暂时通过的情况。
- 驱动记录的实际 SCK 为 `75,000,000 Hz`（由时钟配置计算，并非逻辑分析仪实测）。CFSR/HFSR/BFAR 均为 `0`，`psram_fail_offset=0`。
- 间接读/写带宽分别为 `11928 / 14284 KiB/s`；映射读/写带宽为 `33284 / 9155 KiB/s`。带宽仅为此次原程序测量值。
- 原始结果及运行日志保存在 `.cache/psram-vendor-validation/baseline-result.json` 和 `baseline-run.log`。
- 随后三次 reset-and-halt 并从相同向量入口重跑，均在最终空循环取得 `result=1`、`SCK=75000000`、`CFSR=HFSR=0`，记录见 `.cache/psram-vendor-validation/repeat-result.json`。

结论仅覆盖原程序默认 SPI 指令模式、75 MHz 配置及其测试范围；不代表 byte/half-word/非对齐写、通用堆、QPI、DMA、长时间压力或掉电启动已验收。当前调试运行结束后 CPU 保持 halt，Flash 中保留原厂测试固件。

### 最新范围与恢复复测

按用户要求，当前只以原厂原始程序测试为验收依据，暂不继续缓存及小宽度访问扩展实验。期间在 SRAM 加载过临时访问探针，观察到读回异常及 Fault；这些属于扩展诊断，未完成原因定位，不计入原厂原始测试的通过或失败结果。

结束扩展实验后，重新软件复位并从原包入口运行三次原始程序，均得到 `psram_result=1`、PC=`0x080117E4`、SCK=`75000000`、CFSR/HFSR=`0`。日志为 `.cache/psram-vendor-validation/recovery.log`。Flash 未被扩展探针修改，当前仍为原包 HEX；尚未替换 TuyaOpen 底层，也尚未重新编译原厂源码。

### 新一轮映射写/间接读交叉验证准备

用户授权继续后，新增 [独立补测探针及运行说明](validation/README.md)：每项重新初始化，先间接写读验证初值，再单次映射写，退出映射后间接读回64字节，检查目标和相邻字节。覆盖 byte、half-word、word、页边界和容量末尾；另可开启映射读回对照。

探针以 GCC 编译通过（`-Wall -Wextra` 无告警、无未解析符号），确认生成 STRB/STRH 指令；Python 脚本语法检查通过。启动实测时报 `Probe 202608131200 not found`，随后两次 pyOCD 扫描均无可用调试器，USB 列表也未发现 Slogic。此轮尚未执行 CPU 探针、没有新的实测结论，等待调试器重新接入/透传。

调试器重新接入后已完成交叉补测，见 [详细结果及最小复现数据](validation/RESULTS.md)。原厂配置下部分映射 byte/half-word 写出现实际错位，映射与间接读回一致；byte 写后直接退出映射还出现 abort 等待。原厂原始 word 测试通过的结论仍然有效，但不满足通用 PSRAM 堆的验收条件。

当前优先级：按用户确认的长期保持映射方案，byte 写后退出映射的 abort 等待仅作为低优先级待确认项保留，不阻塞当前适配主线；优先解决 byte/half-word 映射写错位。后续需要退出映射的功能再单独验收收尾流程。

最新接口核对：直接调用原包 `aps6404_read/write()` 进行1/2/4字节读写，共21项（含页边界及容量末尾）全部通过。测得1字节写约5.1 us、读约5.3 us。此前映射直访错位与该函数接口应分别记录，不能泛称“原厂驱动不支持字节读写”。缓存/映射路径实验的进展和未通过项见 [RESULTS.md](validation/RESULTS.md)。

### 2026-11-25 DFU 映射-only 复测

使用 `examples/system/psram_mapped_minimal`，直接提取 `GD32H779.zip` 内的 V0.3.0
CMSIS、OSPI/OSPIM 和 `aps6404l.c/.h`，不经 TuyaOpen 或 FreeRTOS。CNVM 128 KiB
和 ECNVM 64 KiB 均通过 GD32 ROM DFU 写入，并完成整段回读校验后以 DfuSe leave
启动；测试代码只执行 32 位 memory-mapped 读写。

| 范围 | 结果 |
|---|---|
| 偏移 `0x1000` 起 4 KiB，1024 个 word | PASS；`test_phase=5`、`round=1`、`test_word=0x3FF`，期望值等于实际值，CFSR/HFSR 均为 0 |
| 偏移 `0` 起整个 8 MiB，2,097,152 个 word | 最后一个 word 异常；`test_word=0x1FFFFF`、`BFAR=0x907FFFFC`、CFSR=`0x00008200`、HFSR=`0x40000000` |

结论：原厂驱动的 32 位映射读写在其 4 KiB 基线窗口内可用；整片扫描除最后一个
word 外未触发异常。驱动接入时应把映射堆末尾至少 32 字节作为保护区，不允许对
`0x907FFFFC` 发起访问。该结果不扩展为 byte/half-word 映射写或通用 C 堆的通过结论。

### TuyaOpen `your_desk_buddy` 映射堆运行验证

应用通过 `CONFIG_ENABLE_PSRAM=y` 和板级
`CONFIG_GD32H77X_PSRAM_ACCESS_MAPPED=y` 启用 PSRAM，并在
`board_register_hardware()` 之后调用 `tkl_system_psram_init()`。DFU 烧入后读取
运行时状态：

| 项目 | 实测值 |
|---|---|
| `s_psram_initialized` | `1` |
| 厂商驱动实际 SCK | `75,000,000 Hz` |
| OSPI `CTL` | `0x30000309`，`FMOD=MEMORY_MAPPED`、timeout counter 开启 |
| OSPI `STAT` | `0` |
| PSRAM 堆 base / size | `0x90400000` / `0x3FFFE0` |
| 初始空闲块 | offset=`0x400000`、size=`0x3FFFE0`、未使用 |
| CFSR / HFSR / BFAR | `0 / 0 / 0` |

首次接入时，`aps6404_memory_map_disable()` 会在原厂 abort 等待中死循环：
映射写完成后 OSPI 已 `BUSY=0`，但状态为 `TMOUT=1`，原实现只等待 `TC=1`。
平台修复同时处理 `TC/TMOUT/TERR/BUSY=0`，并在 abort 前关闭 timeout counter。
修复后初始化、映射交叉校验和 4 MiB 运行时堆创建均通过。

当前运行时块分配器仍限定为经验证的映射访问边界：消费者只应按 32 位访问；
堆末尾 32 字节保留，不作为通用 byte/half-word C 堆开放。

原厂进一步反馈（用户转述）：映射写按32位使用，连续字节写频率高时不稳定，映射读支持字节读取。后续按此边界设计与验收；间接小长度函数通过不等于可直接开放通用PSRAM堆，缓存方案也仍需完成边界与压力验证。

按用户选择先完成候选配置整理：已固化 [cached-linear-v1](validation/CANDIDATE_V1.md)，提供自动编译、版本/哈希记录及六项基本复现入口。六项数据检查与寄存器配置检查均通过；TMOUT仍由探针显式清除，尚未摆脱插桩，不是可接入通用堆的生产配置。本轮未改TuyaOpen驱动或Flash。

继续验证自动TMOUT后的最新状态见 [v2记录](validation/CANDIDATE_V2.md)：普通短用例已验证实际IRQ清除事件及数据正确，不清除对照读回失败；128KiB连续访问出现总线WAIT且软件恢复未成功，需要实板断电恢复，候选整体仍未通过。已准备更小窗口分段定位入口，仅编译未上板。原厂Flash和TuyaOpen驱动仍未改动。

后续恢复设备后，1KiB分段测试已执行，在连续byte写阶段触发IMPRECISERR，尚未进入校验或最终回写。已编译ACTLR.DISRAMODE对照；该对照启动时软件复位被WAIT阻塞，尚未执行，需要再次断电恢复。见 [详细记录](validation/DYNAMIC_ALLOCATE.md)。
