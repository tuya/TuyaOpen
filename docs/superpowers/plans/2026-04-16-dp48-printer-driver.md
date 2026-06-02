# DP48 打印机驱动 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 TDD/TDL 打印机体系中新增 DP-48A 热敏打印机驱动，TDL 层支持 UTF-8 文字和位图打印。

**Architecture:** TDD 层（tdd_printer_dp48）负责 UART 传输，声明 protocol=ESCPOS、encoding=GBK；TDL 层新增 `tdl_printer_send_text()`（处理 UTF-8→GBK 转换）和 `tdl_printer_send_bitmap()`（生成 ESC/POS GS v 0 命令并处理 x 偏移/裁剪）；utf8_to_gbk 迁移至 tdl_printer/src/ 作为 TDL 私有库。

**Tech Stack:** C99, TuyaOS TAL/TKL APIs, ESC/POS 打印协议

---

## 文件结构

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/peripherals/printer/tdl_printer/include/tdl_printer_driver.h` | 修改 | 新增 `TDD_PRINTER_ENCODING_E`、`TDD_PRINTER_PROTOCOL_E` 及两字段 |
| `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.h` | 新建（迁移） | UTF-8→GBK 私有头文件 |
| `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.c` | 新建（迁移） | UTF-8→GBK 转换实现 |
| `src/peripherals/printer/tdl_printer/src/u2g_tbl.inc` | 新建（迁移） | Unicode→GBK 查找表 |
| `src/peripherals/printer/tdl_printer/include/tdl_printer_manage.h` | 修改 | 新增 `tdl_printer_send_text()`、`tdl_printer_send_bitmap()` 声明 |
| `src/peripherals/printer/tdl_printer/src/tdl_printer_manage.c` | 修改 | 实现两个新接口 |
| `src/peripherals/printer/tdd_printer/include/tdd_printer_dp48.h` | 新建 | DP48 TDD 驱动头文件 |
| `src/peripherals/printer/tdd_printer/src/tdd_printer_dp48.c` | 新建 | DP48 TDD 驱动实现 |
| `src/peripherals/printer/CMakeLists.txt` | 修改 | 新增 DP48 编译项 |
| `src/peripherals/printer/Kconfig` | 修改 | 新增 `ENABLE_PRINTER_DP48` |

---

## Task 1: 扩展 TDD_PRINTER_DEV_INFO_T

**Files:**
- Modify: `src/peripherals/printer/tdl_printer/include/tdl_printer_driver.h`

- [ ] **Step 1: 在 `tdl_printer_driver.h` 中新增两个枚举和两个字段**

将文件第 36 行附近的 `TDD_PRINTER_DEV_INFO_T` 替换为：

```c
typedef enum {
    TDD_PRINTER_ENCODING_NONE = 0,  /* raw mode (MTP02, generic UART) */
    TDD_PRINTER_ENCODING_UTF8 = 1,  /* driver accepts UTF-8 natively */
    TDD_PRINTER_ENCODING_GBK  = 2,  /* driver requires GBK (DP48A) */
} TDD_PRINTER_ENCODING_E;

typedef enum {
    TDD_PRINTER_PROTOCOL_RAW    = 0, /* raw dot bitmap (MTP02) */
    TDD_PRINTER_PROTOCOL_ESCPOS = 1, /* ESC/POS UART (DP48A) */
} TDD_PRINTER_PROTOCOL_E;

typedef struct {
    uint32_t dots_per_line;
    uint32_t bytes_per_line;
    uint32_t head_blocks;
    uint32_t dots_per_block;
    uint32_t steps_per_dot_line;
    TDD_PRINTER_ENCODING_E encoding;
    TDD_PRINTER_PROTOCOL_E protocol;
} TDD_PRINTER_DEV_INFO_T;
```

- [ ] **Step 2: 验证已有驱动无需改动**

检查 `tdd_printer_uart.c` 和 `tdd_printer_mtp02.c` 中的 `TDD_PRINTER_DEV_INFO_T` 初始化语句。两者均使用 `{0}` 或字段初始化，新增字段默认为 0（`ENCODING_NONE` / `PROTOCOL_RAW`），无需改动。

- [ ] **Step 3: 编译验证**

```bash
cd /home/share/samba/tmp/TuyaOpen
# 以任意已有 config 构建，确认无编译错误
# 关键：确认 tdd_printer_mtp02.c 和 tdd_printer_uart.c 编译无警告
```

- [ ] **Step 4: Commit**

```bash
git add src/peripherals/printer/tdl_printer/include/tdl_printer_driver.h
git commit -m "feat(printer): add encoding and protocol fields to TDD_PRINTER_DEV_INFO_T"
```

---

## Task 2: 迁移 utf8_to_gbk 至 TDL 层

**Files:**
- Create: `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.h`
- Create: `src/peripherals/printer/tdl_printer/src/utf8_to_gbk.c`
- Create: `src/peripherals/printer/tdl_printer/src/u2g_tbl.inc`

- [ ] **Step 1: 复制三个文件**

```bash
cp apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/inc/utf8_to_gbk.h \
   src/peripherals/printer/tdl_printer/src/utf8_to_gbk.h

