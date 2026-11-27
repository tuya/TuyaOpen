# 原厂 APS6404L 驱动梳理

本文分析 `GD32H779.zip` 原始源码、附带 AXF 的关键反汇编及仓库当前外设库，未修改原厂驱动或 TuyaOpen 实现。实板结果见 [VENDOR_VALIDATION.md](VENDOR_VALIDATION.md)。

## 1. 要一起理解的文件

以下路径均相对 ZIP 内的 `GD32H779/`：

| 文件 | 职责 |
|---|---|
| `Utilities/bsp_drive/aps6404l.h` | 引脚、器件参数、命令、设备结构和 API |
| `Utilities/bsp_drive/aps6404l.c` | 器件初始化、间接读写、映射配置及小范围自检 |
| `Firmware/GD32H77x_standard_peripheral/Source/gd32h77x_ospi.c` | 真正配置 OSPI 寄存器、轮询 FIFO、触发传输 |
| 同目录 `gd32h77x_ospim.c` | OSPI 到外部引脚端口的路由 |
| `Template/main.c` | MPU、Cache、测试与结果变量 |
| `Template/systick.c`、`gd32h77x_it.c` | 1 ms Tick 及初始化延时依赖 |
| `Firmware/CMSIS/GD/GD32H77x/Source/system_gd32h77x.c` | 系统时钟、向量表偏移 |

原包根 README 描述的是另一个 GD32W515/LVGL 工程，不能用来解释本 PSRAM 测试。以以上实际参与构建的代码为准。附带构建日志显示 Arm Compiler 6.22。

## 2. 初始化的实际顺序

```text
main
  mpu_config → cache_enable → systick_config
  aps6404_init(OSPI0, CK_AHB, SPI)
    清空 dev；初始 mode=SPI
    OSPI deinit；GPIO/OSPIM Port0 路由
    按 ID 最高 33 MHz 配置 OSPI，不开启 refresh
    等待 1 ms，满足上电等待
    四线 66/99 → 单线 66/99 → 等待 1 ms
    9F + 24-bit 地址0 + 单线读取8字节
    检查 MFID=0D、KGD=5D
    如调用者要求 QPI，发送单线35，更新 mode
    按线性读写最高84 MHz重新配置，开启 refresh
    检查间接传输的时序预算是否足够
```

`aps6404_init()` 不执行 `aps6404_selftest()`，也不自动进入 memory-mapped。复位同时发送四线和单线序列，是为兼容 MCU 复位而 PSRAM 仍处于 QPI 的情况。

引脚固定为 PE11/AF11（CS）、PB2/AF9（CLK）、PA2/AF6（IO0）、PB0/AF4（IO1）、PA3/AF6（IO2）、PA1/AF9（IO3）。仅 CS 上拉，其余不设置上下拉。驱动支持 OSPI0/1，但两者都路由到 OSPIM Port0。

驱动本身没有显式配置 PA1 模拟开关，也没有选择 OSPI 内核时钟源；`ospi_clk_hz` 是调用者提供的值，原入口传入 CK_AHB，并依赖默认 OSPI 时钟源。不能脱离原启动环境后继续假定这些条件成立。

## 3. SPI 模式并不等于数据只用一根线

| 操作 | 指令 | 指令线数 | 地址 | 数据线数 | dummy |
|---|---|---|---|---|---|
| 读 ID | 9F | 1 | 24 bit，1线 | 1 | 0 |
| 默认模式读数据 | EB | 1 | 24 bit，4线 | 4 | 6 |
| 默认模式写数据 | 38 | 1 | 24 bit，4线 | 4 | 0 |
| QPI 读数据 | EB | 4 | 24 bit，4线 | 4 | 6 |
| QPI 写数据 | 38 | 4 | 24 bit，4线 | 4 | 0 |

默认主函数使用 SPI 模式，即数据读写为 1-4-4。QPI 才是 4-4-4。所有事务均为 SDR、8-bit 指令、每次发送指令，DQS 关闭。没有发送 C0 切换 wrap，沿用器件默认 Linear Burst。

所附 APS6404L 数据手册的命令表确认了上述读写格式、ID 33 MHz 限制、EB 六个 dummy，以及默认线性模式。原程序通过并不代表 QPI 路径已经测试。

## 4. 时钟与片选约束

分频使用向上取整：`div=ceil(ospi_clk_hz/max_hz)`，实际 SCK 为 `ospi_clk_hz/div`，寄存器写 `div-1`。原工程 CK_AHB 为 300 MHz 时，ID 阶段为 30 MHz，运行阶段为 75 MHz；实板结果变量记录 75 MHz。

固定配置：8 MiB、FIFO threshold=4、半周期采样偏移、CS 高电平3周期、Standard memory type、不添加 quarter-cycle hold。

刷新预算按 SN 器件的 8 us tCEM 计算：

```text
tcem_cycles = floor(SCK × 8000 ns)
budget      = tcem_cycles / 2
overhead    = 指令周期（SPI 8 / QPI 2）+ 地址6周期 + dummy6周期
chunk_limit = min(1024, floor((budget - overhead) / 2))
```

每字节四线 SDR 数据占2周期；对写也使用带读 dummy 的预算，偏保守。75 MHz、SPI 模式得到 `tcem_cycles=600`、`budget=300`、`chunk_limit=140` 字节。refresh 寄存器为300，外设库注释描述其片选释放周期为 `refrate+1` 个时钟，名义约4.01 us，仍需实际波形确认。

实际每笔长度为 `min(chunk_limit, 距1KiB页边界的剩余长度, 用户剩余长度)`。因此从 `0x3F0` 读写64字节会被拆分，驱动避免单笔跨页；器件默认线性模式本身允许在满足 tCEM 时跨页。

