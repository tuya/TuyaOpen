/**
 * @file ui_hello_world.h
 * @brief UI interface for hello world example
 *
 * This file provides the interface for creating and managing the hello world UI,
 * including button creation and message box display.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef UI_HELLO_WORLD_H_
#define UI_HELLO_WORLD_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)

/**
 * @brief Initialize the hello world UI
 * 
 * This function creates a button on the screen and sets up event handlers.
 * The button will display a "hello world" message box when clicked.
 */
void ui_hello_world_init(void);

#endif // ENABLE_LIBLVGL

#ifdef __cplusplus
}
#endif

#endif /* UI_HELLO_WORLD_H_ */

