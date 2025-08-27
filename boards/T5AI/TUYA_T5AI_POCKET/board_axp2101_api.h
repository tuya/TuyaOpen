/**
 * @file board_AXP2101_api.h
 * @author Tuya Inc.
 * @brief AXP2101 power management IC driver API for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_AXP2101_API_H__
#define __BOARD_AXP2101_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define RST_4G_MODULE_CTRL     TUYA_GPIO_NUM_25 /* high is valid work */
#define SIM_VDD_4G_MODULE_CTRL TUYA_GPIO_NUM_22 /* low is valid work*/

#define RTC_VDD        XPOWERS_LDO1
#define VDD_CAM_2V8    XPOWERS_ALDO3
#define VDD_SD_3V3     XPOWERS_ALDO4
#define AVDD_CAM_2V8   XPOWERS_BLDO1
#define DVDD_CAM_1V8   XPOWERS_BLDO2
#define VDD_JOYCON_1V1 XPOWERS_DLDO2
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief AXP2101 power management IC driver API for TUYA_T5AI_POCKET board
 *
 * @param void
 * @return void
 */
OPERATE_RET board_axp2101_init(void);

#ifdef __cplusplus
}
#endif

#endif
