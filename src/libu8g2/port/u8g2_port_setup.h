/**
 * @file u8g2_port_tdl_disp.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __U8G2_PORT_TDL_DISP_H__
#define __U8G2_PORT_TDL_DISP_H__

#include "tuya_cloud_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET u8g2_Setup_tdl_display_f(u8g2_t *u8g2, void *disp_name);

#ifdef __cplusplus
}
#endif

#endif /* __U8G2_PORT_TDL_DISP_H__ */
