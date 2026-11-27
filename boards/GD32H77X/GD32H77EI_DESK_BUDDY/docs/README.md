# 硬件资料与 IO 映射

本目录用于存放 `GD32H77EI_DESK_BUDDY` 的原理图、GPIO 分配表和 PSRAM 资料。

已整理入口：

- [引脚核对](PIN_BRINGUP.md)
- [APS6408L 八线 PSRAM 适配](PSRAM_PORTING.md)

建议文件：

- `GD32H77EI_DESK_BUDDY_SCH_<版本>.pdf`
- `GD32H77EI_DESK_BUDDY_GPIO_<版本>.xlsx`
- `<PSRAM型号>*.pdf`

导入后需要形成一份可执行的引脚核对表，至少包含：

| 分类 | 需要确认的字段 |
| --- | --- |
| 电源/启动 | 电源保持、VCORE、BOOT、复位、调试口 |
| 调试串口 | UART 实例、TX/RX、AF、电平转换器 |
| PSRAM | 器件型号、容量、接口线宽、CS/SCK/DQS/IO0~IO7、电压、时钟 |
| 显示/触摸 | QSPI 数据线、CS/RST/BL/TE、I2C、中断、复位 |
| 音频 | UART 实例、TX/RX、流控、复位、时钟 |
| 摄像头 | DVP 数据/同步/时钟、I2C、复位、PWDN |
| 无线模块 | 传输方式、SPI/UART 引脚、复位、BOOT strap |
| 人机交互 | 按键、编码器、IMU、RGB/LED |
