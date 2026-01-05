# TuyaOpen T5AI 编译系统说明

## 目录
1. [概述](#概述)
2. [源码目录结构](#源码目录结构)
3. [编译环境准备](#编译环境准备)
4. [编译流程详解](#编译流程详解)
5. [CMake构建体系](#cmake构建体系)
6. [平台特定构建](#平台特定构建)

## 概述

TuyaOpen 是涂鸦智能提供的开源 AI+IoT 开发框架，用于快速开发智能互联设备。T5AI 是基于 BK7258 芯片的 AI 开发板，支持语音识别、AI 交互等功能。本文档详细说明针对 T5AI 平台的编译系统和构建流程。

## 源码目录结构

### TuyaOpen 主要目录

```
TuyaOpen/
├── apps/               # 应用示例
│   ├── tuya_cloud/    # 涂鸦云连接示例
│   ├── tuya.ai/       # AI 应用示例
│   └── games/         # 游戏示例
│
├── boards/            # 开发板配置
│   ├── T5AI/         # T5AI 板级配置
│   ├── ESP32/        # ESP32 板级配置
│   └── ...           # 其他开发板
│
├── examples/          # 功能示例代码
│   ├── get-started/  # 入门示例
│   ├── ble/          # 蓝牙示例
│   ├── wifi/         # WiFi 示例
│   ├── peripherals/  # 外设示例
│   └── graphics/     # 图形显示示例
│
├── src/              # SDK 核心源码
│   ├── common/       # 公共模块
│   ├── tal_system/   # 系统抽象层
│   ├── tal_wifi/     # WiFi 抽象层
│   ├── tal_bluetooth/# 蓝牙抽象层
│   ├── tal_driver/   # 驱动抽象层
│   ├── tal_network/  # 网络抽象层
│   ├── tal_security/ # 安全模块
│   ├── tuya_cloud_service/  # 云服务
│   ├── tuya_ai_basic/       # AI 基础服务
│   ├── liblvgl/      # LVGL 图形库
│   └── libcjson/     # JSON 库
│
├── platform/         # 平台相关代码
│   ├── T5AI/        # T5AI 平台包（自动下载）
│   └── platform_config.yaml  # 平台配置
│
├── tools/           # 构建工具
│   ├── cli_command/ # CLI 命令实现
│   ├── cmake/       # CMake 工具
│   └── kconfiglib/  # Kconfig 工具
│
├── CMakeLists.txt   # 根 CMake 配置
├── tos.py          # 主构建脚本
└── export.sh       # 环境设置脚本
```

### T5AI 平台目录（platform/T5AI/）

```
platform/T5AI/
├── t5_os/                    # T5 操作系统
│   ├── ap/                  # 应用处理器代码
│   ├── cp/                  # 通信处理器代码
│   └── build/              # 构建配置
│
├── tuyaos/                  # TuyaOS 适配层
│   └── tuyaos_adapter/     # 平台适配代码
│
├── toolchain_file.cmake    # 工具链配置
├── platform_config.cmake   # 平台 CMake 配置
├── build_example.py        # 构建脚本
└── platform_prepare.py     # 平台准备脚本
```

## 编译环境准备

### 1. 系统要求

- 操作系统：Ubuntu 18.04+ / macOS / Windows (WSL)
- Python：3.6+
- Git：2.0+
- CMake：3.16+
- Ninja：1.10+

### 2. 获取源码

```bash
git clone https://github.com/tuya/tuyaopen.git
cd tuyaopen
```

### 3. 设置环境

```bash
# Linux/macOS
source export.sh

# Windows
export.bat

# 安装 Python 依赖
pip install -r requirements.txt
```

## 编译流程详解

### 完整编译步骤（以 switch_demo 为例）

#### 步骤 1：进入应用目录
```bash
cd apps/tuya_cloud/switch_demo
```

#### 步骤 2：配置板级选项
```bash
tos.py config choice
# 选择 T5AI.config
```

此命令执行的操作：
- 清理之前的构建文件
- 显示可用的板级配置列表
- 将选择的配置复制到 `app_default.config`
- 生成 `using.config` 完整配置

#### 步骤 3：构建项目
```bash
tos.py build
```

### 详细构建流程

#### 1. 环境检查阶段 (`env_check`)
- 检查并更新 git 子模块
- 验证依赖项

#### 2. 平台下载阶段 (`download_platform`)
```python
# 检查 platform/T5AI 目录是否存在
# 如不存在，从 GitHub 克隆：
git clone https://github.com/tuya/TuyaOpen-T5AI platform/T5AI
git checkout 046792bf3443b62994b6d4bc795fcbf42b305eb5
```

#### 3. 平台准备阶段 (`prepare_platform`)
```bash
# 执行 platform/T5AI/platform_prepare.py
# 下载工具链和依赖库
# 设置编译环境变量
```

#### 4. 构建设置阶段 (`build_setup`)
```bash
# 执行 platform/T5AI/build_setup.py
# 传递参数：项目名、平台、框架、芯片
python build_setup.py switch_demo T5AI tuyaopen T5
```

#### 5. CMake 配置阶段 (`cmake_configure`)
```bash
cd .build
cmake -G Ninja /path/to/TuyaOpen \
    -DTOS_PROJECT_NAME=switch_demo \
    -DTOS_PROJECT_ROOT=/path/to/app \
    -DTOS_PROJECT_PLATFORM=T5AI \
    -DTOS_FRAMEWORK=tuyaopen \
    -DTOS_PROJECT_CHIP=T5 \
    -DTOS_PROJECT_BOARD=T5AI
```

#### 6. Ninja 构建阶段 (`ninja_build`)
```bash
# 使用 Ninja 编译 SDK 和应用
ninja tuyaapp
```

生成的库文件：
- `tuyaos.a` - SDK 核心库
- `tuyaapp.a` - 应用库

#### 7. 平台构建阶段 (`build_example.py`)
```python
# 设置环境变量
export TUYA_HEADER_DIR=...
export TUYA_LIBS_DIR=...
export TUYA_LIBS="tuyaapp tuyaos"

# 执行平台 Makefile
cd platform/T5AI/t5_os/build/bk7258
make bk7258 PROJECT=tuya_app APP_NAME=switch_demo -j
```

#### 8. 固件生成阶段
```bash
# 生成 UA 文件（用户应用）
python create_ua_file.py -> switch_demo_QIO_1.0.0.bin

# 生成 UG 文件（升级包）
python create_ug_file.py -> switch_demo_UG_1.0.0.bin

# 复制到 dist/ 目录
dist/switch_demo_1.0.0/
├── switch_demo_QIO_1.0.0.bin  # 完整固件
└── switch_demo_UG_1.0.0.bin   # OTA 升级包
```

## CMake构建体系

### 1. 分层模块化设计

```
根 CMakeLists.txt
├── Kconfig 配置管理
├── 组件自动发现
├── 平台工具链加载
└── 构建参数生成
```

### 2. 组件编译流程

```cmake
# 自动扫描 src/ 下所有组件
list_components(COMPONENT_LIST "${TOP_SOURCE_DIR}/src")

# 添加每个组件
foreach(comp ${COMPONENT_LIST})
    add_subdirectory("${TOP_SOURCE_DIR}/src/${comp}")
endforeach()

# 合并成大库
add_library(tuyaos STATIC ${all_component_objects})
```

### 3. 应用集成

```cmake
# 创建应用库
add_library(tuyaapp)
target_sources(tuyaapp PRIVATE ${APP_SRCS})
target_link_libraries(tuyaapp tuyaos)
```

## 平台特定构建

### T5AI 平台特点

1. **双核架构**
   - AP（Application Processor）：运行应用代码
   - CP（Communication Processor）：处理通信协议

2. **工具链**
   - 编译器：基于 GCC 的定制工具链
   - 构建系统：Make + CMake 混合

3. **内存分区**
   ```
   Flash 分区：
   ├── Bootloader    # 引导程序
   ├── CP Firmware   # 通信处理器固件
   ├── AP Firmware   # 应用处理器固件
   ├── User Data     # 用户数据区
   └── OTA Area      # OTA 升级区
   ```

4. **固件格式**
   - `.bin` - 二进制固件
   - `_QIO.bin` - 完整烧录固件（包含所有分区）
   - `_UG.bin` - OTA 升级包

### 编译产物

```
build-T5AI/
├── .build/              # CMake 构建目录
│   ├── lib/            # 静态库
│   └── include/        # 头文件
│
├── dist/               # 最终固件
│   └── switch_demo_1.0.0/
│       ├── switch_demo_QIO_1.0.0.bin
│       └── switch_demo_UG_1.0.0.bin
│
└── platform/T5AI/      # 平台构建文件
    └── t5_os/build/
```

## 常用命令

### 基本操作

```bash
# 配置板级
tos.py config choice    # 选择配置
tos.py config menu      # 菜单配置
tos.py config save      # 保存配置

# 构建
tos.py build           # 构建项目
tos.py build -v        # 详细输出
tos.py clean           # 清理构建

# 烧录（需要烧录工具）
tos.py flash           # 烧录固件
tos.py monitor         # 串口监控

# 开发
tos.py new <app_name>  # 创建新应用
tos.py update          # 更新平台
```

### 高级选项

```bash
# 自定义平台路径
export PLATFORM_PATH=/custom/path/to/platform

# 跳过平台更新检查
touch platform/T5AI/.dont_update_platform

# 并行编译
tos.py build -j8

# 调试构建
tos.py build -d
```

## 总结

TuyaOpen 的编译系统具有以下特点：

1. **统一构建接口**：通过 `tos.py` 提供一致的命令行接口
2. **模块化设计**：SDK 组件独立管理，易于维护和扩展
3. **自动化管理**：自动下载平台代码、工具链和依赖
4. **灵活配置**：通过 Kconfig 系统管理配置选项
5. **跨平台支持**：同一套代码可编译到不同硬件平台
6. **两阶段构建**：CMake 构建 SDK + 平台特定构建

整个编译流程从配置选择到固件生成完全自动化，开发者只需关注应用逻辑开发，大大提高了开发效率。