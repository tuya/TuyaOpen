/**
 * @file pocket_button.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tuya_iot.h"

#include "tdl_button_manage.h"
#include "tdl_joystick_manage.h"
#include "lv_vendor.h"
#include "app_display.h"
#include "game_pet.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define MENU_BUTTON_NAME  "btn_menu"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char *name;
    TDL_BUTTON_TOUCH_EVENT_E event;
    uint32_t  key_tp;
}BUTTON_CODE_MAP_T;

BUTTON_CODE_MAP_T disp_btn_code_map[] = {
    {"btn_enter", TDL_BUTTON_PRESS_DOWN, KEY_ENTER},
    {"btn_esc",   TDL_BUTTON_PRESS_DOWN, KEY_ESC},
};

typedef struct {
    TDL_JOYSTICK_TOUCH_EVENT_E event;
    uint32_t key_tp;
}JOYSTICK_CODE_MAP_T;

JOYSTICK_CODE_MAP_T disp_joystick_code_map[] = {
    { TDL_JOYSTICK_UP, KEY_UP},
    { TDL_JOYSTICK_DOWN, KEY_DOWN},
    { TDL_JOYSTICK_LEFT, KEY_LEFT},
    { TDL_JOYSTICK_RIGHT, KEY_RIGHT},
    { TDL_JOYSTICK_BUTTON_PRESS_DOWN, KEY_JOYCON},
};


/***********************************************************
***********************variable define**********************
***********************************************************/
static uint32_t sg_cur_key = 0;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __menu_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    (void)argc; 

    if(TDL_BUTTON_PRESS_REPEAT == event) {
        PR_DEBUG("Reset ctrl data!");
        tuya_iot_reset(tuya_iot_client_get());
    }else if(TDL_BUTTON_LONG_PRESS_START == event) {
        game_pet_reset();
    }
}



static void __disp_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    uint32_t i;
    (void)argc;

    for (i = 0; i < CNTSOF(disp_btn_code_map); i++) {
        if (strcmp(name, disp_btn_code_map[i].name) == 0 && event == disp_btn_code_map[i].event) {
            PR_DEBUG("Button pressed: %s, event: %d, key type: %d", name, event, disp_btn_code_map[i].key_tp);
            sg_cur_key = disp_btn_code_map[i].key_tp;
            break;
        }
    }
}

static void __disp_joystick_function_cb(char *name, TDL_JOYSTICK_TOUCH_EVENT_E event, void *argc)
{
    uint32_t i;
    (void)argc;

     for (i = 0; i < CNTSOF(disp_joystick_code_map); i++) {
         if (event == disp_joystick_code_map[i].event) {
            PR_DEBUG("joystick event: %d,  key type: %d", event, disp_joystick_code_map[i].key_tp);
            sg_cur_key = disp_joystick_code_map[i].key_tp;
            break;
         }
     }
}

static void __keypad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    /*Set the last pressed coordinates*/
    data->key = sg_cur_key;
    
    // Set the key state - if there's a key press, set to PRESSED, otherwise RELEASED
    if (sg_cur_key != 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        sg_cur_key = 0; // Clear the key after reading to avoid repeated events
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void pocket_game_pet_indev_init(void)
{
    // display button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 3,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    tdl_button_create(MENU_BUTTON_NAME, &button_cfg, &button_hdl);
    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_REPEAT, __menu_button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_LONG_PRESS_START, __menu_button_function_cb);

    for(uint32_t i=0; i<CNTSOF(disp_btn_code_map); i++) {
        tdl_button_create(disp_btn_code_map[i].name, &button_cfg, &button_hdl);
        tdl_button_event_register(button_hdl, disp_btn_code_map[i].event, __disp_button_function_cb);
    }

    TDL_JOYSTICK_CFG_T joystick_cfg = {
        .button_cfg = {.long_start_valid_time = 3000,
                       .long_keep_timer = 1000,
                       .button_debounce_time = 50,
                       .button_repeat_valid_count = 2,
                       .button_repeat_valid_time = 500},
        .adc_cfg =
            {
                .adc_max_val = 8192,        /* adc max value */
                .adc_min_val = 0,           /* adc min value */
                .normalized_range = 10,     /* normalized range ±10 */
                .sensitivity = 2,           /* sensitivity should < normalized range */
            },
    };

    TDL_JOYSTICK_HANDLE sg_joystick_hdl = NULL;

    tdl_joystick_create(JOYSTICK_NAME, &joystick_cfg, &sg_joystick_hdl);

    for(uint32_t i=0; i<CNTSOF(disp_joystick_code_map); i++) {
        tdl_joystick_event_register(sg_joystick_hdl, disp_joystick_code_map[i].event, __disp_joystick_function_cb);
    }

    /*Register a touchpad input device*/
    lv_indev_t *indev_keypad = NULL;
    indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, __keypad_read);
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_indev_set_group(indev_keypad, group);
    lv_group_set_default(group);
}
