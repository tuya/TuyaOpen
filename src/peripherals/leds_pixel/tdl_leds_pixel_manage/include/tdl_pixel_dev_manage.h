/**
* @file tdl_pixel_dev_manage.h
* @author www.tuya.com
* @brief tdl_light_data module is used to manage leds pixel device
* @version 0.1
* @date 2022-03-01
*
* @copyright Copyright (c) tuya.inc 2022
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
#define PIXEL_MAX_NUM                 200
#define PIXEL_DEV_NAME_MAX_LEN        20

/*********************************************************************
****************************variable define***************************
*********************************************************************/
typedef unsigned char PIXEL_SEND_STATE_E;
#define PIXEL_TX_START   0
#define PIXEL_TX_SUCC    1
#define PIXEL_TX_FAILED  2

typedef unsigned char PIXEL_DEV_CFG_CMD_E;
#define PIXEL_DEV_CMD_SET_PIXEL_NUM             0x00
#define PIXEL_DEV_CMD_SET_TX_CB                 0x01
#define PIXEL_DEV_CMD_GET_RESOLUTION            0x02
#define PIXEL_DEV_CMD_GET_PIXEL_NUM             0x03
#define PIXEL_DEV_CMD_GET_DRV_COLOR_CH          0x04
#define PIXEL_DEV_CMD_SET_WHITE_COLOR_CTRL      0x05
#define PIXEL_DEV_CMD_GET_WHITE_COLOR_CTRL      0x06
#define PIXEL_DEV_CMD_GET_PWM_HARDWARE_CFG      0x07
#define PIXEL_DEV_CMD_SET_RGB_LINE_SEQUENCE     0x08
typedef struct {
	unsigned int        pixel_num;  
    unsigned short      pixel_resolution;
}PIXEL_DEV_CONFIG_T;

typedef void* PIXEL_HANDLE_T;

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
* @brief      查找幻彩灯带设备
*
* @param[in]   name                 灯带名字
* @param[out]  handle               设备句柄
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_find(char *name, OUT PIXEL_HANDLE_T *handle);

/**
* @brief        启动设备
*
* @param[in]   handle               设备句柄
* @param[in]   config               配置参数
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_open(PIXEL_HANDLE_T handle, PIXEL_DEV_CONFIG_T* config);

/**
* @brief        刷新所有像素显存的数据到驱动端显示
*
* @param[in]    handle               设备句柄
* @param[in]    timeout_ms           任务超时时间
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_refresh(PIXEL_HANDLE_T handle);

/**
* @brief        配置设备参数
*
* @param[in]    handle               设备句柄
* @param[in]    cmd                  命令字
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_config(PIXEL_HANDLE_T handle, PIXEL_DEV_CFG_CMD_E cmd, void *arg);

/**
* @brief        停止设备
*
* @param[in]    handle               设备句柄
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
int tdl_pixel_dev_close(PIXEL_HANDLE_T handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_DEV_MANAGE_H__*/
