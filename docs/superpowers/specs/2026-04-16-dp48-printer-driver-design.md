# DP48 打印机驱动设计文档

**日期**: 2026-04-16  
**分支**: dev_picture  
**作者**: zhouss@tuya.com

---

## 1. 背景与目标

将 `apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/src/dp48a_printer.c` 中的 DP-48A 热敏打印机功能集成到标准的 TDD/TDL 打印机驱动体系，使其可被其他应用复用。

**目标**：
- 在 `src/peripherals/printer/tdd_printer/` 下新增 DP48 TDD 驱动
- 在 TDL 层扩展文字编码转换和位图打印能力
- 驱动自包含，应用层无需关心 UART 细节和编码格式

---

## 2. 架构总览

```
应用层
  │  tdl_printer_send_text(handle, utf8_str)
  │  tdl_printer_send_bitmap(handle, x, width, height, data)
  │  tdl_printer_send(handle, raw_bytes, len)
  ▼
TDL 层 (tdl_printer_manage.c)
  │  查 dev_info.encoding：GBK → UTF-8 转 GBK；UTF-8 → 直通
  │  查 dev_info.protocol：ESCPOS → 生成 GS v 0 命令；RAW → 直发
  │  utf8_to_gbk.c 放在 tdl_printer/src/（TDL 私有）
  ▼
TDD 层 (tdd_printer_dp48.c)
  │  UART open / write / close
  │  dev_info 声明 protocol=ESCPOS, encoding=GBK
  ▼
DP-48A 打印机硬件（UART, 9600/19200 baud）
```

---

## 3. TDD_PRINTER_DEV_INFO_T 扩展

在 `tdl_printer_driver.h` 的 `TDD_PRINTER_DEV_INFO_T` 中新增两个字段：

```c
typedef enum {
    TDD_PRINTER_ENCODING_NONE = 0,  // 原始模式（MTP02、通用 UART）
    TDD_PRINTER_ENCODING_UTF8 = 1,  // 驱动原生接受 UTF-8
    TDD_PRINTER_ENCODING_GBK  = 2,  // 驱动需要 GBK（DP48A）
} TDD_PRINTER_ENCODING_E;

typedef enum {
    TDD_PRINTER_PROTOCOL_RAW    = 0, // 原始点阵（MTP02）
    TDD_PRINTER_PROTOCOL_ESCPOS = 1, // ESC/POS UART（DP48A）
} TDD_PRINTER_PROTOCOL_E;

typedef struct {
    uint32_t dots_per_line;
    uint32_t bytes_per_line;
    uint32_t head_blocks;
    uint32_t dots_per_block;
    uint32_t steps_per_dot_line;
    TDD_PRINTER_ENCODING_E encoding;   // 新增：文字编码
    TDD_PRINTER_PROTOCOL_E protocol;   // 新增：通信协议
} TDD_PRINTER_DEV_INFO_T;
```

**已有驱动兼容性**：`encoding` 和 `protocol` 默认值均为 0（NONE/RAW），MTP02 和 UART 驱动无需改动。

---

## 4. TDL 层新增接口

### 4.1 文字打印

```c
OPERATE_RET tdl_printer_send_text(TDL_PRINTER_HANDLE handle, const char *text);
```

**行为**：
- `protocol == RAW`：返回 `OPRT_NOT_SUPPORTED`（RAW 驱动不含字体渲染）
- `encoding == GBK`：`utf8_to_gbk_buf(text)` → `intfs.write(gbk_bytes)`
- `encoding == UTF8` 或 `NONE`：`intfs.write(text)` 直通

**UTF-8→GBK 转换**：  
`utf8_to_gbk.c/h` 迁移自 app 层，放在 `tdl_printer/src/`（TDL 内部私有，不对外暴露头文件）。

---

### 4.2 位图打印

```c
OPERATE_RET tdl_printer_send_bitmap(TDL_PRINTER_HANDLE handle,
                                    uint16_t x,
                                    uint16_t width,
                                    uint16_t height,
                                    const uint8_t *data);
```

**参数**：
- `x`：起始像素位置（0 = 靠左）
- `width`：图片宽度（像素）
- `height`：图片高度（像素）
- `data`：1-bit 单色位图，行优先，MSB 在前，每行 `(width+7)/8` 字节

**裁剪规则（静默）**：

```
if x >= printer_width → return OK（整图在区域外，不打印）
eff_width = min(width, printer_width - x)
```

**ESCPOS 协议实现**：

