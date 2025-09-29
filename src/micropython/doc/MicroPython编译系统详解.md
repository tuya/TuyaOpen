# MicroPython 编译系统详解

## 目录
1. [概述](#概述)
2. [源代码架构](#源代码架构)
3. [编译工具链](#编译工具链)
4. [编译流程](#编译流程)
5. [STM32平台编译示例](#stm32平台编译示例)
6. [内存布局和链接](#内存布局和链接)

## 概述

MicroPython 是 Python 3 的精简高效实现，专门为微控制器和嵌入式系统设计。本文档详细说明 MicroPython 的编译系统架构、源代码组织和构建流程。

## 源代码架构

MicroPython 采用分层架构设计，代码组织清晰，各部分职责明确：

### 1. `py/` - Python 虚拟机核心（最重要）

这是 MicroPython 的心脏，包含完整的 Python 解释器实现：

#### 词法分析和解析器
- `lexer.c` - 词法分析器，将源码分解为 token
- `parse.c` - 语法解析器，构建抽象语法树(AST)
- `scope.c` - 作用域分析，处理变量作用域

#### 编译器
- `compile.c` - 主编译器，将 AST 转换为字节码
- `emitbc.c` - 字节码发射器
- `emitglue.c` - 编译器粘合代码
- `emitnative.c` - 本地代码生成器（可选）
- `asmthumb.c/asmx64.c/asmarm.c` - 各架构汇编器

#### 虚拟机运行时
- `vm.c` - 字节码虚拟机主循环
- `runtime.c` - 运行时支持（函数调用、异常等）
- `bc.c` - 字节码相关工具
- `nlr*.c` - 非局部返回（异常处理机制）

#### 内存管理
- `malloc.c` - 内存分配器
- `gc.c` - 垃圾回收器（标记-清除算法）
- `pystack.c` - Python 栈管理
- `qstr.c` - 字符串池（QSTR）管理

#### 对象系统 (obj*.c)
```
objint.c      - 整数对象
objfloat.c    - 浮点数对象
objstr.c      - 字符串对象
objlist.c     - 列表对象
objdict.c     - 字典对象
objtuple.c    - 元组对象
objset.c      - 集合对象
objarray.c    - 数组对象
objfun.c      - 函数对象
objtype.c     - 类型系统
objexcept.c   - 异常对象
objgenerator.c - 生成器
```

#### 内置模块
- `modbuiltins.c` - builtins 模块
- `modsys.c` - sys 模块
- `modmath.c` - math 模块
- `modio.c` - io 模块
- `modstruct.c` - struct 模块
- `modgc.c` - gc 模块

### 2. `extmod/` - 扩展模块

提供硬件抽象和高级功能：

#### machine 模块（硬件抽象）
```
machine_i2c.c    - I2C 总线
machine_spi.c    - SPI 总线
machine_uart.c   - UART 串口
machine_adc.c    - ADC 模数转换
machine_pwm.c    - PWM 脉宽调制
machine_timer.c  - 定时器
machine_wdt.c    - 看门狗
machine_pinbase.c - GPIO 基础
```

#### 网络和通信
- `modbluetooth.c` - 蓝牙 BLE
- `modlwip.c` - TCP/IP 协议栈
- `modssl.c` - SSL/TLS 加密
- `modwebsocket.c` - WebSocket
- `modasyncio.c` - 异步 I/O

#### 文件系统
- `vfs.c` - 虚拟文件系统框架
- `vfs_fat.c` - FAT 文件系统
- `vfs_lfs.c` - LittleFS 文件系统
- `vfs_posix.c` - POSIX 文件系统

### 3. `shared/` - 共享组件

可在多个平台间复用的代码：

- **`runtime/`** - 运行时辅助
  - `pyexec.c` - REPL 执行器
  - `gchelper.c` - GC 辅助（汇编）
  - `interrupt_char.c` - Ctrl+C 中断
  - `softtimer.c` - 软件定时器

- **`libc/`** - 精简 C 库
- **`readline/`** - 命令行编辑
- **`netutils/`** - 网络工具（DHCP 等）
- **`timeutils/`** - 时间处理
- **`tinyusb/`** - USB 栈

### 4. `lib/` - 第三方库

```
stm32lib/     - STM32 HAL 库
littlefs/     - 小型文件系统
lwip/         - TCP/IP 协议栈
mbedtls/      - TLS/SSL 加密
btstack/      - 蓝牙协议栈
libm/         - 数学库
micropython-lib/ - Python 标准库
```

### 5. `ports/stm32/` - STM32 平台代码

#### 系统核心
```
main.c         - 主入口
stm32_it.c     - 中断处理
systick.c      - 系统时钟
pendsv.c       - PendSV 中断（任务切换）
powerctrl.c    - 电源管理
```

#### 外设驱动
```
uart.c         - UART 实现
i2c.c          - I2C 实现
spi.c          - SPI 实现
adc.c          - ADC 实现
dac.c          - DAC 实现
timer.c        - 定时器
rtc.c          - 实时时钟
dma.c          - DMA 控制器
can.c          - CAN 总线
```

#### 存储系统
```
flash.c        - 内部 Flash
flashbdev.c    - Flash 块设备
sdcard.c       - SD 卡驱动
storage.c      - 存储管理
```

#### USB 功能
```
usb.c          - USB 核心
usbd_conf.c    - USB 配置
usbd_desc.c    - USB 描述符
usbd_cdc_interface.c  - USB CDC（串口）
usbd_hid_interface.c  - USB HID（键鼠）
usbd_msc_interface.c  - USB MSC（存储）
```

## 编译工具链

### GCC 工具链配置

MicroPython 使用 ARM GCC 交叉编译工具链。工具链的配置分布在多个文件中：

#### 1. 交叉编译器前缀配置
在 `ports/stm32/Makefile` 第63行定义：
```makefile
CROSS_COMPILE ?= arm-none-eabi-
```

#### 2. 工具链组件定义
在 `py/mkenv.mk` 第32-42行定义了具体的工具：
```makefile
AS = $(CROSS_COMPILE)as         # 汇编器
CC = $(CROSS_COMPILE)gcc        # C 编译器
CPP = $(CC) -E                  # C 预处理器
CXX = $(CROSS_COMPILE)g++       # C++ 编译器
GDB = $(CROSS_COMPILE)gdb       # 调试器
LD = $(CROSS_COMPILE)ld         # 链接器
OBJCOPY = $(CROSS_COMPILE)objcopy  # 对象文件转换
SIZE = $(CROSS_COMPILE)size     # 大小分析
STRIP = $(CROSS_COMPILE)strip   # 符号剥离
AR = $(CROSS_COMPILE)ar         # 静态库工具
```

通过 `?=` 操作符，用户可以通过环境变量或命令行参数覆盖默认的工具链：
```bash
# 使用自定义工具链
make CROSS_COMPILE=/path/to/custom/toolchain/bin/arm-none-eabi-
```

### 编译参数配置

#### CPU 架构相关（以 STM32F4 为例）
```makefile
# Cortex-M4 核心配置
CFLAGS_CORTEX_M = -mthumb                    # Thumb 指令集
CFLAGS_MCU_f4 = -mtune=cortex-m4 -mcpu=cortex-m4  # M4 核心
CFLAGS += -mfpu=fpv4-sp-d16                  # FPU 硬件浮点
CFLAGS += -mfloat-abi=hard                   # 硬浮点 ABI
```

#### 优化选项
```makefile
# Debug 模式
DEBUG=1: COPT = -Og -g -DPENDSV_DEBUG

# Release 模式
COPT = -Os -DNDEBUG      # 优化大小
CSUPEROPT = -O3          # 性能关键代码用 -O3

# 代码优化
CFLAGS += -fdata-sections -ffunction-sections  # 分段编译
LDFLAGS += -Wl,--gc-sections                  # 移除未使用代码

# LTO（链接时优化）
LTO=1: CFLAGS += -flto=auto
```

## 编译流程

### 完整编译流程概览

MicroPython 的编译过程分为四个主要阶段：

```
[准备阶段] → [预生成阶段] → [编译阶段] → [链接阶段] → [固件生成]
     ↓            ↓              ↓            ↓            ↓
 mpy-cross    生成头文件     编译C源码      链接目标文件    生成bin/hex
  (可选)      (必需)         (必需)        (必需)         (必需)
```

### 1. 准备阶段

#### 1.1 编译 mpy-cross（可选）
```bash
# 仅在需要冻结 Python 模块时需要
cd mpy-cross
make
```

**说明**：
- mpy-cross 是独立的交叉编译器，将 .py 文件编译为 .mpy 字节码
- 如果不使用 frozen 模块（MICROPY_MODULE_FROZEN_MPY=0），可跳过此步
- 用于将 Python 代码预编译并嵌入固件，节省 RAM

#### 1.2 初始化子模块
```bash
cd ports/stm32
make BOARD=NUCLEO_F446RE submodules
```

### 2. 预生成阶段（编译前必需）

这是编译前最关键的步骤，必须生成所有必需的头文件：

#### 2.1 QSTR（字符串池）生成

**生成流程**：
```bash
# 步骤1：扫描所有源文件，提取 MP_QSTR_* 使用
python tools/makeqstrdefs.py pp qstr.i.last > qstr.split

# 步骤2：收集所有 QSTR 定义
cat py/qstrdefs.h qstr.split | python tools/makeqstrdata.py > qstrdefs.collected.h

# 步骤3：生成最终的 QSTR 头文件
python tools/makeqstrdata.py qstrdefs.collected.h > qstrdefs.generated.h
```

**扫描的源文件范围**：
- `py/*.c` - 所有核心 VM 文件（除了 nlr*.c）
- `extmod/*.c` - 扩展模块文件（如果启用）
- `shared/runtime/*.c` - 运行时支持文件
- `shared/readline/*.c` - 命令行编辑
- `ports/<port>/*.c` - 平台特定文件

**生成的文件**：
- `genhdr/qstrdefs.collected.h` - 收集的 QSTR 定义
- `genhdr/qstrdefs.generated.h` - 最终的 QSTR 枚举和数据

#### 2.2 版本信息生成

```bash
python tools/makeversionhdr.py genhdr/mpversion.h
```

**生成内容**：
```c
#define MICROPY_GIT_TAG "v1.23.0"
#define MICROPY_GIT_HASH "abc123"
#define MICROPY_BUILD_DATE "2024-03-15"
#define MICROPY_VERSION_MAJOR (1)
#define MICROPY_VERSION_MINOR (23)
#define MICROPY_VERSION_MICRO (0)
```

#### 2.3 模块定义生成

```bash
# 扫描所有 MP_REGISTER_MODULE 宏
python tools/makemoduledefs.py $(SRC_QSTR) > genhdr/moduledefs.h
```

**扫描内容**：
- `MP_REGISTER_MODULE(MP_QSTR_module_name, module_obj)` - 模块注册
- `MP_REGISTER_EXTENSIBLE_MODULE(...)` - 可扩展模块注册

**生成内容**：
```c
// 自动生成的模块表条目
MODULE_DEF_MP_QSTR_BUILTINS,
MODULE_DEF_MP_QSTR_SYS,
MODULE_DEF_MP_QSTR_GC,
// ...
```

#### 2.4 GC 根指针生成

```bash
# 扫描所有 MP_REGISTER_ROOT_POINTER 宏
python tools/make_root_pointers.py $(SRC_QSTR) > genhdr/root_pointers.h
```

**扫描内容**：
```c
MP_REGISTER_ROOT_POINTER(mp_obj_t pyb_stdio_uart);
MP_REGISTER_ROOT_POINTER(mp_obj_t pyb_config_main);
```

**生成内容**：
```c
// 添加到 struct _mp_state_vm_t 的成员
mp_obj_t pyb_stdio_uart;
mp_obj_t pyb_config_main;
```

#### 2.5 压缩数据生成

```bash
# 扫描所有 MP_ERROR_TEXT 字符串
python tools/makecompresseddata.py $(SRC_QSTR) > genhdr/compressed.data.h
```

**功能**：
- 提取所有错误消息字符串
- 应用压缩算法（词频替换）
- 生成压缩数据表

**生成内容**：
```c
#define MP_MAX_UNCOMPRESSED_TEXT_LEN (80)
MP_COMPRESSED_DATA("compressed_string_data_here")
MP_MATCH_COMPRESSED("original", "compressed")
```

#### 2.6 Frozen 模块生成（可选）

```bash
# 如果定义了 FROZEN_MANIFEST
python tools/makemanifest.py \
    -o frozen_content.c \
    -v "MPY_DIR=$(TOP)" \
    -v "PORT_DIR=$(shell pwd)" \
    -b "$(BUILD)" \
    $(FROZEN_MANIFEST)
```

**功能**：
- 预编译 Python 模块为字节码
- 生成 C 代码嵌入固件
- 节省运行时 RAM

### 2.7 生成文件依赖关系图

```
源文件扫描
    ↓
┌─────────────────────────────────────────────┐
│           预生成头文件（必需）                  │
├─────────────────────────────────────────────┤
│ 1. qstrdefs.generated.h    - QSTR定义        │
│ 2. mpversion.h             - 版本信息        │
│ 3. moduledefs.h            - 模块定义表      │
│ 4. root_pointers.h         - GC根指针        │
│ 5. compressed.data.h       - 压缩错误文本    │
│ 6. qstr_pool.h             - QSTR池结构      │
└─────────────────────────────────────────────┘
    ↓
编译 C 源文件
```

### 3. 编译阶段

只有在所有头文件生成完成后，才能开始编译：

```bash
# 预处理 -> 编译 -> 汇编
arm-none-eabi-gcc -c source.c -o source.o \
    -I. -Ibuild -Igenhdr \           # 包含生成的头文件目录
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 \
    -Os -fdata-sections -ffunction-sections

# 生成依赖关系
arm-none-eabi-gcc -MM -MF source.d source.c
```

### 4. 链接阶段

```bash
arm-none-eabi-gcc \
    -T boards/stm32f411.ld \         # 链接脚本
    -Wl,-Map=firmware.map \          # 生成 map 文件
    -Wl,--gc-sections \              # 删除未使用段
    -nostdlib \                      # 不用标准库
    obj1.o obj2.o ... \              # 目标文件
    -lm -lgcc \                      # 数学库和编译器库
    -o firmware.elf
```

### 5. 固件生成

```bash
# ELF -> BIN（二进制）
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin

# ELF -> HEX（Intel HEX）
arm-none-eabi-objcopy -O ihex firmware.elf firmware.hex

# 生成 DFU 文件（USB 烧录）
python tools/pydfu.py -b 0x08000000:firmware.bin firmware.dfu

# 查看固件大小
arm-none-eabi-size firmware.elf
```

## STM32平台编译示例

以 NUCLEO-F446RE 开发板为例的完整编译流程：

### 1. 准备环境

```bash
# 安装 ARM 交叉编译工具链
sudo apt-get install gcc-arm-none-eabi

# 获取 MicroPython 源码
git clone https://github.com/micropython/micropython.git
cd micropython
```

### 2. 编译 mpy-cross

```bash
cd mpy-cross
make
cd ..
```

### 3. 编译固件

```bash
cd ports/stm32
make BOARD=NUCLEO_F446RE submodules
make BOARD=NUCLEO_F446RE
```

### 4. 板级配置文件

**mpconfigboard.mk**：
```makefile
MCU_SERIES = f4              # MCU 系列
CMSIS_MCU = STM32F446xx      # 具体型号
AF_FILE = boards/stm32f446_af.csv  # 引脚复用配置
LD_FILES = boards/stm32f411.ld     # 链接脚本
TEXT0_ADDR = 0x08000000      # Flash 起始地址
```

**mpconfigboard.h**：
```c
// 硬件配置
#define MICROPY_HW_BOARD_NAME "NUCLEO-F446RE"
#define MICROPY_HW_CLK_PLLN (336)  // PLL 倍频
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)  // 168MHz CPU

// 外设定义
#define MICROPY_HW_UART2_TX (pin_A2)  // UART 引脚
#define MICROPY_HW_I2C1_SCL (pin_B8)  // I2C 引脚
#define MICROPY_HW_SPI1_SCK (pin_A5)  // SPI 引脚
#define MICROPY_HW_LED1 (pin_A5)      // LED 引脚
```

### 5. 烧录固件

```bash
# 使用 ST-LINK
make BOARD=NUCLEO_F446RE deploy-stlink

# 使用 DFU 模式
make BOARD=NUCLEO_F446RE deploy

# 使用 OpenOCD
make BOARD=NUCLEO_F446RE deploy-openocd
```

## 内存布局和链接

### 链接脚本内存布局

```ld
MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (xrw)   : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS {
    .text : {          /* 代码段 */
        *(.isr_vector)  /* 中断向量表 */
        *(.text*)       /* 程序代码 */
        *(.rodata*)     /* 只读数据 */
    } > FLASH

    .data : {          /* 初始化数据 */
        *(.data*)
    } > RAM AT> FLASH

    .bss : {           /* 未初始化数据 */
        *(.bss*)
    } > RAM

    .heap : {          /* Python 堆 */
        _heap_start = .;
        . = . + 100K;
        _heap_end = .;
    } > RAM
}
```

### 编译产物组成

典型的 STM32F446RE 固件组成：

```
固件大小分析（约 380KB）：
├── Python VM 核心 (150KB)
│   ├── 解释器 (40KB)
│   ├── 编译器 (35KB)
│   ├── 对象系统 (45KB)
│   └── 内置模块 (30KB)
├── 扩展模块 (60KB)
│   ├── machine 模块
│   ├── 网络模块
│   └── 文件系统
├── HAL 驱动库 (80KB)
├── 平台驱动 (50KB)
├── 共享组件 (20KB)
└── 冻结代码 (20KB)

内存使用（128KB RAM）：
├── 静态数据 (2KB)
├── BSS 段 (28KB)
├── Python 堆 (80KB)
└── 系统栈 (16KB)
```

## 特殊编译技术

### 1. QSTR 优化

**原理**：将所有字符串在编译时收集到统一的字符串池

**优势**：
- 相同字符串只存储一次
- 字符串比较变为整数比较（O(1)复杂度）
- 显著减少内存占用

**实现细节**：
```c
// 使用前
if (strcmp(str, "print") == 0) { ... }  // 字符串比较

// QSTR 优化后
if (str == MP_QSTR_print) { ... }       // 整数比较
```

### 2. 冻结模块

**原理**：Python 代码在编译时转换为字节码，直接嵌入固件

**类型**：
- **Frozen String**: Python 源码以字符串形式存储在 Flash
- **Frozen MPY**: 预编译的字节码存储在 Flash

**优势**：
- 节省 RAM（代码存在 Flash 中）
- 加快启动速度（无需运行时编译）
- 保护源代码（MPY 格式）

### 3. ROM 压缩

**压缩技术**：
- 错误消息压缩（词频替换）
- 代码段压缩（可选的 XIP 压缩）
- 数据段压缩

**实现**：
```python
# makecompresseddata.py 使用词频分析
# 高频词汇用短编码替换
"memory allocation failed" → "\x80\x81\x82"  # 压缩表示
```

### 4. 内联汇编

**使用场景**：
- GC 根指针扫描（gchelper.s）
- 上下文切换（nlr 实现）
- 性能关键路径

**示例**：
```c
// 内联 Thumb 汇编示例
__attribute__((naked)) void gc_helper_get_regs(regs_t *regs) {
    __asm volatile (
        "str r0, [r0, #0]\n"
        "str r1, [r0, #4]\n"
        // ...
        "bx lr\n"
    );
}
```

## 编译前准备清单

### 必需步骤（按顺序）

1. **工具链安装**
   - [ ] 安装 Python 3.x
   - [ ] 安装交叉编译器（如 arm-none-eabi-gcc）
   - [ ] 安装 make 工具

2. **mpy-cross 构建**（如果使用 frozen 模块）
   - [ ] 编译 mpy-cross 工具
   - [ ] 验证 mpy-cross 可执行

3. **头文件生成**（编译前必需）
   - [ ] 生成 qstrdefs.generated.h
   - [ ] 生成 mpversion.h
   - [ ] 生成 moduledefs.h
   - [ ] 生成 root_pointers.h
   - [ ] 生成 compressed.data.h
   - [ ] 确保 qstr_pool.h 存在

4. **源文件准备**
   - [ ] 确认所有源文件就位
   - [ ] 配置文件设置完成
   - [ ] 平台适配代码准备

5. **编译环境检查**
   - [ ] 包含路径正确设置
   - [ ] 编译选项配置完成
   - [ ] 链接脚本准备就绪

## 编译选项说明

### 常用编译参数

```bash
# 启用链接时优化（减小固件大小）
make BOARD=NUCLEO_F446RE LTO=1

# 详细编译输出
make BOARD=NUCLEO_F446RE V=1

# 清理编译文件
make BOARD=NUCLEO_F446RE clean

# 自定义冻结模块
make BOARD=NUCLEO_F446RE FROZEN_MANIFEST=my_manifest.py
```

### 调试选项

```bash
# 启用调试信息
make BOARD=NUCLEO_F446RE DEBUG=1

# 生成汇编文件
make BOARD=NUCLEO_F446RE CFLAGS_EXTRA="-save-temps"
```

## 总结

MicroPython 的编译系统通过精心设计的模块化架构，实现了：

1. **高度可移植性**：核心 VM 与平台无关，易于移植到新平台
2. **可裁剪性**：通过配置选择需要的功能，适应不同资源限制
3. **易扩展性**：模块化设计便于添加新功能
4. **代码复用**：共享组件在各平台间复用
5. **优化效率**：多级优化技术确保代码大小和执行效率

整个编译系统通过 Makefile 精心组织，支持灵活配置，能够针对不同 MCU 和应用场景进行优化，是嵌入式 Python 实现的典范。