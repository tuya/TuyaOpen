# PC3 / PC3_C 的 SPI 路由限制

依据官方《GD32H77x_78x User Manual》Rev1.0（2026 年 11 月），
同目录 `GD32H77x_78x_User_Manual_Rev1.0.pdf`：

- 印刷页 98，1.8.1 节 `SYSCFG_PMCFG0`，bit 27 `PC3SWON`。
- 印刷页 549–550，13.3.12 节及图 13-6。

`PC3SWON=0` 时，PC3_C 可使用数字输入输出，PC3 只能使用模拟功能；
`PC3SWON=1` 时，PC3 可使用数字输入输出，PC3_C 只能使用模拟功能。
因此 PC3 与 PC3_C 不能同时承担两个独立的 SPI 数字信号。

新版 Desk Buddy 若将 SPI1 SCK 接 PC3、MOSI 接 PC3_C，则无法通过 AF
配置使两者同时工作。需调整其中一个信号的 PCB 路由，例如将 MOSI 改为
SPI1 的 PB15（AF5）或 PC1（AF5），并检查是否与其他功能冲突。
当前 START 验证用 PB13/PB14/PB15，不受此问题影响。

PC2 与 PC2_C 也有同类限制，由 bit 26 `PC2SWON` 控制。

来源：https://www.gd32mcu.com/data/documents/userManual/GD32H77x_78x_User_Manual_Rev1.0.pdf
