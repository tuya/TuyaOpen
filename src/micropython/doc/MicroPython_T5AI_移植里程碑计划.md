# MicroPython 移植到 TuyaOpen T5AI 里程碑计划

## 项目概述
将 MicroPython(v1.26.0) 作为组件集成到 TuyaOpen 框架中，专门针对 T5AI (BK7258) 平台进行优化。

## 借助AI开发
将`micropython`和`TuyaOpen`仓库下载到同一目录，
然后使用AI coding工具，分别分析`micropython`的编译体系和与编译工作，
将`micropython`中我们需要用到的源码和工具复制到`TuyaOpen`中，
再按照里程碑计划，让AI工具一步一步完成开发,
我们自己编译并验证功能，将结果反馈给AI工具。

## 里程碑规划

### 阶段 0：环境准备与可行性验证 (1-2天)
**目标**：搭建开发环境，验证基础编译流程
- [ ] 安装 TuyaOpen 开发环境和工具链
- [ ] 成功编译 TuyaOpen T5AI 示例应用
- [ ] 准备 MicroPython 源码
- [ ] 研究 BK7258 技术文档和内存布局
- [ ] 创建项目仓库和分支管理策略

**交付物**：
- 可工作的开发环境
- 编译验证报告

---

### 里程碑 1：最小可编译框架 (3-5天) ✅ [已完成]
**目标**：建立 MicroPython 组件基础架构，实现空壳编译

#### 主要任务：
1. **创建组件目录结构**
   ```
   TuyaOpen/src/micropython/
   ├── CMakeLists.txt
   ├── Kconfig
   ├── include/
   ├── port/
   │   └── t5ai/
   ├── mpy/              # MicroPython 源码（保持原结构）
   │   ├── py/          # 核心 VM + 生成工具
   │   ├── extmod/
   │   ├── shared/
   │   ├── lib/
   │   └── tools/       # 其他工具
   └── genhdr/          # 生成的头文件
   ```

2. **配置 CMake 集成**
   - 编写顶层 CMakeLists.txt
   - 集成到 TuyaOpen 构建系统
   - 配置编译选项和链接参数

3. **创建最小端口文件**
   - main.c (空实现)
   - mphalport.h/c (基础定义)
   - mpconfigport.h (最小配置)

**验收标准**：
- `tos.py build` 能成功编译包含 MicroPython 组件
- 生成的固件能烧录但暂无功能

---

### 里程碑 2：核心运行时移植 (7-10天) ✅ [已完成]
**目标**：实现 MicroPython 核心解释器在 T5AI 上运行

#### 实施策略
**"完整复制，选择性编译"** - 复制 MicroPython 所有源码，通过 CMakeLists.txt 控制编译范围，从最小核心逐步扩展

#### 重要约定
**源码复制规则**：所有从 MicroPython 仓库复制的文件都放置在 `TuyaOpen/src/micropython/mpy/` 目录下，并保持原有的目录结构不变。

目录映射关系：
- `micropython/py/` → `TuyaOpen/src/micropython/mpy/py/` （包含生成工具）
- `micropython/extmod/` → `TuyaOpen/src/micropython/mpy/extmod/`
- `micropython/shared/` → `TuyaOpen/src/micropython/mpy/shared/`
- `micropython/lib/` → `TuyaOpen/src/micropython/mpy/lib/`
- `micropython/tools/` → `TuyaOpen/src/micropython/mpy/tools/`

**注意**：生成工具脚本（如 makeqstrdata.py、makemoduledefs.py 等）位于 `py/` 目录中，而非 `tools/` 目录。

**头文件生成策略**：
- QSTR 文件通过扫描源码动态生成
- mpversion.h 使用固定内容（不需要 Git 信息获取）
- 其他头文件根据实际需要生成

#### 主要任务：

1. **源码和工具准备**
   - 复制 MicroPython 完整源码到 `TuyaOpen/src/micropython/mpy/`
     * `mpy/py/` - 核心虚拟机和生成工具
       - 所有 .c 和 .h 文件（核心 VM）
       - makeqstrdata.py（QSTR 生成）
       - makeversionhdr.py（版本信息）
       - makemoduledefs.py（模块定义）
       - make_root_pointers.py（GC 根指针）
       - makecompresseddata.py（压缩数据）
     * `mpy/shared/` - 共享组件（runtime, libc, readline 等）
     * `mpy/extmod/` - 扩展模块（备用）
     * `mpy/lib/` - 第三方库（备用）
     * `mpy/tools/` - 其他工具脚本（如 mpy-tool.py）