cp apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/src/utf8_to_gbk.c \
   src/peripherals/printer/tdl_printer/src/utf8_to_gbk.c

cp apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/inc/u2g_tbl.inc \
   src/peripherals/printer/tdl_printer/src/u2g_tbl.inc
```

- [ ] **Step 2: 确认 utf8_to_gbk.c 中的 include 路径正确**

`utf8_to_gbk.c` 第 8 行是 `#include "utf8_to_gbk.h"`，第 13 行是 `#include "u2g_tbl.inc"`。  
两者迁移后在同一目录，路径无需修改。

- [ ] **Step 3: 确认 CMakeLists.txt 的 glob 会自动包含新文件**

查看 `src/peripherals/printer/CMakeLists.txt` 第 14 行：
```cmake
file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/tdl_printer/src/*.c")
```
`utf8_to_gbk.c` 放在 `tdl_printer/src/` 下，已被 glob 自动收录，**无需修改 CMakeLists.txt**。

- [ ] **Step 4: Commit**

```bash
git add src/peripherals/printer/tdl_printer/src/utf8_to_gbk.h \
        src/peripherals/printer/tdl_printer/src/utf8_to_gbk.c \
        src/peripherals/printer/tdl_printer/src/u2g_tbl.inc
git commit -m "feat(printer/tdl): migrate utf8_to_gbk to TDL layer as private utility"
```

---

## Task 3: 实现 tdl_printer_send_text()

**Files:**
- Modify: `src/peripherals/printer/tdl_printer/include/tdl_printer_manage.h`
- Modify: `src/peripherals/printer/tdl_printer/src/tdl_printer_manage.c`

- [ ] **Step 1: 在 `tdl_printer_manage.h` 中新增声明**

在 `tdl_printer_paper_feed` 声明后面加：

```c
/**
 * @brief Send UTF-8 text to the printer with automatic encoding conversion
 * @param[in] handle printer handle
 * @param[in] text UTF-8 encoded text string
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if driver protocol is RAW
 * @note For GBK drivers (e.g. DP48A), text is converted from UTF-8 to GBK
 *       internally. For UTF-8 or NONE drivers, text is sent as-is.
 */
OPERATE_RET tdl_printer_send_text(TDL_PRINTER_HANDLE handle, const char *text);
```

- [ ] **Step 2: 在 `tdl_printer_manage.c` 中添加 include 和实现**

在文件顶部 `#include "tdl_printer_manage.h"` 之后添加：

```c
#include "utf8_to_gbk.h"
```

在文件末尾（`tdl_printer_driver_register` 之后）添加：

```c
/**
 * @brief Send UTF-8 text to the printer with automatic encoding conversion
 */
OPERATE_RET tdl_printer_send_text(TDL_PRINTER_HANDLE handle, const char *text)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(text, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer not opened, status: %d, name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.write, OPRT_INVALID_PARM);

    /* RAW protocol has no font renderer */
    if (node->dev_info.protocol == TDD_PRINTER_PROTOCOL_RAW) {
        return OPRT_NOT_SUPPORTED;
    }

    size_t text_len = strlen(text);
    if (text_len == 0) {
        return OPRT_OK;
    }

    if (node->dev_info.encoding == TDD_PRINTER_ENCODING_GBK) {
        /* GBK output is always <= UTF-8 input in bytes */
        uint8_t *gbk_buf = (uint8_t *)tal_malloc(text_len);
        if (gbk_buf == NULL) {
            return OPRT_MALLOC_FAILED;
        }

        int gbk_len = utf8_to_gbk_buf((const uint8_t *)text, text_len,
                                       gbk_buf, text_len);
        if (gbk_len < 0) {
            PR_ERR("UTF-8 to GBK conversion failed: %d", gbk_len);
            tal_free(gbk_buf);
            return OPRT_COM_ERROR;
        }

        OPERATE_RET rt = node->intfs.write(node->tdd_handle, gbk_buf, (uint32_t)gbk_len);
        tal_free(gbk_buf);
        return rt;
    }

    /* UTF-8 or NONE: pass through */
    return node->intfs.write(node->tdd_handle, (const uint8_t *)text, (uint32_t)text_len);
}
```

