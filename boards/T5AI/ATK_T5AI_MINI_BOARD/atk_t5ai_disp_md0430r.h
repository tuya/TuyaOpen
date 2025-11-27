/**
 * @file atk_t5ai_disp_md0430r.h
 * @brief atk_t5ai_disp_md0430r module is used to display rgb lcd md0430r.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __ATK_T5AI_DISP_MD0430R_H__
#define __ATK_T5AI_DISP_MD0430R_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t width;
    uint16_t height;
    TUYA_DISPLAY_ROTATION_E rotation;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_IO_CTRL_T      rst;
}ATK_T5AI_DISP_MD0430R_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET atk_t5ai_disp_rgb_md0430r_register(char *name, ATK_T5AI_DISP_MD0430R_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __ATK_T5AI_DISP_MD0430R_H__ */
