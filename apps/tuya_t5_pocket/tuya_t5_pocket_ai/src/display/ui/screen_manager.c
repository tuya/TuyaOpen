/**
 * @file screen_manager.c
 * @brief Implementation of screen manager for handling screen navigation and stack operations
 *
 * This file contains the implementation of a stack-based screen navigation system.
 * It provides functions to push screens onto the stack, pop screens from the stack,
 * and navigate between screens with animation effects.
 *
 * The implementation includes:
 * - Stack operations (init, push, pop, check if empty)
 * - Screen navigation functions (back, back to home)
 * - Screen loading with animation
 * - Screen lifecycle management
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "screen_manager.h"
#include "startup_screen.h"
#include "main_screen.h"
#include <stdio.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_DEPTH 6
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    Screen_t* screens[MAX_DEPTH]; /**< Array of screen pointers */
    uint8_t top;                  /**< Index of the top of the stack */
} ScreenStack_t;
/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static ScreenStack_t screen_stack;

/***********************************************************
***********************function define**********************
***********************************************************/

static void screen_stack_init(ScreenStack_t* stack) {
    stack->top = 0;
}

static uint8_t screen_stack_push(ScreenStack_t* stack, Screen_t* screen) {
    if (stack->top >= MAX_DEPTH)
		return -1;
    stack->screens[stack->top++] = screen;
	return 0;
}

static uint8_t screen_stack_pop(ScreenStack_t* stack) {
    if (stack->top <= 0)
		return -1;
    stack->screens[--stack->top]->deinit();
    return 0;
}

static uint8_t screen_stack_is_empty(const ScreenStack_t* stack) {
    return stack->top == 0;
}

static Screen_t* get_top_screen(ScreenStack_t* stack) {
    // Check if stack is empty
    if (stack->top == 0) {
        return NULL; // Return NULL if stack is empty
    }

    // Return pointer to top screen
    return stack->screens[stack->top - 1];
}

/**
 * @brief Get the current screen (top of stack)
 * @return Pointer to the current screen, or NULL if the screen stack is empty
 */
Screen_t* screen_get_now_screen(void) {
    return get_top_screen(&screen_stack);
}


/**
 * @brief Go back to the previous screen
 *
 * This function unloads the current screen and loads the previous screen.
 * If there is no previous screen, it loads the startup screen.
 */
void screen_back(void) {
	if (screen_stack_is_empty(&screen_stack)) {
        return;
    }

    // 弹出当前屏幕
    screen_stack_pop(&screen_stack);

    if (screen_stack_is_empty(&screen_stack)) {
        // 如果栈为空，入栈并切换到StartupScreen
        screen_stack_push(&screen_stack, &startup_screen);
        // screen_stack_push(&screen_stack, &startup_screen);
        startup_screen.init();

        // Check if screen object is valid before loading
        if (startup_screen.screen_obj && *startup_screen.screen_obj) {
            lv_scr_load_anim(*startup_screen.screen_obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
        } else {
            printf("Error: startup_screen.screen_obj is NULL or invalid\n");
        }
    } else {
        // 切换到上一个屏幕
        Screen_t *previous_screen = screen_stack.screens[screen_stack.top - 1];
        previous_screen->init();

        // Check if screen object is valid before loading
        if (previous_screen->screen_obj && *previous_screen->screen_obj) {
            lv_scr_load_anim(*previous_screen->screen_obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
        } else {
            printf("Error: previous_screen->screen_obj is NULL or invalid\n");
        }
    }
}

/**
 * @brief Go back to the home screen (bottom of stack)
 *
 * This function unloads all screens except the home screen.
 */
void screen_back_bottom(void) {

    if (screen_stack_is_empty(&screen_stack)) {
        // 栈为空时，不应发生
        return;
    }

    // 弹出除栈底的所有屏幕
    while(screen_stack.top > 1)
        screen_stack_pop(&screen_stack);
    screen_stack.screens[screen_stack.top - 1]->init(); // 初始化新屏幕

    // Check if screen object is valid before loading
    Screen_t *bottom_screen = screen_stack.screens[screen_stack.top - 1];
    if (bottom_screen->screen_obj && *bottom_screen->screen_obj) {
        lv_scr_load_anim(*bottom_screen->screen_obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true); // 加载并应用动画
    } else {
        printf("Error: bottom_screen->screen_obj is NULL or invalid\n");
    }
}

/**
 * @brief Load a new screen to the top of the stack
 *
 * @param newScreen Pointer to the new screen to be loaded
 */
void screen_load(Screen_t *newScreen) {
	// 检查堆栈是否已满
    if (screen_stack.top >= MAX_DEPTH - 1) {
        // 错误处理：堆栈满
        return;
    }

    // 如果堆栈非空，反初始化当前屏幕
    if (screen_stack.top > 0) {
        screen_stack.screens[screen_stack.top - 1]->deinit();
    }

    // 将新屏幕推入堆栈
    screen_stack_push(&screen_stack, newScreen);
    newScreen->init(); // 初始化新屏幕

    // Check if screen object is valid before loading
    if (newScreen->screen_obj && *newScreen->screen_obj) {
        lv_scr_load_anim(*newScreen->screen_obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true); // 加载并应用动画
    } else {
        printf("Error: newScreen->screen_obj is NULL or invalid\n");
    }
}

/**
 * @brief Initialize the screen manager
 *
 * This function initializes the screen stack and loads the startup screen.
 */
void screens_init(void) {
    screen_stack_init(&screen_stack);
    screen_stack_push(&screen_stack, &startup_screen);
    startup_screen.init();

    // Check if screen object is valid before loading
    if (startup_screen.screen_obj && *startup_screen.screen_obj) {
        lv_disp_load_scr(*startup_screen.screen_obj);
    } else {
        printf("Error: startup_screen.screen_obj is NULL or invalid during initialization\n");
    }
}
