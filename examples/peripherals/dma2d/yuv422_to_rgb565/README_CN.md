# DMA2D 硬件测试

本示例使用 `tal_dma2d` 验证 ESP32S31 KORVO 板上的 PPA 适配：

1. 将 YUV422（UYVY）转换为 RGB565；
2. 执行一次 YUV422 同格式 DMA memcpy；
3. 等待异步操作完成并校验输出。

串口日志同时出现 `YUV422 -> RGB565 passed`、`YUV422 memcpy passed` 和
`DMA2D hardware test PASSED`，表示 `tkl_dma2d` 适配和 `tal_dma2d` 链路正常。

构建并烧录：

```bash
cd examples/peripherals/dma2d/yuv422_to_rgb565
tos.py build
tos.py flash -p /dev/ttyUSB0
tos.py monitor -p /dev/ttyUSB0
```
