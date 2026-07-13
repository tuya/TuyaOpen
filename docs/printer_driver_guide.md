# 打印机驱动使用指南

本文档描述 TuyaOpen 打印机模块的架构、接口和使用方法。

---

## 1. 模块架构

打印机模块分三层：

```
应用层
  ├─ tdl_printer_send_text()     文字打印（编码自动转换）
  ├─ tdl_printer_send_bitmap()   图片打印（协议自动适配）
  ├─ tdl_printer_send()          原始字节发送
  ├─ tdl_printer_start() / end() 打印任务控制
  ├─ tdl_printer_paper_feed()    进纸
  └─ tdl_printer_open() / close() 设备管理

TDL 层（src/peripherals/printer/tdl_printer/）
  协议适配：ESC/POS 命令生成，UTF-8→GBK 转换，位图裁剪

TDD 层（src/peripherals/printer/tdd_printer/）
  硬件驱动：UART / GPIO / SPI / PWM 操作
```

---

## 2. 已支持驱动

| 驱动 | 文件 | 协议 | 编码 | 接口类型 |
|------|------|------|------|----------|
| DP-48A 热敏打印机 | `tdd_printer_dp48.c` | ESC/POS | GBK | UART |
| 通用 UART 打印机 | `tdd_printer_uart.c` | RAW | NONE | UART |
| MTP02-DXD 打印机构 | `tdd_printer_mtp02.c` | RAW | NONE | GPIO/SPI/PWM |

---

## 3. DP-48A 打印机驱动

### 3.1 硬件规格

| 参数 | 值 |
|------|----|
| 通信接口 | UART |
| 默认波特率 | 19200 bps |
| 打印宽度 | 384 dots（48mm） |
| 协议 | ESC/POS |
| 文字编码 | GBK（内部支持 GB18030） |
| 图片格式 | 1-bit 单色位图，`GS v 0` 命令 |

### 3.2 注册驱动

```c
#include "tdd_printer_dp48.h"
#include "tal_uart.h"

TDD_PRINTER_DP48_CFG_T dp48_cfg = {
    .port_id  = TUYA_UART_NUM_2,
    .uart_cfg = {
        .baudrate  = 19200,
        .databits  = TUYA_UART_DATA_LEN_8BIT,
        .stopbits  = TUYA_UART_STOP_LEN_1BIT,
        .parity    = TUYA_UART_PARITY_TYPE_NONE,
        .flowctrl  = TUYA_UART_FLOWCTRL_NONE,
    },
};

tdd_printer_dp48_register("dp48", &dp48_cfg);
```

### 3.3 Kconfig 配置

```
CONFIG_ENABLE_PRINTER=y
CONFIG_ENABLE_PRINTER_DP48=y
CONFIG_PRINTER_NAME="dp48"
```

---

## 4. 使用示例

### 4.1 初始化流程

```c
#include "tdl_printer_manage.h"

TDL_PRINTER_HANDLE printer;

// 1. 注册驱动（在系统初始化阶段）
tdd_printer_dp48_register("dp48", &dp48_cfg);

// 2. 查找打印机
tdl_printer_find("dp48", &printer);

// 3. 打开打印机（可选：注册事件回调）
TDL_PRINTER_OPEN_PARAM_T param = {
    .event_cb       = my_printer_event_cb,
    .event_cb_arg   = NULL,
    .poll_interval_ms = 500,
};
tdl_printer_open(printer, &param);
```

### 4.2 打印文字（UTF-8 输入）

```c
// 应用层始终传 UTF-8，TDL 自动转换为 GBK 发给 DP48A
tdl_printer_send_text(printer, "你好，世界！\r\n");
tdl_printer_send_text(printer, "Hello, TuyaOpen!\r\n");
```

### 4.3 发送 ESC/POS 命令

通过 `tdl_printer_send()` 发送原始 ESC/POS 命令字节：

```c
// 初始化打印机（ESC @）
uint8_t init_cmd[] = {0x1B, 0x40};
tdl_printer_send(printer, init_cmd, sizeof(init_cmd));

// 居中对齐（ESC a 1）
uint8_t center_cmd[] = {0x1B, 0x61, 0x01};
tdl_printer_send(printer, center_cmd, sizeof(center_cmd));

// 加粗开（ESC E 1）
uint8_t bold_on[] = {0x1B, 0x45, 0x01};
tdl_printer_send(printer, bold_on, sizeof(bold_on));

// 打印文字
tdl_printer_send_text(printer, "涂鸦智能\r\n");

// 加粗关
uint8_t bold_off[] = {0x1B, 0x45, 0x00};
tdl_printer_send(printer, bold_off, sizeof(bold_off));

// 进纸 3 行（ESC d 3）
uint8_t feed_cmd[] = {0x1B, 0x64, 0x03};
tdl_printer_send(printer, feed_cmd, sizeof(feed_cmd));

// 切纸（GS V 0）
uint8_t cut_cmd[] = {0x1D, 0x56, 0x00};
tdl_printer_send(printer, cut_cmd, sizeof(cut_cmd));
```

