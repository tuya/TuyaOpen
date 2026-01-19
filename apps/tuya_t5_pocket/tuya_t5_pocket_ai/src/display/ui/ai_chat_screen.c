/**
 * @file ai_pet_chat_screen.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"

#include "toast_screen.h"
#include "rfid_scan_screen.h"
#include "ai_log_screen.h"
#include "main_screen.h"

#include "app_display.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char *emoji_name;
    char *toast_show_str;
} CHAT_EMOJI_SHOW_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static const CHAT_EMOJI_SHOW_T cCHAT_EMOJI_SHOW_LIST[] = {
    {EMOJI_HAPPY,   "PET: Happy"},
    {EMOJI_ANGRY,   "PET: Angry"},
    {EMOJI_FEARFUL, "PET: Fearful"},
    {EMOJI_SAD,     "PET: Sad"},
}; 

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_set_emotion(char *emotion)
{
    char *show_string = NULL;

    for (int i = 0; i < CNTSOF(cCHAT_EMOJI_SHOW_LIST); i++) {
        if (strcmp(emotion, cCHAT_EMOJI_SHOW_LIST[i].emoji_name) == 0) {
            show_string = cCHAT_EMOJI_SHOW_LIST[i].toast_show_str;
            lv_vendor_disp_lock();
            toast_screen_show(show_string, 1000);
            lv_vendor_disp_unlock();
            break;
        }
    }
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    lv_vendor_disp_lock();

    switch(wifi_status) {
        case AI_UI_WIFI_STATUS_DISCONNECTED:
            main_screen_set_wifi_state(0);
            break;
        case AI_UI_WIFI_STATUS_GOOD:
            main_screen_set_wifi_state(3);
            break;
        case AI_UI_WIFI_STATUS_FAIR:
            main_screen_set_wifi_state(4);
            break;
        case AI_UI_WIFI_STATUS_WEAK:
            main_screen_set_wifi_state(1);
            break;
        default:
            break;
    }

    lv_vendor_disp_unlock();
}

void __ui_set_custom_msg(uint32_t type, uint8_t *data, int len )
{
    lv_vendor_disp_lock();

    switch(type) {
        case POCKET_DISP_TP_RFID_SCAN_SUCCESS:
            rfid_scan_screen_load();
            break;
        case POCKET_DISP_TP_AI_LOG:
            PR_DEBUG("AI LOG: %d", len);
            if (data && len > 0) {
                ai_log_screen_update_log((const char *)data, len);
            }
            break;
        default:
            break;
    }

    lv_vendor_disp_unlock();
}

OPERATE_RET ai_ui_chat_register(void)
{
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_emotion     = __ui_set_emotion;
    intfs.disp_wifi_state  = __ui_set_network;
    intfs.disp_other_msg   = __ui_set_custom_msg;

    return ai_ui_register(&intfs);
}
