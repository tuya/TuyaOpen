/**
 * @file pocket_button.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdl_button_manage.h"
#include "ai_pocket_pet_app.h"
#include "lv_vendor.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char *name;
    TDL_BUTTON_TOUCH_EVENT_E event;
    uint32_t lv_key_code;
}BUTTON_CODE_MAP_T;

BUTTON_CODE_MAP_T button_code_map[] = {
    {"btn_up",    TDL_BUTTON_PRESS_SINGLE_CLICK, KEY_UP},
    {"btn_down",  TDL_BUTTON_PRESS_SINGLE_CLICK, KEY_DOWN},
    {"btn_left",  TDL_BUTTON_PRESS_SINGLE_CLICK, KEY_LEFT},
    {"btn_right", TDL_BUTTON_PRESS_SINGLE_CLICK, KEY_RIGHT},
    {"btn_up",    TDL_BUTTON_LONG_PRESS_START, KEY_ENTER},
    {"btn_down",  TDL_BUTTON_LONG_PRESS_START, KEY_ESC},
    {"btn_right", TDL_BUTTON_LONG_PRESS_START, KEY_I},
};

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    uint32_t i;
    (void)argc;

    for (i = 0; i < sizeof(button_code_map) / sizeof(button_code_map[0]); i++) {
        if (strcmp(name, button_code_map[i].name) == 0 && event == button_code_map[i].event) {
            PR_DEBUG("Button pressed: %s, event: %d, key code: %d", name, event, button_code_map[i].lv_key_code);

            lv_vendor_disp_lock();
            lv_demo_ai_pocket_pet_handle_input(button_code_map[i].lv_key_code);
            lv_vendor_disp_unlock();

            break;
        }
    }
}

void pocket_pet_button_init(void)
{
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 50};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    for(uint32_t i=0; i<CNTSOF(button_code_map); i++) {
        tdl_button_create(button_code_map[i].name, &button_cfg, &button_hdl);

        tdl_button_event_register(button_hdl, button_code_map[i].event, __button_function_cb);
    }
}