### 4.4 打印图片

```c
// 图片要求：1-bit 单色位图，行优先，MSB 在前
// 每行 (width + 7) / 8 字节

// 示例：8x8 笑脸
const uint8_t smiley[] = {
    0x3C, 0x42, 0xA5, 0x81,
    0xA5, 0x99, 0x42, 0x3C,
};

// 居中打印：x = (384 - 8) / 2 = 188
tdl_printer_send_bitmap(printer, 188, 8, 8, smiley);

// 靠左打印：x = 0
tdl_printer_send_bitmap(printer, 0, 8, 8, smiley);
```

**裁剪规则**：
- 若 `x >= 384`：静默不打印，返回 OK
- 若 `x + width > 384`：右侧超出部分静默裁剪
- 若 `width < 384`：仅打印图片宽度，右侧留白

### 4.5 打印任务控制（适用于 MTP02）

对于 UART 类打印机（DP48A），`start`/`end` 接口返回 `OPRT_NOT_SUPPORTED`，直接使用 `send`/`send_text`/`send_bitmap` 即可。

```c
// MTP02 流程（DP48A 不需要）
tdl_printer_start(printer);
tdl_printer_send(printer, dot_bitmap, len);
tdl_printer_end(printer);   // 自动进纸+停机
```

### 4.6 状态监控回调

```c
void my_printer_event_cb(TDL_PRINTER_HANDLE handle,
                         TDL_PRINTER_EVENT_E event,
                         void *data, void *arg)
{
    switch (event) {
    case TDL_PRINTER_EVENT_PAPER_OUT:
        PR_WARN("打印机缺纸");
        break;
    case TDL_PRINTER_EVENT_PAPER_IN:
        PR_NOTICE("纸张已装入");
        break;
    case TDL_PRINTER_EVENT_OVERHEATED:
        PR_WARN("打印头过热");
        break;
    default:
        break;
    }
}
```

> **注意**：DP48A 的 `get_status` 暂未实现（需要 UART 接收解析），状态回调对 DP48A 无效。

---

## 5. 接口速查

### TDL 层（`tdl_printer_manage.h`）

| 函数 | 说明 |
|------|------|
| `tdl_printer_find(name, &handle)` | 按名字查找已注册打印机 |
| `tdl_printer_open(handle, param)` | 打开打印机，可选状态监控 |
| `tdl_printer_close(handle)` | 关闭打印机，释放资源 |
| `tdl_printer_start(handle)` | 开始打印任务（MTP02 需要） |
| `tdl_printer_end(handle)` | 结束打印任务（MTP02 需要） |
| `tdl_printer_send(handle, data, len)` | 发送原始字节（ESC/POS 命令等） |
| `tdl_printer_send_text(handle, utf8)` | 发送 UTF-8 文字（TDL 自动处理编码） |
| `tdl_printer_send_bitmap(handle, x, w, h, data)` | 发送单色位图（TDL 自动生成协议命令） |
| `tdl_printer_paper_feed(handle, lines)` | 进纸 n 点行 |
| `tdl_printer_get_dev_info(handle, &info)` | 获取打印机参数 |

### TDD 注册（`tdd_printer_dp48.h`）

| 函数 | 说明 |
|------|------|
| `tdd_printer_dp48_register(name, cfg)` | 注册 DP48A 驱动 |

---

## 6. 常用 ESC/POS 命令参考

| 功能 | 命令字节 |
|------|---------|
| 初始化（复位） | `1B 40` |
| 左对齐 | `1B 61 00` |
| 居中对齐 | `1B 61 01` |
| 右对齐 | `1B 61 02` |
| 加粗开 / 关 | `1B 45 01` / `1B 45 00` |
| 下划线开 / 关 | `1B 2D 01` / `1B 2D 00` |
| 反白开 / 关 | `1D 42 01` / `1D 42 00` |
| 字号：双高 | `1D 21 01` |
| 字号：双宽 | `1D 21 10` |
| 字号：双高双宽 | `1D 21 11` |
| 字号：正常 | `1D 21 00` |
| 进纸 n 行 | `1B 64 n` |
| 进纸 n 点 | `1B 4A n` |
| 全切纸 | `1D 56 00` |
| 半切纸 | `1D 56 01` |
| 打印位图 | `1D 76 30 00 xL xH yL yH [data]` |
| 设置水平位置 | `1B 24 nL nH` |
| 查询打印机状态 | `10 04 01` |
| 查询缺纸状态 | `1D 72 01` |
