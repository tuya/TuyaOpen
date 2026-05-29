/**
 * @file tdl_printer_driver.h
 * @brief Printer driver interface for TDL layer
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_PRINTER_DRIVER_H__
#define __TDL_PRINTER_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDD_PRINTER_HANDLE_T;

typedef uint32_t TDD_PRINTER_STATUS_FLAG_T;
#define TDD_PRINTER_FLAG_PAPER_OUT    (1 << 0)
#define TDD_PRINTER_FLAG_OVERHEATED   (1 << 1)
#define TDD_PRINTER_FLAG_BUSY         (1 << 2)
#define TDD_PRINTER_FLAG_ERROR        (1 << 3)

typedef struct {
    TDD_PRINTER_STATUS_FLAG_T flags;
    int32_t temperature;
} TDD_PRINTER_DEV_STATUS_T;

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

typedef struct {
    OPERATE_RET (*open)(TDD_PRINTER_HANDLE_T handle);
    OPERATE_RET (*start)(TDD_PRINTER_HANDLE_T handle);
    OPERATE_RET (*write)(TDD_PRINTER_HANDLE_T handle, const uint8_t *data, uint32_t len);
    OPERATE_RET (*close)(TDD_PRINTER_HANDLE_T handle);
    OPERATE_RET (*get_status)(TDD_PRINTER_HANDLE_T handle, TDD_PRINTER_DEV_STATUS_T *status);
    OPERATE_RET (*paper_feed)(TDD_PRINTER_HANDLE_T handle, uint32_t lines);
    OPERATE_RET (*end)(TDD_PRINTER_HANDLE_T handle);
} TDD_PRINTER_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdl_printer_driver_register(char *name, TDD_PRINTER_INTFS_T *intfs,
                                        TDD_PRINTER_DEV_INFO_T *dev_info,
                                        TDD_PRINTER_HANDLE_T tdd_hdl);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_PRINTER_DRIVER_H__ */