2. **构建系统配置**
   - 创建预编译头文件生成系统
     * 使用 `mpy/py/makeqstrdata.py` 生成 qstrdefs.generated.h
     * 生成固定内容的 mpversion.h（三个宏定义：GIT_TAG、GIT_HASH、BUILD_DATE）
     * 使用 `mpy/py/makemoduledefs.py` 生成 moduledefs.h
     * 使用 `mpy/py/make_root_pointers.py` 生成 root_pointers.h
     * 使用 `mpy/py/makecompresseddata.py` 生成 compressed.data.h
   - 实现 QSTR 自动生成机制
     * 扫描源文件提取 MP_QSTR_* 使用
     * 生成 QSTR 枚举和数据结构
   - 配置 CMakeLists.txt 选择最小编译集
     * 参考 minimal port 的文件列表
     * 使用条件编译控制功能模块
     * 逐步添加文件直到链接成功

3. **平台适配层实现**
   - 基础硬件抽象
     * mp_hal_stdout_tx_* (串口输出)
     * mp_hal_ticks_ms (系统时钟)
     * mp_hal_delay_* (延时函数)
   - 内存管理对接
     * 使用 TAL malloc/free
     * 配置 GC 堆大小和位置
   - 中断和任务管理
     * 集成到 TAL 线程系统

4. **最小运行时验证**
   - 使用 MICROPY_CONFIG_ROM_LEVEL_MINIMUM 配置
   - 启用必要功能：编译器、GC、REPL
   - 实现 do_str() 执行简单 Python 代码
   - 基础 REPL 循环实现

**验收标准**：
- 项目能够成功编译，无 QSTR 未定义错误
- 能执行 `print("Hello from MicroPython on T5AI!")`
- 基础 REPL 可以响应输入
- 支持基本 Python 语法（变量、函数、循环等）
- gc模块内存回收功能验证

```python
import gc

dir(gc)

print("Step1: Init")
gc.mem_free()
gc.mem_alloc()

print("Step2: Use some memory")
a = [1, 2, 3, 4, 5] * 100
gc.mem_free()
gc.mem_alloc()

print("Step3: Free some memory")
del a
gc.collect()
gc.mem_free()
gc.mem_alloc()
```

---

### 里程碑 3：标准库与外设驱动 (10-14天)
**目标**：实现常用 Python 模块和硬件外设控制

#### 主要任务：
1. **标准模块移植**
   - builtins 模块完善
   - sys, os 基础功能
   - time 模块
   - struct, json 等数据处理模块

2. **硬件驱动开发**
   - GPIO (machine.Pin)
   - UART (machine.UART)
   - I2C/SPI (machine.I2C/SPI)
   - PWM (machine.PWM)
   - ADC (machine.ADC)

3. **TAL 层对接**
   - 使用 TAL GPIO 接口
   - 使用 TAL 串口接口
   - 使用 TAL 定时器接口

**验收标准**：
- 能控制 LED、读取按键
- 串口通信正常
- I2C/SPI 设备能正常访问

---

### 里程碑 4：网络功能集成 (7-10天)
**目标**：实现 WiFi 和网络协议栈支持

#### 主要任务：
1. **WiFi 模块**
   - network.WLAN 实现
   - AP/STA 模式支持
   - 扫描和连接功能

2. **Socket 支持**
   - usocket 模块移植
   - TCP/UDP 协议
   - 与 TuyaOpen 网络栈对接

3. **高级网络功能**
   - urequests (HTTP 客户端)
   - 简单 Web 服务器
   - MQTT 基础支持

**验收标准**：
- 能连接 WiFi 网络
- 能进行 HTTP 请求
- 能建立 TCP/UDP 连接

---

### 里程碑 5：文件系统与存储 (5-7天)
**目标**：实现持久化存储和文件系统

#### 主要任务：
1. **Flash 存储管理**
   - 分区规划 (代码区、文件系统区)
   - Flash 读写驱动

2. **文件系统实现**
   - LittleFS 或 FAT 集成
   - uos 模块文件操作
   - 配置文件存储

3. **冻结模块支持**
   - 预编译 Python 模块
   - 优化启动时间和内存使用

**验收标准**：
- 能创建、读写文件
- Python 脚本能持久化存储
- 系统配置能保存

---

### 里程碑 6：Tuya 云服务集成 (10-14天)
**目标**：实现与 Tuya 云平台的无缝对接

#### 主要任务：
1. **Tuya SDK 封装**
   - 设备激活流程
   - 数据点上报/下发
   - OTA 升级支持