- [ ] **Step 3: 编译验证**

确保 `utf8_to_gbk.h` 可以被 `tdl_printer_manage.c` 找到（两者都在 `tdl_printer/src/` 或通过相对路径可达）。若编译报找不到头文件，在 CMakeLists.txt 的 `LIB_PRIVATE_INC` 中加上 `${MODULE_PATH}/tdl_printer/src`。

- [ ] **Step 4: Commit**

```bash
git add src/peripherals/printer/tdl_printer/include/tdl_printer_manage.h \
        src/peripherals/printer/tdl_printer/src/tdl_printer_manage.c
git commit -m "feat(printer/tdl): add tdl_printer_send_text with UTF-8 to GBK conversion"
```

---

## Task 4: 实现 tdl_printer_send_bitmap()

**Files:**
- Modify: `src/peripherals/printer/tdl_printer/include/tdl_printer_manage.h`
- Modify: `src/peripherals/printer/tdl_printer/src/tdl_printer_manage.c`

- [ ] **Step 1: 在 `tdl_printer_manage.h` 中新增声明**

在 `tdl_printer_send_text` 声明后面加：

```c
/**
 * @brief Send a monochrome bitmap to the printer
 * @param[in] handle printer handle
 * @param[in] x      horizontal start position in dots (0 = left edge)
 * @param[in] width  image width in pixels
 * @param[in] height image height in pixels
 * @param[in] data   1-bit monochrome bitmap, row-major, MSB first,
 *                   (width+7)/8 bytes per row
 * @return OPRT_OK on success
 * @note Images extending beyond the right edge are silently clipped.
 *       If x >= printer_width, the call succeeds without printing anything.
 *       For RAW protocol, dots_per_line must be non-zero.
 */
OPERATE_RET tdl_printer_send_bitmap(TDL_PRINTER_HANDLE handle,
                                    uint16_t x, uint16_t width,
                                    uint16_t height, const uint8_t *data);
```

- [ ] **Step 2: 在 `tdl_printer_manage.c` 末尾添加实现**

