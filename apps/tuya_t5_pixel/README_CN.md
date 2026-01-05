# Tuya T5AI 像素LED演示程序

## 项目背景

**Tuya-T5-Pixels** 是一个基于TuyaOpen开源框架打造的高密度紧凑像素屏项目。它采用32x32 LED像素矩阵（1024颗WS2812 RGB LED）搭配涂鸦T5 WIFI/蓝牙芯片模组，从硬件设计到软件代码全开源。


<p align="center">
  <img src="https://image.lceda.cn/pullimage/YReJW2gWZRjsvgAM7G5vERZ7oYyRDvnGN0HqK8Hq.png" alt="Tuya T5AI Pixel 硬件展示" width="600"/>
</p>
<p align="center">
  <img src="https://image.lceda.cn/pullimage/sgucMkrAWAtZ2Zac9KTDHiWnw0mJFqYpy3Zvw3Lr.png" alt="Tuya T5AI Pixel 硬件展示" width="600"/>
</p>


### 核心硬件特性

- **1024颗全彩单线灯**（WS2812）32x32矩阵
- **多传感器支持：** 温度/湿度/压力（BME280）、VOC空气质量（SGP41）、环境光（BH1750）、6轴加速度陀螺仪（BMI270）
- **音频I/O：** MEMS麦克风 + 喇叭 + 硬件回采
- **用户界面：** 3个用户按键（OK/A/B）、USB-C供电、双路烧录/调试串口（CH342F）
- **连接性：** 通过Tuya T5模组实现WiFi/蓝牙，支持App IoT控制
- **尺寸：** 85mm × 75mm 紧凑设计

> ⚠️ **亮度建议：**  
> 请勿将LED全局亮度（如`BRIGHTNESS`参数）设置为最大。鉴于32×32高密度像素阵列（1024颗LED），全亮度会导致发热明显，并可能造成过流或损伤硬件。  
> **推荐亮度为 10% 左右**（如 `BRIGHTNESS = 0.1` 或更低，示例代码默认 5%），既保证观感，同时避免过热和能耗过高。


### 创意场景

- **AI 语音天气屏：** 语音唤醒查询天气，像素屏显示图标和文字
- **桌面像素时钟：** 网络同步时间，支持亮度调节，夜间低亮模式
- **开源像素画板：** 手机APP实时绘制，支持AI生成32x32像素风格图片

### 资源链接

- **文档官网：** https://tuyaopen.ai/zh
- **Github 软件代码仓库：** https://github.com/tuya/TuyaOpen

---

本文件夹包含多个演示应用程序，展示了Tuya T5AI Pixel开发板上32x32 LED像素矩阵显示屏的功能。所有演示程序都使用在 `boards/T5AI/TUYA_T5AI_PIXEL/board_pixel_api.h` 中实现的板级支持包（BSP）API进行像素显示操作。

## 概述

这些演示程序展示了使用1024像素（32x32）LED矩阵的各种视觉效果、动画和交互功能。BSP提供了统一的API，用于：
- 像素设备初始化和管理
- HSV到RGB颜色转换
- 矩阵坐标到LED索引映射
- 基于帧的绘制操作
- 多字体文本渲染
- 位图和GIF动画支持

## 构建项目

