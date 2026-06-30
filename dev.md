# ESP32 平台 TKL SPI 接口适配与功能验证开发任务

## 一、任务目标
1. 基于 ESP-IDF v5.4 原生 SPI 主机驱动，在 ESP32 平台完成 TuyaOS `tkl_spi` 通用外设接口的适配实现，补全 ESP32 平台的 SPI 硬件抽象层能力。
2. 通过 SPI 屏幕驱动、SD 卡文件系统两个实际业务场景完成接口功能验收，验证 `tkl_spi` 接口的可用性、稳定性与跨平台兼容性。

## 二、参考资源
### 2.1 接口定义文件
- TKL SPI 接口头文件：`platform/T5AI/tuyaos/tuyaos_adapter/include/spi/include/tkl_spi.h`
- ESP32 平台适配代码根目录：`platform/ESP32/tuya_open_sdk/tuyaos_adapter`
- TKL 文件系统接口定义
  - 头文件：`platform/T5AI/tuyaos/tuyaos_adapter/include/system/tkl_fs.h`
  - 参考实现：`platform/T5AI/tuyaos/tuyaos_adapter/src/system/tkl_fs.c`

### 2.2 官方技术文档
- ESP-IDF v5.4 SPI 主机驱动官方手册：https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32/api-reference/peripherals/spi_master.html
- ESP-IDF v5.4 SD SPI 官方手册：https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.4/esp32/api-reference/peripherals/sdspi_host.html

### 2.3 参考代码与示例
- ST7789 屏幕驱动参考（含寄存器定义、管脚映射）：`boards/ESP32/common/lcd/tdd_disp_esp_st7789_spi.c`
- T5 平台 SD 卡操作参考示例：`examples/peripherals/sd`
- 目标板级文件：`boards/ESP32/WAVESHARE_ESP32S3_Touch_AMOLED_1.8/Waveshare_ESP32_S3_Touch_AMOLED_1_8.c`

## 三、核心开发任务：ESP32 平台 tkl_spi 接口适配
在 `platform/ESP32/tuya_open_sdk/tuyaos_adapter` 目录下完成 `tkl_spi` 全接口的落地实现，要求如下：
1. 严格遵循 `tkl_spi.h` 定义的接口规范与参数约定，基于 ESP-IDF `spi_master` 驱动封装全部标准 TKL SPI 接口。
2. 支持 SPI 主机模式下的设备注册、片选管理、时钟/极性/相位配置、全双工传输、半双工传输等基础能力。
3. 兼容 ESP32、ESP32-S3 等同系列芯片的 SPI 外设，适配 GPIO 矩阵与 IO_MUX 两种管脚路由逻辑。
4. 遵循 ESP-IDF SPI 驱动约束：处理 DMA 缓冲区对齐、最大传输长度限制、时序补偿等已知问题，保障传输稳定性。

## 四、验收验证任务
### 任务一：SPI 屏幕点亮与绘制功能验证
1. **修改对象**：`examples/peripherals/spi` 示例工程
2. **功能要求**
   - 基于适配完成的 `tkl_spi` 接口，驱动 ST7789 SPI 屏幕完成上电初始化、背光控制与屏幕点亮。
   - 实现基础绘制能力（如纯色填充、字符显示、图形绘制），验证 SPI 发送时序、数据传输的正确性。
3. **约束条件**
   - 屏幕寄存器配置、硬件管脚映射完全沿用 `tdd_disp_esp_st7789_spi.c` 中的定义，不得修改原有管脚分配。
   - 所有 SPI 通信操作必须通过 `tkl_spi` 抽象接口完成，禁止直接调用 ESP-IDF 原生 SPI 驱动函数。

### 任务二：SD 卡初始化与文件系统读写验证
#### 子任务 2.1 SD 卡 SPI 硬件初始化
1. **修改对象**：`boards/ESP32/WAVESHARE_ESP32S3_Touch_AMOLED_1.8/Waveshare_ESP32_S3_Touch_AMOLED_1_8.c`
2. **硬件管脚配置**
   - MOSI：GPIO1
   - SCK：GPIO2
   - MISO：GPIO3
   - SD 片选（SDCS）：EXIO7
3. **功能要求**：基于 `tkl_spi` 接口实现 SD 卡的 SPI 模式初始化流程，完成主机与 SD 卡的硬件链路通信验证。

#### 子任务 2.2 基于 tkl_fs 的 SD 卡读写实现
1. 梳理 `tkl_fs.h` 的接口定义与调用逻辑，明确 TKL 文件系统抽象层的设计规范与对接要求。
2. 结合 ESP32 平台特性与已适配的 `tkl_spi` 接口，打通 SD 卡 SPI 驱动与 FATFS 文件系统的链路，实现通过 `tkl_fs` 接口完成文件创建、读写、删除等基础操作。
3. 参考 `examples/peripherals/sd` 中 T5 平台的 SD 卡使用示例，保持跨平台接口调用逻辑的一致性。

## 五、验收标准
1. 全部代码可正常编译通过，无编译错误与关键警告，符合 TuyaOS 适配层的代码规范与目录结构要求。
2. 屏幕示例运行后，ST7789 屏幕正常点亮，绘制内容显示清晰稳定，无花屏、丢数据等 SPI 通信异常。
3. SD 卡功能验证：可正常识别 SD 卡，通过 `tkl_fs` 接口完成文件读写操作，写入与读取数据完全一致，无文件系统错误。
4. 所有 SPI 硬件操作均通过 `tkl_spi` 抽象接口完成，实现平台解耦，符合 TKL 外设抽象层的设计目标。

## 六、注意事项
1. 适配 `tkl_spi` 时需严格遵循 ESP-IDF SPI 驱动的硬件约束：DMA 缓冲区需 32 位对齐、半双工与 DMA 的兼容性限制、高速场景下的 dummy 位时序补偿等，参考官方文档规避已知问题。
2. SD 卡 SPI 模式需严格遵循 SD 协议的初始化时序与片选逻辑，确保与 `tkl_fs` 层的对接符合协议规范。
3. 保留原有工程的代码结构与文件组织方式，新增适配逻辑遵循现有目录规范与命名风格，不破坏已有功能。