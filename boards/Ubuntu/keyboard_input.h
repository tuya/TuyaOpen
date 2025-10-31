/**
 * @file keyboard_input.h
 * @brief Keyboard input handler for Ubuntu platform
 *
 * Provides keyboard input handling to simulate button press for chat triggering.
 * Press 'S' key to start/stop conversation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __KEYBOARD_INPUT_H__
#define __KEYBOARD_INPUT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    KEYBOARD_EVENT_PRESS_S = 0,      /**< 'S' key pressed - Start */
    KEYBOARD_EVENT_PRESS_X,          /**< 'X' key pressed - Stop/End */
    KEYBOARD_EVENT_PRESS_Q,          /**< 'Q' key pressed (quit) */
    KEYBOARD_EVENT_PRESS_V,          /**< 'V' key pressed (volume up) */
    KEYBOARD_EVENT_PRESS_D,          /**< 'D' key pressed (volume down) */
} KEYBOARD_EVENT_E;

typedef void (*KEYBOARD_EVENT_CB)(KEYBOARD_EVENT_E event, void *arg);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize keyboard input handler
 *
 * Creates a background thread to monitor keyboard input.
 * Supports non-blocking keyboard reading for chat triggering.
 *
 * @param[in] callback  Callback function for keyboard events
 * @param[in] arg       User argument passed to callback
 *
 * @return OPERATE_RET
 * @retval OPRT_OK      Success
 * @retval Other        Error code
 */
OPERATE_RET keyboard_input_init(KEYBOARD_EVENT_CB callback, void *arg);

/**
 * @brief Deinitialize keyboard input handler
 *
 * Stops the keyboard monitoring thread and cleans up resources.
 *
 * @return OPERATE_RET
 */
OPERATE_RET keyboard_input_deinit(void);

/**
 * @brief Check if keyboard input is active
 *
 * @return bool
 * @retval true   Keyboard handler is running
 * @retval false  Keyboard handler is not running
 */
bool keyboard_input_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEYBOARD_INPUT_H__ */

