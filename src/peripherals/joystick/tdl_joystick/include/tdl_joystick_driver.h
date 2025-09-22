/**
 * @file tdl_joystick_driver.h
 * @brief Joystick driver module
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @date 2025-07-08     maidang      Initial version
 */

#ifndef _TDL_JOYSTICK_DRIVER_H_
#define _TDL_JOYSTICK_DRIVER_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* TDL_JOYSTICK_DEV_HANDLE;
typedef void (*TDL_JOYSTICK_CB)(void *arg);

typedef enum {
    JOYSTICK_TIMER_SCAN_MODE = 0,
    JOYSTICK_IRQ_MODE,
} TDL_JOYSTICK_MODE_E;

typedef struct {
    TDL_JOYSTICK_DEV_HANDLE dev_handle;     /* joystick device handle */
    TDL_JOYSTICK_CB irq_cb;                 /* joystick irq callback */
} TDL_JOYSTICK_OPRT_INFO;

typedef struct {
    OPERATE_RET (*joystick_create)(TDL_JOYSTICK_OPRT_INFO *dev);            /* create joystick */
    OPERATE_RET (*joystick_delete)(TDL_JOYSTICK_OPRT_INFO *dev);            /* delete joystick */
    OPERATE_RET (*read_value)(TDL_JOYSTICK_OPRT_INFO *dev, uint8_t *value); /* read joystick value */
} TDL_JOYSTICK_CTRL_INFO;

typedef struct {
    void *dev_handle;                        /* joystick device handle */
    TDL_JOYSTICK_MODE_E mode;                /* joystick mode */
    TUYA_ADC_NUM_E      adc_num;             /* adc num */
    uint8_t             adc_ch_x;        
    uint8_t             adc_ch_y;
} TDL_JOYSTICK_DEVICE_INFO_T;

/**
 * @brief Register joystick driver
 * 
 * @param name joystick name
 * @param joystick_ctrl_info joystick control info
 * @param joystick_cfg_info joystick config info
 * @return OPERATE_RET 
 */
OPERATE_RET tdl_joystick_register(char *name, TDL_JOYSTICK_CTRL_INFO *joystick_ctrl_info,
                                TDL_JOYSTICK_DEVICE_INFO_T *joystick_cfg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_BUTTON_H_*/