```c
/**
 * @brief Copy eff_width bits from src (starting at bit 0) into dst (starting
 *        at bit x), zero-padding the rest of dst. dst must be zero-filled
 *        before calling.
 */
static void __bitmap_place_row(uint8_t *dst, uint16_t dst_bytes,
                                const uint8_t *src,
                                uint16_t x, uint16_t eff_width)
{
    if (x % 8 == 0) {
        /* Byte-aligned fast path */
        uint16_t eff_bytes = (eff_width + 7) / 8;
        memcpy(dst + x / 8, src, eff_bytes);
        if (eff_width % 8 != 0) {
            dst[x / 8 + eff_bytes - 1] &= (uint8_t)(0xFF << (8 - (eff_width % 8)));
        }
    } else {
        /* Non-aligned: bit-by-bit copy */
        for (uint16_t i = 0; i < eff_width; i++) {
            uint8_t bit = (src[i / 8] >> (7 - (i % 8))) & 1;
            if (bit) {
                uint16_t dst_pos = x + i;
                dst[dst_pos / 8] |= (uint8_t)(1 << (7 - (dst_pos % 8)));
            }
        }
    }
    (void)dst_bytes;
}

static OPERATE_RET __send_bitmap_escpos(TDL_PRINTER_NODE_T *node,
                                         uint16_t x, uint16_t eff_width,
                                         uint16_t src_bytes_per_row,
                                         uint16_t height,
                                         const uint8_t *data)
{
    /* Row width includes left padding (x bits) */
    uint16_t total_width = x + eff_width;
    uint16_t total_bytes = (total_width + 7) / 8;

    /* GS v 0 command */
    uint8_t cmd[] = {
        0x1D, 0x76, 0x30, 0x00,
        (uint8_t)(total_bytes & 0xFF), (uint8_t)(total_bytes >> 8),
        (uint8_t)(height & 0xFF),      (uint8_t)(height >> 8)
    };
    TUYA_CALL_ERR_RETURN(node->intfs.write(node->tdd_handle, cmd, sizeof(cmd)));

    uint8_t *row_buf = (uint8_t *)tal_malloc(total_bytes);
    if (row_buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    for (uint16_t row = 0; row < height; row++) {
        const uint8_t *src_row = data + (uint32_t)row * src_bytes_per_row;
        memset(row_buf, 0, total_bytes);
        __bitmap_place_row(row_buf, total_bytes, src_row, x, eff_width);
        OPERATE_RET rt = node->intfs.write(node->tdd_handle, row_buf, total_bytes);
        if (rt != OPRT_OK) {
            tal_free(row_buf);
            return rt;
        }
    }

    tal_free(row_buf);
    return OPRT_OK;
}

static OPERATE_RET __send_bitmap_raw(TDL_PRINTER_NODE_T *node,
                                      uint16_t x, uint16_t eff_width,
                                      uint16_t src_bytes_per_row,
                                      uint16_t height,
                                      const uint8_t *data)
{
    uint16_t printer_bytes = (uint16_t)((node->dev_info.dots_per_line + 7) / 8);

    uint8_t *row_buf = (uint8_t *)tal_malloc(printer_bytes);
    if (row_buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    for (uint16_t row = 0; row < height; row++) {
        const uint8_t *src_row = data + (uint32_t)row * src_bytes_per_row;
        memset(row_buf, 0, printer_bytes);
        __bitmap_place_row(row_buf, printer_bytes, src_row, x, eff_width);
        OPERATE_RET rt = node->intfs.write(node->tdd_handle, row_buf, printer_bytes);
        if (rt != OPRT_OK) {
            tal_free(row_buf);
            return rt;
        }
    }

    tal_free(row_buf);
    return OPRT_OK;
}

/**
 * @brief Send a monochrome bitmap to the printer
 */
OPERATE_RET tdl_printer_send_bitmap(TDL_PRINTER_HANDLE handle,
                                    uint16_t x, uint16_t width,
                                    uint16_t height, const uint8_t *data)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer not opened, status: %d, name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.write, OPRT_INVALID_PARM);

    uint16_t printer_width = (uint16_t)node->dev_info.dots_per_line;

    /* RAW protocol with no dots info can't do bitmap */
    if (node->dev_info.protocol == TDD_PRINTER_PROTOCOL_RAW && printer_width == 0) {
        return OPRT_NOT_SUPPORTED;
    }

    /* x entirely outside print area */
    if (printer_width > 0 && x >= printer_width) {
        return OPRT_OK;
    }

    if (width == 0 || height == 0) {
        return OPRT_OK;
    }

    /* Clip right edge */
    uint16_t eff_width = width;
    if (printer_width > 0 && (uint32_t)x + width > printer_width) {
        eff_width = printer_width - x;
    }

    uint16_t src_bytes_per_row = (width + 7) / 8;

    if (node->dev_info.protocol == TDD_PRINTER_PROTOCOL_ESCPOS) {
        return __send_bitmap_escpos(node, x, eff_width, src_bytes_per_row, height, data);
    } else {
        return __send_bitmap_raw(node, x, eff_width, src_bytes_per_row, height, data);
    }
}
```

- [ ] **Step 3: 编译验证**

确认无编译错误和警告（特别是 `uint16_t` 和 `uint32_t` 的混合运算）。

- [ ] **Step 4: Commit**

```bash
git add src/peripherals/printer/tdl_printer/include/tdl_printer_manage.h \
        src/peripherals/printer/tdl_printer/src/tdl_printer_manage.c
git commit -m "feat(printer/tdl): add tdl_printer_send_bitmap with ESC/POS and RAW support"
```

---

## Task 5: 新建 tdd_printer_dp48.h

**Files:**
- Create: `src/peripherals/printer/tdd_printer/include/tdd_printer_dp48.h`

- [ ] **Step 1: 创建头文件**

