#ifndef __PWM_LED_CTRL_H__
#define __PWM_LED_CTRL_H__

#ifdef __cplusplus
extern "C" {
#endif      

#include "tuya_cloud_types.h"

#define BRIGHT_VALUE_DP_ID      22  //亮度
#define SWITCH_DP_ID            20  //开关


int app_set_led_onoff(bool led_state);

int app_set_led_brightness(INT_T value);

OPERATE_RET app_led_contral_task(VOID);


#ifdef __cplusplus
}
#endif

#endif