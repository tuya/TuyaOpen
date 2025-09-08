#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__

#include "tuya_cloud_types.h"

typedef uint8_t USER_SERVO_ACTION_E;
typedef enum {
    SERVO_UP = 0,
    SERVO_DOWN,
    SERVO_LEFT,
    SERVO_RIGHT,
    SERVO_CENTER,
    SERVO_NOD,
    SERVO_CLOCKWISE,
    SERVO_ANTICLOCKWISE,
    SERVO_MAX
} SERVO_ACTION_E;

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_servo_init(VOID);
VOID app_servo_move(SERVO_ACTION_E action);

#ifdef __cplusplus
}
#endif
#endif // __APP_SERVO_H__