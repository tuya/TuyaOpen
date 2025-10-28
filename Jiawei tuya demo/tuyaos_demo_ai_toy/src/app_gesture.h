#ifndef __APP_GESTURE_H__
#define __APP_GESTURE_H__

#include "tuya_cloud_types.h"

#define GUSTURE_SCL_PIN    TUYA_GPIO_NUM_20
#define GUSTURE_SDA_PIN    TUYA_GPIO_NUM_29
#define GUSTURE_INT_PIN    TUYA_GPIO_NUM_27


// 手势类型枚举
typedef enum {
    GESTURE_NONE = 0,
    GESTURE_RIGHT,
    GESTURE_LEFT,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_FORWARD,
    GESTURE_BACKWARD,
    GESTURE_CLOCKWISE,
    GESTURE_ANTICLOCKWISE,
    GESTURE_WAVE
} GESTURE_TYPE_E;

typedef VOID (*GESTURE_CB_T)(GESTURE_TYPE_E gesture);

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_gesture_init(GESTURE_CB_T cb);

#ifdef __cplusplus
}
#endif
#endif // __APP_GESTURE_H__
