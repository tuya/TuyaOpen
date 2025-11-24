/**
 * @file menu_video_screen.h
 * @brief Header file for the video menu screen
 */

#ifndef MENU_VIDEO_SCREEN_H
#define MENU_VIDEO_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

typedef enum {
    VIDEO_ACTION_PLAY_VIDEO,
    VIDEO_ACTION_TAKE_PHOTO,
    VIDEO_ACTION_RECORD_VIDEO,
    VIDEO_ACTION_VIEW_GALLERY,
} video_action_t;

extern Screen_t menu_video_screen;

void menu_video_screen_init(void);
void menu_video_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_VIDEO_SCREEN_H*/
