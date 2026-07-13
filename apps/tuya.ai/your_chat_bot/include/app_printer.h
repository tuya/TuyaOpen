/**
 * @file app_printer.h
 * @brief app_printer module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_PRINTER_H__
#define __APP_PRINTER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET app_print_jpeg_img(uint8_t *jpeg, uint32_t len);

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
OPERATE_RET app_print_img_from_album(const char *filename);
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* __APP_PRINTER_H__ */
