#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__

#include "tuya_cloud_types.h"

typedef uint8_t USER_SERVO_ACTION_E;
typedef enum {
    SERVO_UP = 0,
    SERVO_DOWN = 1,
    SERVO_LEFT = 2,
    SERVO_RIGHT = 3,
    SERVO_CENTER = 4,
    SERVO_NOD = 5,
    SERVO_CLOCKWISE = 6,
    SERVO_ANTICLOCKWISE = 7,
    SERVO_MAX
} SERVO_ACTION_E;

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_servo_init(VOID);
VOID app_servo_move(SERVO_ACTION_E action);
VOID app_servo_cleanup(VOID);

#ifdef __cplusplus
}
#endif
#endif // __APP_SERVO_H__