```c
/**
 * @file tdd_printer_dp48.h
 * @brief DP-48A ESC/POS thermal printer TDD driver
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * Drives the DP-48A (and compatible) thermal printer via UART using
 * the ESC/POS protocol. The printer accepts GBK-encoded text.
 * Text encoding conversion (UTF-8 → GBK) is handled by the TDL layer.
 */

#ifndef __TDD_PRINTER_DP48_H__
#define __TDD_PRINTER_DP48_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tdl_printer_driver.h"
#include "tal_uart.h"

/* DP-48A hardware constants */
#define DP48_DOTS_PER_LINE   384
#define DP48_BYTES_PER_LINE  48

typedef struct {
    TUYA_UART_NUM_E port_id;
    TAL_UART_CFG_T  uart_cfg;
} TDD_PRINTER_DP48_CFG_T;

/**
 * @brief Register DP-48A printer driver to TDL layer
 * @param[in] name printer name for TDL registration
 * @param[in] cfg  UART port and configuration
 * @return OPRT_OK on success, error code on failure
 * @note After registration, use tdl_printer_find() to get the handle.
 *       The driver declares protocol=ESCPOS and encoding=GBK.
 *       Use tdl_printer_send_text() for UTF-8 text (auto-converted to GBK).
 *       Use tdl_printer_send() for raw ESC/POS command bytes.
 *       Use tdl_printer_send_bitmap() for monochrome images.
 */
OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PRINTER_DP48_H__ */
```

- [ ] **Step 2: Commit**

```bash
git add src/peripherals/printer/tdd_printer/include/tdd_printer_dp48.h
git commit -m "feat(printer/tdd): add tdd_printer_dp48 header"
```

---

## Task 6: 新建 tdd_printer_dp48.c

**Files:**
- Create: `src/peripherals/printer/tdd_printer/src/tdd_printer_dp48.c`

- [ ] **Step 1: 创建实现文件**

```c
/**
 * @file tdd_printer_dp48.c
 * @brief DP-48A ESC/POS thermal printer TDD driver implementation
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_printer_dp48.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_uart.h"

#include <string.h>

typedef struct {
    TDD_PRINTER_DP48_CFG_T cfg;
} TDD_PRINTER_DP48_HANDLE_T;

static OPERATE_RET __tdd_printer_dp48_open(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    OPERATE_RET rt = tal_uart_init(hdl->cfg.port_id, &hdl->cfg.uart_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("DP48 UART init failed: %d", rt);
    }
    return rt;
}

static OPERATE_RET __tdd_printer_dp48_write(TDD_PRINTER_HANDLE_T handle,
                                             const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    int ret = tal_uart_write(hdl->cfg.port_id, data, len);
    if (ret < 0) {
        PR_ERR("DP48 UART write error: %d", ret);
        return (OPERATE_RET)ret;
    }
    if ((uint32_t)ret != len) {
        PR_WARN("DP48 UART partial write, expect:%u real:%d", len, ret);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

static OPERATE_RET __tdd_printer_dp48_close(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    return tal_uart_deinit(hdl->cfg.port_id);
}

OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg)
{
    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl =
        (TDD_PRINTER_DP48_HANDLE_T *)tal_malloc(sizeof(TDD_PRINTER_DP48_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_PRINTER_DP48_HANDLE_T));
    memcpy(&hdl->cfg, cfg, sizeof(TDD_PRINTER_DP48_CFG_T));

    TDD_PRINTER_INTFS_T dp48_intfs = {
        .open  = __tdd_printer_dp48_open,
        .write = __tdd_printer_dp48_write,
        .close = __tdd_printer_dp48_close,
        /* start, end, paper_feed, get_status: NULL (ESC/POS commands via send()) */
    };

    TDD_PRINTER_DEV_INFO_T dev_info = {
        .dots_per_line  = DP48_DOTS_PER_LINE,
        .bytes_per_line = DP48_BYTES_PER_LINE,
        .encoding       = TDD_PRINTER_ENCODING_GBK,
        .protocol       = TDD_PRINTER_PROTOCOL_ESCPOS,
    };

    OPERATE_RET rt = tdl_printer_driver_register(name, &dp48_intfs, &dev_info,
                                                  (TDD_PRINTER_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        PR_ERR("Failed to register DP48 printer: %d", rt);
    }

    return rt;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/peripherals/printer/tdd_printer/src/tdd_printer_dp48.c
git commit -m "feat(printer/tdd): add tdd_printer_dp48 UART ESC/POS driver"
```

