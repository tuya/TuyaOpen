#ifndef _TDL_JOYSTICK_DRIVER_H_
#define _TDL_JOYSTICK_DRIVER_H_

#include "tuya_cloud_types.h"
// #include "tdl_button_driver.h"
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
    TDL_JOYSTICK_DEV_HANDLE dev_handle; // tdd handle
    TDL_JOYSTICK_CB irq_cb;            // irq cb
} TDL_JOYSTICK_OPRT_INFO;

typedef struct {
    OPERATE_RET (*joystick_create)(TDL_JOYSTICK_OPRT_INFO *dev);
    OPERATE_RET (*joystick_delete)(TDL_JOYSTICK_OPRT_INFO *dev);
    OPERATE_RET (*read_value)(TDL_JOYSTICK_OPRT_INFO *dev, uint8_t *value);
} TDL_JOYSTICK_CTRL_INFO;

typedef struct {
    void *dev_handle;
    TDL_JOYSTICK_MODE_E mode;
} TDL_JOYSTICK_DEVICE_INFO_T;

// 按键软件配置
OPERATE_RET tdl_joystick_register(char *name, TDL_JOYSTICK_CTRL_INFO *joystick_ctrl_info,
                                TDL_JOYSTICK_DEVICE_INFO_T *joystick_cfg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_BUTTON_H_*/