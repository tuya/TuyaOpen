#ifndef __TUYA_ROBOT_ACTIONS_H__
#define __TUYA_ROBOT_ACTIONS_H__

#include "tuya_cloud_types.h"

typedef enum {

    ROBOT_ACTION_NONE = 0,
    ROBOT_ACTION_FORWARD,       // Forward
    ROBOT_ACTION_BACKWARD,      // Backward
    ROBOT_ACTION_LEFT,          // Turn left
    ROBOT_ACTION_RIGHT,         // Turn right
    ROBOT_ACTION_SPIN,          // Spin
    ROBOT_ACTION_DANCE,         // Dance
    ROBOT_ACTION_HANDSHAKE,     // Handshake
    ROBOT_ACTION_JUMP,          // Jump
    ROBOT_ACTION_STAND,         // Stand
    ROBOT_ACTION_SIT,           // Sit
    ROBOT_ACTION_GETDOWN,       // Get down
    ROBOT_ACTION_STRETCH,       // Stretch
    ROBOT_ACTION_DRAGONBOAT,    // Rowing
    ROBOT_ACTION_MAX,
} TUYA_ROBOT_ACTION_E;


OPERATE_RET tuya_robot_action_init(void);
OPERATE_RET tuya_robot_action_set(TUYA_ROBOT_ACTION_E action);
 
#endif // __TUYA_ROBOT_ACTIONS_H__ 