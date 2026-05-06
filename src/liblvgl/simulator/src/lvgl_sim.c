/**
 * @file lvgl_sim.c
 * @brief LVGL PC simulator — SDL2 bootstrap, entry point, main loop
 * @version 1.0
 * @date 2025-04-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "lvgl_sim.h"
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include <unistd.h>

#ifndef SIM_SCREEN_WIDTH
#define SIM_SCREEN_WIDTH 384
#endif

#ifndef SIM_SCREEN_HEIGHT
#define SIM_SCREEN_HEIGHT 168
#endif

#ifndef SIM_WINDOW_TITLE
#define SIM_WINDOW_TITLE "TuyaOpen LVGL Simulator"
#endif

/**
 * @brief Start LVGL simulator with SDL2
 * @param[in] entry_cb UI init function
 * @return none
 */
void lvgl_sim_start(LVGL_SIM_ENTRY_CB entry_cb)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(SIM_SCREEN_WIDTH,
                                                SIM_SCREEN_HEIGHT);
    lv_sdl_window_set_title(disp, SIM_WINDOW_TITLE);
    lv_sdl_mouse_create();

    lv_indev_t *kb = lv_sdl_keyboard_create();
    lv_group_t *g  = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(kb, g);

    if (entry_cb) {
        entry_cb();
    }

    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms > 50) {
            ms = 50;
        }
        usleep(ms * 1000);
    }
}

/* LVGL_SIM_ENTRY_FUNC is injected via compile definition (e.g. ui_init) */
extern void LVGL_SIM_ENTRY_FUNC(void);

/**
 * @brief Simulator entry point — provides main() for Linux platform
 */
int main(void)
{
    lvgl_sim_start(LVGL_SIM_ENTRY_FUNC);
    return 0;
}
