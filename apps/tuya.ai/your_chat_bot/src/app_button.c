/**
 * @file app_button.c
 * @brief app_button module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_button.h"
#include "app_recorder.h"
#include "app_chat_bot.h"
#include "app_player.h"

#include "netmgr.h"

#include "tal_api.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define APP_BUTTON_NAME "app_button"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __app_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    APP_WORK_MODE_E work_mode = app_chat_bot_get_work_mode();
    PR_DEBUG("app button function cb, work mode: %d", work_mode);

    // network status
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (status == NETMGR_LINK_DOWN) {
        PR_DEBUG("network is down, ignore button event");
        if (app_player_is_playing()) {
            return;
        }
        app_player_play_alert(APP_ALERT_TYPE_NOT_ACTIVE);
        return;
    }

    PR_DEBUG("button event: %s, %d", name, event);

    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        if (work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
            PR_DEBUG("button press down, chat bot enable");
            app_chat_bot_enable_set(1);
            app_recorder_stat_post(VOICE_STATE_IN_START);
        }
    } break;
    case TDL_BUTTON_PRESS_UP: {
        if (work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
            PR_DEBUG("button press up, chat bot disable");
            app_recorder_stat_post(VOICE_STATE_IN_STOP);
            app_chat_bot_enable_set(0);
        }
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        if (work_mode == APP_CHAT_BOT_WORK_MODE_ONE_SHOT) {
            uint8_t is_enable = app_chat_bot_is_enable();
            is_enable = !is_enable;
            app_chat_bot_enable_set(is_enable);
            PR_DEBUG("button single click, chat bot %s", is_enable ? "enable" : "disable");
        }
    } break;
    default:
        break;
    }
}

OPERATE_RET app_button_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;

    if (pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = pin,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
        .level = active_level,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(APP_BUTTON_NAME, &button_hw_cfg));

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 50};
    TUYA_CALL_ERR_RETURN(tdl_button_create(APP_BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);

    return rt;
}
