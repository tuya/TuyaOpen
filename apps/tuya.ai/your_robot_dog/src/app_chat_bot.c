#include "tal_api.h"

#include "netmgr.h"
#include "tkl_fs.h"

#include "ai_chat_main.h"
#include "app_chat_bot.h"

#if defined(ENABLE_DOG_ACTION) && (ENABLE_DOG_ACTION == 1)
#include "servo_ctrl/servo_ctrl.h"
#include "servo_ctrl/tuya_robot_actions.h"
#endif

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif

extern OPERATE_RET ai_ui_robot_dog_register(void);

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = tkl_fs_mount("/", DEV_INNER_FLASH);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
    }

    /* Register robot-dog UI bridge only when AI display is enabled. */
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_robot_dog_register());
#endif

#if defined(ENABLE_DOG_ACTION) && (ENABLE_DOG_ACTION == 1)
    TUYA_CALL_ERR_RETURN(robot_action_thread_init());
    TUYA_CALL_ERR_RETURN(robot_action_add_action(ROBOT_ACTION_STAND));
#endif

    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_FREE,
        .default_vol = 70,
    };

    TUYA_CALL_ERR_RETURN(ai_chat_init(&ai_chat_cfg));

    return OPRT_OK;
}
