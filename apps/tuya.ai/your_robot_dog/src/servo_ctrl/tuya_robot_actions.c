
#include "tuya_cloud_types.h"
#include "tuya_robot_actions.h"
#include "servo_ctrl.h"
#include "tal_log.h"



OPERATE_RET tuya_robot_action_init(void)
{
    PR_DEBUG("[%s] enter", __func__);
    return robot_action_thread_init();
}

OPERATE_RET tuya_robot_action_set(TUYA_ROBOT_ACTION_E action)
{
    PR_DEBUG("[%s] enter", __func__);
    if (action < ROBOT_ACTION_NONE || action >= ROBOT_ACTION_MAX) {
        return OPRT_INVALID_PARM; // Invalid action
    }
    return robot_action_add_action(action);
}