> **建议**：详细环境搭建、编译与烧录步骤，请结合TuyaOpen官方文档：  
> - [环境搭建](https://tuyaopen.ai/docs/quick-start/enviroment-setup)  
> - [项目编译](https://tuyaopen.ai/docs/quick-start/project-compilation)  
> - [固件烧录](https://tuyaopen.ai/docs/quick-start/firmware-burning)

### 前置要求

- 已完成TuyaOpen SDK环境配置（详见上方“环境搭建”）
- 终端支持 bash shell
- 目标开发板：**TUYA_T5AI_PIXEL**

### 快速构建和烧录流程

1. **进入TuyaOpen SDK根目录，并导出环境变量**  
   ```bash
   cd /path/to/TuyaOpen
   . ./export.sh
   ```

2. **进入对应项目目录**  
   ```bash
   cd apps/tuya_t5_pixel
   ```

3. **项目配置**（首次构建时，选择硬件平台 TUYA_T5AI_PIXEL）  
   ```bash
   tos.py config
   # 交互选择 TUYA_T5AI_PIXEL 开发板
   ```

4. **编译项目**  
   ```bash
   tos.py build
   ```

5. **烧录固件到设备**  
   > 烧录建议配合 USB/UART 方式，详见官方[烧录教程](https://tuyaopen.ai/docs/quick-start/firmware-burning)
   ```bash
   # 典型命令（以USB方式为例，根据实际COM口和参数调整）
   tos.py flash
   ```
   或者使用官方TuyaOpen烧录工具，具体参数参见上方文档。

---

常用构建演示程序（在步骤3后直接执行编译)：
- 构建 `tuya_t5_pixel_demo`            → 默认演示
- 构建 `tuya_t5_pixel_sphere_effect`   → 3D球体动画
- 构建 `tuya_t5_pixel_mic_spectrum_meter` → 麦克风频谱
- 构建 `tuya_t5_pixel_simple_shapes`   → 简单图元渲染
- 构建 `tuya_t5_pixel_bongocat_kb`     → 键盘事件驱动的像素（键盘猫+积功德）动画
- 构建 `tuya_t5_pixel_weather`         → 天气信息显示（需配置UUID和AUTHKEY）

如需构建其他目标或指定 demo，可在 `apps/tuya_t5_pixel/` 下调整相应配置。

## 可用演示程序

# Demo 1. tuya_t5_pixel_demo

**描述：** 一个综合演示程序，包含多种动画效果、像素艺术动画和交互控制。

**功能：**
- **动画效果：**
  - 彩虹色滚动文字
  - 呼吸颜色效果（红、绿、蓝循环）
  - 水波纹效果动画
  - 彩色渐变2D波浪效果
  - 旋转雪花图案
  - 扫描动画（列和行扫描）
  - 呼吸圆圈效果
  - 跑马灯效果
  - 彩色波浪效果
- **像素艺术动画：** 10个预加载的像素艺术动画，包括猫咪、马里奥角色和各种表情包
- **沙粒物理演示：** 由加速度计（BMI270传感器）控制的交互式粒子物理模拟
- **交互控制：**
  - OK按钮：循环切换动画，切换循环模式
  - A按钮：切换像素艺术动画
  - B按钮：播放蜂鸣器旋律，激活沙粒物理演示
- **蜂鸣器集成：** 播放启动旋律和"小星星"乐曲

**使用的硬件：**
- 32x32 LED像素矩阵（1024像素）
- 3个按钮（OK、A、B）
- 蜂鸣器
- BMI270加速度计/陀螺仪传感器

# Demo 2. tuya_t5_pixel_sphere_effect

**描述：** 3D旋转球体可视化，具有音频响应式旋转速度。

**功能：**
- **3D球体渲染：** 将3D球体投影到2D LED矩阵上，具有正确的透视效果
- **旋转动画：** 围绕多个轴平滑旋转
- **音频响应：** 旋转速度响应音频输入（麦克风）
- **颜色渐变：** 球体表面的UV映射颜色渐变
- **交互控制：** 按钮控制旋转速度和模式切换

**使用的硬件：**
- 32x32 LED像素矩阵
- 麦克风输入
- 控制按钮

# Demo 3. tuya_t5_pixel_mic_spectrum_meter

**描述：** 实时音频频谱分析仪，在LED矩阵上显示频段。

**功能：**
- **FFT分析：** 对16kHz音频输入执行快速傅里叶变换
- **8频段频谱显示：** 可视化8个频段，每个频段4像素宽
- **实时可视化：** 以高帧率更新，实现平滑的频谱显示
- **颜色编码：** 不同频段使用不同颜色
- **音频处理：** 处理16位PCM音频，使用环形缓冲区管理

**使用的硬件：**
- 32x32 LED像素矩阵
- 麦克风输入（16kHz单声道音频）

# Demo 4. tuya_t5_pixel_simple_shapes

**描述：** 使用BSP帧API的基本绘制图元的简单演示。

**功能：**
- **绘制图元：**
  - 各种颜色的填充矩形
  - 圆形（填充和轮廓）
  - 线条
  - 使用不同字体的文本渲染
- **基于帧的渲染：** 使用BSP帧API进行高效绘制
- **调色板：** 展示所有32种预定义颜色
- **字体展示：** 使用不同字体大小显示文本

**使用的硬件：**
- 32x32 LED像素矩阵

# Demo 5. tuya_t5_pixel_bongocat

**描述：** 键盘事件驱动的（键盘猫+功德木鱼）像素艺术动画演示，支持通过串口接收键盘按键事件，实时控制Bongo Cat和木块动画。

**功能：**
- **双模式动画：**
  - **Bongo Cat模式：** 显示3帧可爱猫咪动画
    - 按键UP (0x00)：随机显示帧1或帧3
    - 按键DOWN (0x01)：显示帧2（双手举起）
  - **木块模式：** 显示5帧木块动画（32x25像素，底部对齐）
    - 按键UP (0x00)：显示帧0（向上位置）
    - 按键DOWN (0x01)：显示帧4（向下位置，第5帧）
- **串口通信：** 通过UART0接收单字节命令（115200波特率）
  - 0x00：按键UP事件
  - 0x01：按键DOWN事件
  - 所有接收到的数据会以十六进制格式回显（用于调试）
- **模式切换：** 使用OK按钮在Bongo Cat和木块模式之间切换

**PC端键盘事件工具：**
- **位置：** `apps/tuya_t5_pixel/tuya_t5_pixel_bongocat_kb/pc_kb_event/`
- **功能：** Python脚本监听PC键盘按键事件，并通过串口发送到开发板
- **使用方法：**
  1. 安装依赖：
     ```bash
     cd apps/tuya_t5_pixel/tuya_t5_pixel_bongocat_kb/pc_kb_event
     pip install -r requirements.txt
     ```
  2. 配置串口：
     - 编辑 `key_event.py`，修改 `SERIAL_PORT` 变量为您的串口设备名
     - Windows: `'COM14'` 等
     - Linux: `'/dev/ttyUSB0'` 等
     - macOS: `'/dev/cu.usbserial-*'` 等
  3. 运行脚本：
     ```bash
     python key_event.py
     ```
  4. 按任意键即可看到按键事件发送到开发板
  5. 按 Ctrl+C 退出程序
  - **注意事项：**
    - 脚本会占用串口，烧录固件前需先关闭脚本
    - 选择Tuya T5的**烧录串口**（而非调试串口）
    - 脚本发送单字节命令：0x00（按键UP）和0x01（按键DOWN）

**使用的硬件：**
- 32x32 LED像素矩阵（1024像素）
- OK按钮（模式切换）
- UART0串口（115200波特率）

**串口命令格式：**
- 单字节命令，无需起始字节
- `0xA0 0x00`：按键UP事件
- `0xA0 0x01`：按键DOWN事件
- 所有接收数据会以十六进制格式回显：`RX: 0xXX 0xYY ...`

# Demo 6. tuya_t5_pixel_weather

**描述：** 天气信息显示应用，从Tuya云服务获取天气数据并在像素屏上显示。

**功能：**
- **天气信息显示：**
  - 当前温度及体感温度
  - 今日最高/最低温度
  - 湿度百分比
  - 风速和风向
  - 空气质量指数（AQI）
  - 时间和日期（支持时区）
- **天气图标：** 16x16像素天气图标显示在屏幕右上角
- **页面循环：** 自动循环显示不同天气信息页面

**配置要求：**
> ⚠️ **重要：** 使用此演示前，必须配置Tuya IoT平台授权码
> 
> 1. **编辑配置文件：** `apps/tuya_t5_pixel/tuya_t5_pixel_weather/include/tuya_config.h`
> 
> 2. **配置以下参数：**
>    - `TUYA_OPENSDK_UUID`: SDK认证所需的设备UUID
>    - `TUYA_OPENSDK_AUTHKEY`: 安全通信所需的认证密钥
> 
> 3. **获取凭证：**
>    - 产品创建和PID生成：https://developer.tuya.com/cn/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc
>    - 授权码获取：https://tuyaopen.ai/docs/quick-start#get-tuyaopen-license
> 
> 4. **配置设备位置：**
   - 使用Tuya App绑定设备
   - 在App中设置设备所在位置（城市/地区）
   - 位置信息用于获取准确的天气数据
> 
> **警告：** 未配置正确的UUID和AUTHKEY将导致设备无法连接到Tuya云服务器，天气数据将无法获取。


## TUYA_T5AI_PIXEL 板级支持包（BSP）

所有演示程序都使用在以下位置实现的BSP API：
- **位置：** `boards/T5AI/TUYA_T5AI_PIXEL/`
- **头文件：** `board_pixel_api.h`
- **实现：** `board_pixel_api.c`

### 主要使用的BSP函数：

- `board_pixel_get_handle()` - 初始化并获取像素设备句柄
- `board_pixel_matrix_coord_to_led_index()` - 将2D (x,y)坐标转换为LED索引
- `board_pixel_hsv_to_pixel_color()` - 将HSV颜色空间转换为RGB像素颜色
- `board_pixel_frame_*()` - 基于帧的绘制操作（在simple_shapes演示中使用）

如需更多BSP音频、蜂鸣器和像素矩阵API示例，请参考：
- `boards/T5AI/TUYA_T5AI_PIXEL/board_buzzer_api.h`
- `boards/T5AI/TUYA_T5AI_PIXEL/board_audio_api.h`
- `apps/tuya_t5_pixel/tuya_t5_pixel_demo/src/tuya_main.c` 部分演示实现



## 故障排除

### 常见问题

1. **找不到像素设备：**
   - 确保 `PIXEL_DEVICE_NAME` 配置正确
   - 验证SPI和像素LED驱动程序已启用
   - 检查硬件连接

2. **音频不工作：**
   - 验证 `AUDIO_CODEC_NAME` 配置
   - 检查麦克风连接
   - 确保音频编解码器已正确初始化

3. **显示问题：**
   - 验证像素数量匹配（1024）
   - 检查亮度设置
   - 确保电源供应正常





## 参考资料

- **BSP文档：** 参见 `boards/T5AI/TUYA_T5AI_PIXEL/board_pixel_api.h` 获取完整API文档
- **硬件文档：** 参考Tuya T5AI Pixel板文档
- **SDK文档：** TuyaOpen SDK文档

## 许可证

版权所有 (c) 2021-2025 Tuya Inc. 保留所有权利。

