/**
 * @file lv_games.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_system.h"

#include "lvgl.h"
#include "lv_vendor.h"
#include "board_com_api.h"
#include "lv_games.h"
#include "sy_black.c"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

LV_FONT_DECLARE(sy_black)   // 自定义字体
const lv_font_t * my_font = &sy_black;

/***********************************************************
***********************function define**********************
***********************************************************/

#if LV_USE_GAME_HUARONGDAO == 1     // 华容道
static void new_game_hrd_event_handler(lv_event_t *e)
{
    huarongdao();
}
#endif

#if LV_USE_GAME_XIAOXIAOLE == 1     // 消消乐
static void new_game_xxl_event_handler(lv_event_t *e)
{
    xiaoxiaole();
}
#endif

#if LV_USE_GAME_YANG == 1           // 羊了个羊
static void new_game_yang_event_handler(lv_event_t *e)
{
    yang_game();
}
#endif

#if LV_USE_GAME_PVZ == 1            // 植物大战僵尸
static void new_game_pvz_event_handler(lv_event_t *e)
{
    pvz_start();
}
#endif

#if LV_USE_GAME_2048 == 1           // 2048
static void new_game_2048_event_handler(lv_event_t *e)
{
    lv_2048_start();
}
#endif

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    int32_t y_pos = 55;
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    lv_vendor_init(DISPLAY_NAME);

    lv_obj_t *scr_main = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(0xf8f5f0), LV_PART_MAIN);
    lv_obj_set_scroll_dir(scr_main, LV_DIR_NONE);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_main, LV_SCROLLBAR_MODE_OFF);

    #if LV_USE_GAME_HUARONGDAO == 1
        lv_obj_t * btn_hrd = lv_btn_create(scr_main);
        lv_obj_set_size(btn_hrd, 150, 50);
        lv_obj_align_to(btn_hrd, scr_main, LV_ALIGN_TOP_MID, 0, y_pos);
        lv_obj_add_event_cb(btn_hrd, new_game_hrd_event_handler, LV_EVENT_CLICKED, scr_main);

        lv_obj_t * lbl_hrd = lv_label_create(btn_hrd);
        lv_label_set_text(lbl_hrd, "华容道");
        lv_obj_set_style_text_font(lbl_hrd, my_font, 0);
        lv_obj_center(lbl_hrd);
        y_pos += 80;
    #endif

    #if LV_USE_GAME_XIAOXIAOLE == 1
        lv_obj_t * btn_xxl = lv_btn_create(scr_main);
        lv_obj_set_size(btn_xxl, 150, 50);
        lv_obj_align_to(btn_xxl, scr_main, LV_ALIGN_TOP_MID, 0, y_pos);
        lv_obj_add_event_cb(btn_xxl, new_game_xxl_event_handler, LV_EVENT_CLICKED, scr_main);

        lv_obj_t * lbl_xxl = lv_label_create(btn_xxl);
        lv_label_set_text(lbl_xxl, "消消乐");
        lv_obj_set_style_text_font(lbl_xxl, my_font, 0);
        lv_obj_center(lbl_xxl);
        y_pos += 80;
    #endif

    #if LV_USE_GAME_YANG == 1
        lv_obj_t * btn_yang = lv_btn_create(scr_main);
        lv_obj_set_size(btn_yang, 150, 50);
        lv_obj_align_to(btn_yang, scr_main, LV_ALIGN_TOP_MID, 0, y_pos);
        lv_obj_add_event_cb(btn_yang, new_game_yang_event_handler, LV_EVENT_CLICKED, scr_main);

        lv_obj_t * lbl_yang = lv_label_create(btn_yang);
        lv_label_set_text(lbl_yang, "羊了个羊");
        lv_obj_set_style_text_font(lbl_yang, my_font, 0);
        lv_obj_center(lbl_yang);
        y_pos += 80;
    #endif

    #if LV_USE_GAME_PVZ == 1
        lv_obj_t * btn_pvz = lv_btn_create(scr_main);
        lv_obj_set_size(btn_pvz, 150, 50);
        lv_obj_align_to(btn_pvz, scr_main, LV_ALIGN_TOP_MID, 0, y_pos);
        lv_obj_add_event_cb(btn_pvz, new_game_pvz_event_handler, LV_EVENT_CLICKED, scr_main);

        lv_obj_t * lbl_pvz = lv_label_create(btn_pvz);
        lv_label_set_text(lbl_pvz, "植物大战僵尸");
        lv_obj_set_style_text_font(lbl_pvz, my_font, 0);
        lv_obj_center(lbl_pvz);
        y_pos += 80;
    #endif

    #if LV_USE_GAME_2048 == 1
        lv_obj_t * btn_2048 = lv_btn_create(scr_main);
        lv_obj_set_size(btn_2048, 150, 50);
        lv_obj_align_to(btn_2048, scr_main, LV_ALIGN_TOP_MID, 0, y_pos);
        lv_obj_add_event_cb(btn_2048, new_game_2048_event_handler, LV_EVENT_CLICKED, scr_main);

        lv_obj_t * lbl_2048 = lv_label_create(btn_2048);
        lv_label_set_text(lbl_2048, "2048");
        lv_obj_set_style_text_font(lbl_2048, my_font, 0);
        lv_obj_center(lbl_2048);
        y_pos += 80;
    #endif

    lv_vendor_start();
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void) arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif