# 给下一位智能体的提示词

请先完整阅读同目录的 `SPI_AT_BRINGUP_HANDOFF_20261117.md`，然后继续排查 Desk Buddy H779↔VW553/TY1 SPI AT 启动握手问题。

现状：临时 GPIO 软件 SPI 固件已在 H779 烧录并回读校验成功，TY1 沿用现有 256 字节 CS_ONLY v2 从机固件。用户确认接线为 H779 PG4→TY1 PA4 (MOSI)、H779 PB14←TY1 PA5 (MISO)、PC3→PA2 (SCK)、PC0→PA3 (CS)。实际日志确认 56 帧中 47 有效、9 无效，且 H779 收到有效 SPI header；因此先查 `READY`/reset 事件和无效帧计数的精确来源，别再从“完全不通”开始猜。

先静态阅读 `at_module_reset.c/.h`、`at_session.c`、`at_spi_transport.c`、`at_spi_protocol.c`，给出基于代码的事件时序和坏帧分类依据。不要立刻改代码或重烧：先用现有日志证据确定最小根因；若证据不足，准备无 DTR/RTS 切换、实时 flush、有字节计数的串口监听器，再让用户手动复位采集。日志只输出状态信息，不泄露 UUID、auth key 或 Wi-Fi 密码。

保持临时方案隔离：GPIO 软件 SPI 只在 Desk Buddy 配置打开、Kconfig 默认关闭、无自动 fallback；硬件 SPI DMA+IRQ 路径和 START 配置不受影响。任何修复需做最小相关测试与编译，解释验证结果；未经用户下一步确认，不要再烧录。
