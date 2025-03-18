#ifndef _TDL_BUTTON_DRIVER_H_
#define _TDL_BUTTON_DRIVER_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *DEVICE_BUTTON_HANDLE;
typedef void (*TDL_BUTTON_CB)(void *arg);

typedef enum {
    BUTTON_TIMER_SCAN_MODE = 0,
    BUTTON_IRQ_MODE,
} TDL_BUTTON_MODE_E;

typedef struct {
    DEVICE_BUTTON_HANDLE dev_handle; // tdd handle
    TDL_BUTTON_CB irq_cb;            // irq cb
} TDL_BUTTON_OPRT_INFO;

typedef struct {
    OPERATE_RET (*button_create)(TDL_BUTTON_OPRT_INFO *dev);
    OPERATE_RET (*button_delete)(TDL_BUTTON_OPRT_INFO *dev);
    OPERATE_RET (*read_value)(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value);
} TDL_BUTTON_CTRL_INFO;

typedef struct {
    void *dev_handle;
    TDL_BUTTON_MODE_E mode;
} TDL_BUTTON_DEVICE_INFO_T;

// 按键软件配置
OPERATE_RET tdl_button_register(char *name, TDL_BUTTON_CTRL_INFO *button_ctrl_info,
                                TDL_BUTTON_DEVICE_INFO_T *button_cfg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_BUTTON_H_*/