#ifndef __TUYA_ROBOT_ACTIONS_H__
#define __TUYA_ROBOT_ACTIONS_H__

#include "tuya_cloud_types.h"

typedef enum {

    ROBOT_ACTION_NONE = 0,
    ROBOT_ACTION_FORWARD,       //前进
    ROBOT_ACTION_BACKWARD,      //后退
    ROBOT_ACTION_LEFT,          //左转
    ROBOT_ACTION_RIGHT,         //右转
    ROBOT_ACTION_SPIN,          //转圈
    ROBOT_ACTION_DANCE,         //跳舞   
    ROBOT_ACTION_HANDSHAKE,     //握手
    ROBOT_ACTION_JUMP,          //跳跃
    ROBOT_ACTION_STAND,         //站立
    ROBOT_ACTION_SIT,           //坐下
    ROBOT_ACTION_GETDOWN,       //趴下
    ROBOT_ACTION_STRETCH,       //伸懒腰
    ROBOT_ACTION_DRAGONBOAT,    //划船
    ROBOT_ACTION_MAX,
} TUYA_ROBOT_ACTION_E;


OPERATE_RET tuya_robot_action_init(void);
OPERATE_RET tuya_robot_action_set(TUYA_ROBOT_ACTION_E action);
 
#endif // __TUYA_ROBOT_ACTIONS_H__ 