2. **Python API 设计**
   - tuya 模块实现
   - 简化的设备模型
   - 事件回调机制

3. **示例应用**
   - 智能开关 Demo
   - 传感器数据上报
   - 场景联动示例

**验收标准**：
- 能通过 Python 代码连接 Tuya 云
- 能上报设备状态
- 能接收云端控制指令

---

### 里程碑 7：AI 功能支持 (14-21天)
**目标**：集成 T5AI 的 AI 能力到 Python

#### 主要任务：
1. **语音功能**
   - 音频采集/播放
   - 本地语音识别接口
   - TTS 支持

2. **AI 模型运行**
   - TensorFlow Lite Micro 集成
   - 模型加载和推理
   - Python 接口封装

3. **视觉处理** (如果支持)
   - 摄像头驱动
   - 图像处理库
   - 简单 CV 算法

**验收标准**：
- 能进行语音唤醒
- 能运行简单 AI 模型
- Python 可调用 AI 功能

---

### 里程碑 8：优化与产品化 (7-10天)
**目标**：性能优化和产品级质量保证

#### 主要任务：
1. **性能优化**
   - 内存使用优化
   - 启动时间优化
   - 功耗优化

2. **稳定性提升**
   - 异常处理完善
   - 看门狗集成
   - 内存泄漏检查

3. **开发者工具**
   - VSCode 插件支持
   - 调试工具
   - 性能分析工具

4. **文档完善**
   - API 文档
   - 移植指南
   - 示例代码库

**验收标准**：
- 72小时稳定性测试通过
- 内存使用 < 100KB (基础系统)
- 完整的开发文档

---

## 时间预估汇总

| 里程碑 | 预计时间 | 累计时间 |
|--------|---------|----------|
| 阶段0：环境准备 | 1-2天 | 2天 |
| 里程碑1：最小框架 | 3-5天 | 7天 |
| 里程碑2：核心运行时 | 7-10天 | 17天 |
| 里程碑3：标准库与外设 | 10-14天 | 31天 |
| 里程碑4：网络功能 | 7-10天 | 41天 |
| 里程碑5：文件系统 | 5-7天 | 48天 |
| 里程碑6：Tuya云集成 | 10-14天 | 62天 |
| 里程碑7：AI功能 | 14-21天 | 83天 |
| 里程碑8：优化与产品化 | 7-10天 | 93天 |

**总计：约3-4个月完成全功能移植**

## 最小可用版本 (MVP)
如果需要快速获得可用版本，可以在**里程碑3**完成后得到基础可用的 MicroPython：
- 时间：约1个月
- 功能：基础 Python 运行时 + 硬件控制
- 适用：原型开发、功能验证

## 风险与挑战

1. **内存限制**
   - T5AI 的 RAM/PSRAM 分配策略需要优化
   - 可能需要裁剪部分 Python 功能

2. **实时性要求**
   - 语音处理对实时性要求高
   - 需要合理设计中断和任务优先级

3. **工具链兼容性**
   - BK7258 特定的编译器优化选项
   - 调试工具的适配

4. **文档依赖**
   - BK7258 详细技术文档的获取
   - TAL API 的完整性

## 建议实施策略

1. **快速迭代**：每个里程碑都产出可运行版本
2. **持续集成**：建立自动化测试和构建流程
3. **社区参与**：开源部分代码，获取反馈
4. **灵活调整**：根据实际进展调整计划

## 技术支持需求

- BK7258 详细技术手册
- TuyaOpen TAL API 文档
- T5AI 硬件原理图
- Tuya 云 API 接口文档
- 测试硬件若干套

---

## 项目进度跟踪 TODO List

### 阶段 0：环境准备与可行性验证 ✅
- [x] 安装 TuyaOpen 开发环境
- [x] 配置 ARM 交叉编译工具链
- [x] 编译运行 T5AI 示例程序
- [x] 下载 MicroPython 源码
- [x] 分析 BK7258 技术规格
- [x] 创建 Git 项目仓库

### 里程碑 1：最小可编译框架 ✅
- [x] 创建 src/micropython 目录结构
- [x] 编写顶层 CMakeLists.txt
- [x] 创建 Kconfig 配置文件
- [x] 实现空的 main.c
- [x] 创建 mphalport.h/c 基础文件
- [x] 编写 mpconfigport.h 最小配置
- [x] 集成到 TuyaOpen 构建系统
- [x] 成功编译生成固件

