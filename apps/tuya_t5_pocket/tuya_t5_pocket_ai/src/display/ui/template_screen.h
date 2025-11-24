/**
 * @file template_screen.h
 * @brief Declaration of the template screen for the application
 *
 * This file contains the declarations for the template screen which is displayed
 * when the application starts. It shows a splash screen with "TuyaOpen" and
 * "AI Pocket Pet Demo" text, and automatically transitions after a timeout.
 *
 * The template screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef TEMPLATE_SCREEN_H
#define TEMPLATE_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t template_screen;

void template_screen_init(void);
void template_screen_deinit(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*template_SCREEN_H*/