---

## Task 7: 更新构建系统

**Files:**
- Modify: `src/peripherals/printer/CMakeLists.txt`
- Modify: `src/peripherals/printer/Kconfig`

- [ ] **Step 1: 修改 `CMakeLists.txt`**

在 `CONFIG_ENABLE_PRINTER_MTP02` 块之后，`LIB_PUBLIC_INC` 之前添加：

```cmake
if (CONFIG_ENABLE_PRINTER_DP48 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_printer/src/tdd_printer_dp48.c)
endif()
```

同时确认 `LIB_PUBLIC_INC` 包含 dp48 头文件所在目录（已有 `${MODULE_PATH}/tdd_printer/include`，无需额外添加）。

若编译时出现 `utf8_to_gbk.h` 找不到的错误，在 `LIB_PRIVATE_INC` 中添加：

```cmake
set(LIB_PRIVATE_INC
    ${MODULE_PATH}/tdl_printer/src
)
```

完整 CMakeLists.txt 修改后应如下（关键部分）：

```cmake
if (CONFIG_ENABLE_PRINTER STREQUAL "y")
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

file(GLOB_RECURSE LIB_SRCS
    "${MODULE_PATH}/tdl_printer/src/*.c")

if (CONFIG_ENABLE_PRINTER_UART STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_printer/src/tdd_printer_uart.c)
endif()

if (CONFIG_ENABLE_PRINTER_MTP02 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_printer/src/tdd_printer_mtp02.c)
endif()

if (CONFIG_ENABLE_PRINTER_DP48 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_printer/src/tdd_printer_dp48.c)
endif()

set(LIB_PRIVATE_INC
    ${MODULE_PATH}/tdl_printer/src
)

set(LIB_PUBLIC_INC
    ${MODULE_PATH}/tdl_printer/include
    ${MODULE_PATH}/tdd_printer/include
)
```

- [ ] **Step 2: 修改 `Kconfig`**

在 `ENABLE_PRINTER_MTP02` 相关块之后添加：

```kconfig
config ENABLE_PRINTER_DP48
    bool "enable DP48A ESC/POS UART printer driver"
    default n
    depends on ENABLE_PRINTER
```

- [ ] **Step 3: 编译验证**

启用 `CONFIG_ENABLE_PRINTER_DP48=y` 进行构建，确认：
1. `tdd_printer_dp48.c` 编译无报错
2. `utf8_to_gbk.c` 和 `tdl_printer_manage.c` 编译无报错
3. `tdd_printer_dp48.h` 可被应用层正常 include

- [ ] **Step 4: Commit**

```bash
git add src/peripherals/printer/CMakeLists.txt \
        src/peripherals/printer/Kconfig
git commit -m "build(printer): add ENABLE_PRINTER_DP48 config and utf8_to_gbk private include"
```

---

## Self-Review

**Spec coverage:**

| 设计要求 | 对应 Task |
|---------|----------|
| TDD_PRINTER_DEV_INFO_T 新增 encoding/protocol | Task 1 |
| utf8_to_gbk 迁移至 tdl_printer/src/ | Task 2 |
| tdl_printer_send_text() UTF-8→GBK | Task 3 |
| tdl_printer_send_bitmap() ESC/POS 路径 | Task 4 |
| tdl_printer_send_bitmap() RAW 路径 | Task 4 |
| 位图静默裁剪 | Task 4 (`eff_width = min(width, printer_width - x)`) |
| x offset 支持 | Task 4 (`__bitmap_place_row` 的 x 参数) |
| 非字节对齐 bit-copy 防越界 | Task 4 (bit-by-bit loop，天然不越界) |
| tdd_printer_dp48.h | Task 5 |
| tdd_printer_dp48.c open/write/close | Task 6 |
| dev_info 声明 GBK + ESCPOS | Task 6 |
| CMakeLists.txt + Kconfig | Task 7 |
| LIB_PRIVATE_INC 让 tdl_manage 找到 utf8_to_gbk.h | Task 7 |

**已知限制（不在本计划范围）：**
- `get_status` 未实现（需 UART RX 支持）
- `paper_feed`/`start`/`end` 未实现（通过 `tdl_printer_send()` 发 ESC/POS 命令替代）
- 应用层负责发送 ESC/POS 初始化命令（`ESC @`）和切纸命令
