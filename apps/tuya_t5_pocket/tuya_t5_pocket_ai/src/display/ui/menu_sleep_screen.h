/**
 * @file menu_sleep_screen.h
 * @brief Header file for the sleep menu screen
 */

#ifndef MENU_SLEEP_SCREEN_H
#define MENU_SLEEP_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

typedef enum {
    SLEEP_ACTION_SLEEP,
    SLEEP_ACTION_WAKE_UP,
    SLEEP_ACTION_SET_BEDTIME,
    SLEEP_ACTION_CHECK_SLEEP_STATUS,
} sleep_action_t;

extern Screen_t menu_sleep_screen;

typedef struct {
    bool is_sleeping;
    uint8_t sleep_quality;
    uint32_t sleep_duration;
    uint32_t bedtime_hour;
} sleep_status_t;

void menu_sleep_screen_init(void);
void menu_sleep_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_SLEEP_SCREEN_H*/
