/**
 * @file tdl_pixel_dev_manage.h
 * @brief TDL layer device management for LED pixel devices
 *
 * This header file provides the TDL (Tuya Device Layer) interface for LED pixel
 * device management and control. It includes functions for device discovery,
 * initialization, configuration, and control operations. The module provides
 * high-level abstractions for managing LED strip devices across different
 * pixel controller types and handles device lifecycle management.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_PIXEL_DEV_MANAGE_H__
#define __TDL_PIXEL_DEV_MANAGE_H__

#include <stdint.h>
#include "tuya_cloud_types.h"
#include "tdu_light_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
******************************macro define****************************
*********************************************************************/
#define PIXEL_MAX_NUM          200
#define PIXEL_DEV_NAME_MAX_LEN 20

/*********************************************************************
****************************variable define***************************
*********************************************************************/
typedef unsigned char PIXEL_SEND_STATE_E;
#define PIXEL_TX_START  0
#define PIXEL_TX_SUCC   1
#define PIXEL_TX_FAILED 2

typedef unsigned char PIXEL_DEV_CFG_CMD_E;
#define PIXEL_DEV_CMD_SET_PIXEL_NUM         0x00
#define PIXEL_DEV_CMD_SET_TX_CB             0x01
#define PIXEL_DEV_CMD_GET_RESOLUTION        0x02
#define PIXEL_DEV_CMD_GET_PIXEL_NUM         0x03
#define PIXEL_DEV_CMD_GET_DRV_COLOR_CH      0x04
#define PIXEL_DEV_CMD_SET_WHITE_COLOR_CTRL  0x05
#define PIXEL_DEV_CMD_GET_WHITE_COLOR_CTRL  0x06
#define PIXEL_DEV_CMD_GET_PWM_HARDWARE_CFG  0x07
#define PIXEL_DEV_CMD_SET_RGB_LINE_SEQUENCE 0x08
typedef struct {
    unsigned int pixel_num;
    unsigned short pixel_resolution;
} PIXEL_DEV_CONFIG_T;

typedef void *PIXEL_HANDLE_T;

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @brief       Find the LED strip device
 *
 * @param[in]   name                 LED strip name
 * @param[out]  handle               Device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_find(char *name, OUT PIXEL_HANDLE_T *handle);

/**
 * @brief        Start device
 *
 * @param[in]   handle               Device handle
 * @param[in]   config               Configuration parameters
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_open(PIXEL_HANDLE_T handle, PIXEL_DEV_CONFIG_T *config);

/**
 * @brief        Refresh the data of all pixel display memory to the driver for display
 *
 * @param[in]    handle           Device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_refresh(PIXEL_HANDLE_T handle);

/**
 * @brief        Configure device parameters
 *
 * @param[in]    handle               Device handle
 * @param[in]    cmd                  Command
 * @param[in]    arg                  Argument
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_config(PIXEL_HANDLE_T handle, PIXEL_DEV_CFG_CMD_E cmd, void *arg);

/**
 * @brief        Stop device
 *
 * @param[in]    handle               Device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_dev_close(PIXEL_HANDLE_T handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_DEV_MANAGE_H__*/