```
① 若 x > 0：发送 ESC $ (xL, xH) 设置水平起始位置
② 计算 eff_bytes = (eff_width + 7) / 8
③ 发送 GS v 0 命令头：
   {0x1D, 0x76, 0x30, 0x00, eff_bytes & 0xFF, eff_bytes >> 8, yL, yH}
④ 逐行发送 eff_bytes 字节（来自源数据每行前 eff_bytes 字节）
   若 eff_width % 8 != 0：末字节 &= (0xFF << (8 - eff_width % 8))
```

**RAW 协议实现**：

```
① 分配 dst_row[printer_bytes_per_line]，每行复用并 memset 0
② 逐行处理：
   若 x % 8 == 0（字节对齐）：
     memcpy(dst_row + x/8, src_row, eff_bytes)
     mask 末字节多余 bit
   若 x % 8 != 0（非对齐）：
     shift = x % 8
     逐字节移位写入：
       dst[x/8 + j]     |= src[j] >> shift
       if (x/8 + j + 1 < printer_bytes):          // 防止末字节越界
           dst[x/8 + j + 1] |= src[j] << (8 - shift)
     清除超出 printer_width 的尾部 bit
③ write(dst_row, printer_bytes_per_line)
```

---

## 5. TDD DP48 驱动

### 5.1 头文件 `tdd_printer_dp48.h`

```c
typedef struct {
    TUYA_UART_NUM_E port_id;
    TAL_UART_CFG_T  uart_cfg;
} TDD_PRINTER_DP48_CFG_T;

OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg);
```

### 5.2 实现 `tdd_printer_dp48.c`

实现的 TDD 接口：

| 接口 | 实现 |
|------|------|
| `open` | `tal_uart_init(port_id, uart_cfg)` |
| `write` | `tal_uart_write(port_id, data, len)` |
| `close` | `tal_uart_deinit(port_id)` |
| `start` | NULL（ESC/POS 无需预处理） |
| `end` | NULL（切纸由应用通过 `send()` 发 ESC/POS 命令完成） |
| `paper_feed` | NULL（由应用发 `ESC d n` 命令） |
| `get_status` | NULL（DP48A 状态查询需 UART 接收，暂不实现） |

注册时声明：
```c
TDD_PRINTER_DEV_INFO_T dev_info = {
    .encoding = TDD_PRINTER_ENCODING_GBK,
    .protocol = TDD_PRINTER_PROTOCOL_ESCPOS,
};
```

---

## 6. utf8_to_gbk 迁移

| 原位置 | 新位置 |
|--------|--------|
| `apps/.../src/expand/src/utf8_to_gbk.c` | `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.c` |
| `apps/.../src/expand/inc/utf8_to_gbk.h` | `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.h` |
| `apps/.../src/expand/inc/u2g_tbl.inc` | `src/peripherals/printer/tdl_printer/src/u2g_tbl.inc` |

头文件不对外暴露（放 src/ 而非 include/），仅 `tdl_printer_manage.c` 包含。

---

## 7. 构建系统

### CMakeLists.txt

```cmake
# utf8_to_gbk 跟随 ENABLE_PRINTER 编译（任意打印机均可用）
if (CONFIG_ENABLE_PRINTER STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdl_printer/src/utf8_to_gbk.c)
endif()

# DP48 驱动
if (CONFIG_ENABLE_PRINTER_DP48 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_printer/src/tdd_printer_dp48.c)
endif()
```

### Kconfig

```kconfig
config ENABLE_PRINTER_DP48
    bool "enable DP48A ESC/POS UART printer driver"
    default n
    depends on ENABLE_PRINTER
```

---

## 8. 文件变更汇总

| 文件 | 操作 |
|------|------|
| `tdl_printer_driver.h` | 改：新增 `TDD_PRINTER_ENCODING_E`、`TDD_PRINTER_PROTOCOL_E`、两字段 |
| `tdl_printer_manage.h` | 改：新增 `tdl_printer_send_text()`、`tdl_printer_send_bitmap()` |
| `tdl_printer_manage.c` | 改：实现两个新接口 |
| `tdl_printer/src/utf8_to_gbk.c` | 新（迁移） |
| `tdl_printer/src/utf8_to_gbk.h` | 新（迁移） |
| `tdl_printer/src/u2g_tbl.inc` | 新（迁移） |
| `tdd_printer/include/tdd_printer_dp48.h` | 新 |
| `tdd_printer/src/tdd_printer_dp48.c` | 新 |
| `CMakeLists.txt` | 改：加 dp48、utf8_to_gbk 编译项 |
| `Kconfig` | 改：加 `ENABLE_PRINTER_DP48` |

---

## 9. 驱动说明文档

见 `docs/printer_driver_guide.md`。