`APS6404_MIN_CHUNK=8` 用于检查时钟预算是否过小，**不代表 read/write 拒绝小于8字节**；调用长度及跨页尾部仍可能只有1字节。分块预算也不能约束任务抢占造成的 FIFO 长时间停顿。

## 5. 间接读写如何完成

`aps6404_read/write` 负责检查范围、拆分事务、设置命令，再调用外设库。HAL `ospi_config()` 先恢复 indirect write 功能模式，COMMON_CFG 写 DTLEN，配置 TCFG/TIMCFG/INS/ADDR。

- 写：`ospi_transmit()` 每次向 DATA 寄存器写一个字节，等待 FT；最后等待 TC 并清 TC。
- 读：`ospi_receive()` 保存当前 ADDR/INS，切换 indirect read；有地址时重写 ADDR，无地址时重写 INS，触发接收；逐字节读取 DATA，最后清 TC。

这里的“逐字节写 DATA FIFO”属于间接模式，不等于 CPU 对 `0x90000000` 做 STRB 映射写。两条路径不能互相证明。

这套库的轮询没有超时，返回 SUCCESS 主要表示流程执行完；数据正确性依赖另行读回检查。驱动没有维护 initialized/mapped 状态，没有互斥，也没有在读写入口自动退出映射。调用者必须保证状态和独占访问。

## 6. memory-mapped 进入与退出

进入：分别用 READ_CFG 和 WRITE_CFG 配置读 EB、写38的两套寄存器；等待空闲；手动写 idle timeout=32并开启 timeout；最后以 `timeout_activation=DISABLE` 调用 `ospi_memorymapped_mode()`。

它使用了两种不同的片选释放机制：refresh 针对持续访问，idle timeout 针对访问空闲。它们不是软件轮询超时，也不等于 PSRAM 软件刷新命令。

退出：DSB → HAL abort → 关闭 timeout → 强制清 FMOD。HAL abort 只在 BUSY 时执行真正 abort，因此原厂增加了最后一步强制恢复 indirect write 模式。DSB 不会替代 Write-Back 数据缓存的 Clean；原示例 PSRAM 不可缓存，所以不存在待写回的 PSRAM 脏缓存行。

### 移植中必须保留的旧库行为

原包 OSPI 库为 V0.3.0，仓库当前为 V0.6.0。两者的映射入口不同：

```c
/* 原包 V0.3.0 */
CTL = (CTL & (~TMOUTEN | ~FMOD)) | activation | MEMORY_MAPPED;
/* 当前 V0.6.0 */
CTL = (CTL & ~(TMOUTEN | FMOD)) | activation | MEMORY_MAPPED;
```

TMOUTEN 和 FMOD 位不重叠，旧表达式的 mask 实际为全1：不会清掉手动开启的 TMOUTEN。原包 AXF 反汇编也确认最后是读取 CTL 后直接 OR，没有清位。新表达式会先清 TMOUTEN，再 OR 传入的 activation；原驱动传的是 DISABLE，因此直接搭配新库会关闭 idle timeout。

另一个旧库问题：当 `timeout_activation=ENABLE` 时，该函数执行 `CTL = TMOUTIE` 整寄存器赋值。原驱动特意避开这条路径，不能简单把传参改为 ENABLE 当作修复。

其他差异包括初始化结构字段 `delay_block_bypass` 改为 `sample_delay`，以及若干命令配置分支的清位修正。字段名称、默认值和寄存器效果都应逐项核对，不能只替换头文件名。

## 7. MPU、缓存是 main 的职责

原入口先配置 MPU，再开启 I-Cache、D-Cache：

| 区域 | 属性 |
|---|---|
| 外部未使用地址窗口 | 无访问权限、XN，抑制无效外部访问 |
| AXI SRAM | 可缓存、write-through |
| OSPI0 的8 MiB PSRAM | Device、shareable、bufferable、不可缓存、XN |

因此“全局 D-Cache 开着”和“PSRAM 使用缓存”是两回事。原示例的 PSRAM **没有使用 D-Cache**。main 中对映射区调用 Clean/Invalidate，不能改变这一 MPU 属性，也不能把测试解释为缓存模式通过。

main 注释声称 Normal 属性下会挂总线；这是原厂对此样例的说明，不是所有 GD32H779 配置下的普遍结论。当前没有做过 Write-Back 配置的完整验收。

## 8. 原厂通过结果究竟覆盖了什么

测试顺序：多地址64字节间接自检（含页边界）→ 4KiB间接写/映射读 → 4KiB映射32位写/间接读 → 4KiB映射32位写/word及byte读 → 带宽 → 全8MiB间接写后读、取反后重复。

全容量 pattern 与地址相关，并且先写完整容量再开始读取，有助于发现地址镜像。原示例不包含映射 byte/half-word 写、通用 malloc、DMA、并发、长期压力或缓存策略切换。byte **读**已覆盖，byte **写**未覆盖。

映射切换的部分返回值没有检查；带宽函数也不校验传输返回值和数据。结果1在中途出现过，随后被清为0进行全容量测试，因此必须同时确认 PC 到达最终空循环。此前实测已做该确认。

## 9. 后续接入应保留与补充的边界

应保留原始命令格式、复位兼容序列、采样参数、分块策略、refresh 与 idle timeout 的实际寄存器效果，以及原入口 MPU 条件作为初始基线。

接入 RTOS 时需要重新处理：SysTick 忙等延时依赖、轮询超时、映射状态、独占访问和失败恢复；之后再单独讨论通用堆及缓存。现阶段不能把间接 byte FIFO 操作、原始映射 word 测试、或临时探针异常互相等同。先前临时 SRAM 探针出现的读回异常尚未区分真实写入、映射读回和状态切换因素，不能据此断言芯片不支持 byte/half-word。