### 里程碑 2：核心运行时移植 ✅

- [x] 复制 MicroPython 完整源码到 mpy/ 目录（保持原目录结构）
- [x] 确认 py/ 目录中包含所有生成工具脚本
- [x] 配置 CMakeLists.txt 最小编译文件集
- [x] 实现 QSTR 自动生成（mpy_prepare.cmake）
- [x] 实现 mpversion.h 生成（固定内容）
- [x] 实现其他头文件生成（moduledefs.h, root_pointers.h, compressed.data.h）
- [x] 实现 mp_hal_stdout_tx_* 串口输出
- [x] 实现 mp_hal_ticks_ms 系统时钟
- [x] 实现 mp_hal_delay_* 延时函数
- [x] 配置 TAL 内存分配器对接
- [x] 设置 MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES
- [x] 成功编译生成固件
- [x] 实现 do_str() 函数
- [x] 实现基础 REPL 循环
- [x] 测试 print() 函数
- [x] 测试基础 Python 语句执行
- [x] 验证内存管理正常工作（gc 模块）

### 里程碑 3：标准库与外设驱动
- [ ] 移植 builtins 模块
- [ ] 实现 sys 模块基础功能
- [ ] 实现 os 模块基础功能
- [ ] 移植 time 模块
- [ ] 实现 struct 模块
- [ ] 实现 json 模块
- [ ] 开发 machine.Pin (GPIO)
- [ ] 开发 machine.UART
- [ ] 开发 machine.I2C
- [ ] 开发 machine.SPI
- [ ] 开发 machine.PWM
- [ ] 开发 machine.ADC
- [ ] 对接 TAL GPIO 接口
- [ ] 对接 TAL 串口接口
- [ ] 对接 TAL 定时器接口

### 里程碑 4：网络功能集成
- [ ] 实现 network 模块框架
- [ ] 开发 network.WLAN 类
- [ ] 实现 WiFi STA 模式
- [ ] 实现 WiFi AP 模式
- [ ] 实现 WiFi 扫描功能
- [ ] 移植 usocket 模块
- [ ] 实现 TCP 客户端/服务器
- [ ] 实现 UDP 通信
- [ ] 集成 urequests 模块
- [ ] 实现简单 HTTP 服务器
- [ ] 添加 MQTT 客户端支持

### 里程碑 5：文件系统与存储
- [ ] 规划 Flash 分区布局
- [ ] 实现 Flash 读写驱动
- [ ] 集成 LittleFS 文件系统
- [ ] 实现 uos 文件操作
- [ ] 支持配置文件存储
- [ ] 实现冻结模块机制
- [ ] 优化模块加载速度
- [ ] 测试文件持久化功能

### 里程碑 6：Tuya 云服务集成
- [ ] 分析 Tuya SDK 接口
- [ ] 封装设备激活流程
- [ ] 实现数据点上报机制
- [ ] 实现数据点下发处理
- [ ] 支持 OTA 升级功能
- [ ] 设计 Python tuya 模块 API
- [ ] 实现设备状态管理
- [ ] 开发事件回调系统
- [ ] 编写智能开关 Demo
- [ ] 编写传感器上报 Demo
- [ ] 编写场景联动示例

### 里程碑 7：AI 功能支持
- [ ] 实现音频采集驱动
- [ ] 实现音频播放驱动
- [ ] 集成语音唤醒引擎
- [ ] 封装语音识别接口
- [ ] 实现 TTS 功能
- [ ] 集成 TensorFlow Lite Micro
- [ ] 实现模型加载机制
- [ ] 封装推理 API
- [ ] 开发 Python AI 模块
- [ ] 编写语音控制示例
- [ ] 编写 AI 推理示例

### 里程碑 8：优化与产品化
- [ ] 分析内存使用情况
- [ ] 优化内存分配策略
- [ ] 缩短启动时间
- [ ] 实现低功耗模式
- [ ] 完善异常处理
- [ ] 集成看门狗
- [ ] 进行内存泄漏测试
- [ ] 72小时稳定性测试
- [ ] 开发 VSCode 插件
- [ ] 编写调试工具
- [ ] 实现性能分析器
- [ ] 编写 API 文档
- [ ] 完成移植指南
- [ ] 整理示例代码库
- [ ] 发布 v1.0 版本

### 项目管理
- [ ] 建立 CI/CD 流程
- [ ] 配置自动化测试
- [ ] 创建问题跟踪系统
- [ ] 制定代码审查流程
- [ ] 准备技术分享材料
- [ ] 组织内部培训
