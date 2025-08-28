/**
 * @file startup_screen.c
 * Startup Screen for AI Pocket Pet Application
 *
 * This module handles the startup screen that displays the Tuya Open logo
 * and transitions to the main application after 1 second.
 */

/*********************
 *      INCLUDES
 *********************/
#include "startup_screen.h"
#include "ai_pocket_pet_app.h"
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/

#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif

#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *g_startup_screen = NULL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create and display the startup screen
 * @return Pointer to the startup screen object
 */
lv_obj_t* startup_screen_create(void)
{
    g_startup_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_startup_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_startup_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_startup_screen, LV_OPA_COVER, 0);
    lv_screen_load(g_startup_screen);

    // Create main title
    lv_obj_t *title = lv_label_create(g_startup_screen);
    lv_label_set_text(title, "TuyaOpen");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_color(title, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // Create subtitle
    lv_obj_t *subtitle = lv_label_create(g_startup_screen);
    lv_label_set_text(subtitle, "AI Pocket Pet Demo");
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);

    return g_startup_screen;
}

/**
 * Get the startup screen object
 * @return Pointer to the startup screen object
 */
lv_obj_t* startup_screen_get(void)
{
    return g_startup_screen;
}

/**
 * Timer callback to transition from startup to main screen
 * @param timer The timer object
 */
void startup_screen_timer_cb(lv_timer_t *timer)
{
    // Load the main screen
    lv_screen_load(lv_demo_ai_pocket_pet_get_main_screen());
    // Delete the timer
    lv_timer_del(timer);
}
