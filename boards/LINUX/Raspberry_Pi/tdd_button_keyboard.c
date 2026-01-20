/**
 * @file tdd_button_keyboard.c
 * @brief tdd_button_keyboard module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_button_keyboard.h"
#include "tal_log.h"
#include "tuya_error_code.h"

#include "tal_api.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define BUTTON_NAME_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char name[BUTTON_NAME_LEN];
    BUTTON_CFG_T cfg;
    struct termios orig_termios;
    int termios_saved;
    uint8_t key_pressed;  // Record key pressed state
    uint64_t last_press_time;  // Last time the target character was read (milliseconds)
} BUTTON_CTX_T;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __add_new_button(char *name, BUTTON_CFG_T *data, DEVICE_BUTTON_HANDLE *handle)
{
    BUTTON_CTX_T *p_ctx = NULL;

    if (NULL == name) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == data) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    p_ctx = tal_malloc(sizeof(BUTTON_CTX_T));
    if (NULL == p_ctx) {
        PR_ERR("keyboard button malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(p_ctx, 0, sizeof(BUTTON_CTX_T));
    memcpy(&p_ctx->cfg, data, sizeof(BUTTON_CFG_T));
    strncpy(p_ctx->name, name, BUTTON_NAME_LEN - 1);
    p_ctx->name[BUTTON_NAME_LEN - 1] = '\0';
    p_ctx->termios_saved = 0;
    p_ctx->key_pressed = 0;
    p_ctx->last_press_time = 0;

    *handle = (DEVICE_BUTTON_HANDLE *)p_ctx;

    return OPRT_OK;
}

static void __button_delete(DEVICE_BUTTON_HANDLE handle)
{
    BUTTON_CTX_T *p_ctx = (BUTTON_CTX_T *)handle;

    if (NULL != p_ctx) {
        tal_free(p_ctx);
    }
}


static OPERATE_RET __tdd_button_create(TDL_BUTTON_OPRT_INFO *dev)
{
    BUTTON_CTX_T *p_ctx = (BUTTON_CTX_T *)dev->dev_handle;
    struct termios new_termios;

    PR_DEBUG("button %s create", p_ctx->name);

    // Save current terminal settings
    if (tcgetattr(STDIN_FILENO, &p_ctx->orig_termios) == 0) {
        p_ctx->termios_saved = 1;
        
        // Set new terminal attributes
        new_termios = p_ctx->orig_termios;
        
        // Disable canonical mode and echo, but keep ISIG to support signals like Ctrl-C
        new_termios.c_lflag &= ~(ICANON | ECHO);
        // Keep ISIG flag to make control keys like Ctrl-C, Ctrl-Z work normally
        new_termios.c_lflag |= ISIG;
        
        // Set to return immediately
        new_termios.c_cc[VMIN] = 0;
        new_termios.c_cc[VTIME] = 0;
        
        // Apply new terminal settings
        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) < 0) {
            PR_ERR("tcsetattr failed");
            p_ctx->termios_saved = 0;
        } else {
            PR_DEBUG("terminal set to raw mode for button %s", p_ctx->name);
        }
    } else {
        PR_WARN("tcgetattr failed, button detection may require Enter key");
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_button_delete(TDL_BUTTON_OPRT_INFO *dev)
{
    BUTTON_CTX_T *p_ctx = (BUTTON_CTX_T *)dev->dev_handle;

    PR_DEBUG("button %s delete", p_ctx->name);

    // Restore original terminal settings
    if (p_ctx->termios_saved) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &p_ctx->orig_termios) < 0) {
            PR_ERR("Failed to restore terminal settings");
        } else {
            PR_DEBUG("terminal restored for button %s", p_ctx->name);
        }
        p_ctx->termios_saved = 0;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_button_read_value(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    BUTTON_CTX_T *p_ctx = (BUTTON_CTX_T *)dev->dev_handle;
    char ch = 0;
    int flags = 0;
    ssize_t ret = 0;
    char name_upper = 0;
    char ch_upper = 0;
    int found_match = 0;
    uint64_t current_time = 0;

    if (NULL == dev || NULL == value) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == p_ctx || NULL == p_ctx->name) {
        return OPRT_INVALID_PARM;
    }

    // Get the first character of name, convert to uppercase if it's a lowercase letter
    name_upper = p_ctx->name[0];
    if (name_upper >= 'a' && name_upper <= 'z') {
        name_upper = name_upper - 'a' + 'A';
    }

    // Get current time (milliseconds)
    current_time = tal_system_get_millisecond();

    // Default to using the previous key pressed state
    *value = p_ctx->key_pressed;

    // Check if more than 600ms have passed without reading the target character
    if (p_ctx->key_pressed && (current_time - p_ctx->last_press_time > 600)) {
        // More than 600ms passed, consider the key released
        p_ctx->key_pressed = 0;
        *value = 0;
    }

    // Get current file flags for standard input
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0) {
        return OPRT_OK;
    }

    // Set to non-blocking mode
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
        return OPRT_OK;
    }

    // Non-blocking read of keyboard input, read all available characters
    while ((ret = read(STDIN_FILENO, &ch, 1)) > 0) {
        // Only process alphanumeric characters (a-z, A-Z, 0-9)
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            // Convert received character to uppercase (if it's a letter)
            ch_upper = ch;
            if (ch_upper >= 'a' && ch_upper <= 'z') {
                ch_upper = ch_upper - 'a' + 'A';
            }
            
            // Check if the read character matches the first character of the button name
            if (ch_upper == name_upper) {
                // Match found, key pressed
                found_match = 1;
                p_ctx->key_pressed = 1;
                p_ctx->last_press_time = current_time;
                *value = 1;
            } else {
                // No match, another key was pressed, clear state
                p_ctx->key_pressed = 0;
                *value = 0;
            }
        }
    }
    
    // Restore original file flags
    fcntl(STDIN_FILENO, F_SETFL, flags);

    return OPRT_OK;
}

OPERATE_RET tdd_keyboard_button_register(char *name, BUTTON_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_BUTTON_DEVICE_INFO_T device_info;

    TDL_BUTTON_CTRL_INFO ctrl_info = {0};
    memset(&ctrl_info, 0, sizeof(TDL_BUTTON_CTRL_INFO));
    ctrl_info.button_create = __tdd_button_create;
    ctrl_info.button_delete = __tdd_button_delete;
    ctrl_info.read_value = __tdd_button_read_value;

    DEVICE_BUTTON_HANDLE handle = NULL;
    TUYA_CALL_ERR_RETURN(__add_new_button(name, cfg, &handle));

    device_info.mode = cfg->mode;
    device_info.dev_handle = handle;
    rt = tdl_button_register(name, &ctrl_info, &device_info);
    if (OPRT_OK != rt) {
        PR_ERR("tdl button register err");
        __button_delete(handle);
        return rt;
    }

    return rt;
}

OPERATE_RET tdd_gpio_button_update_level(DEVICE_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level)
{
    BUTTON_CTX_T *p_ctx = (BUTTON_CTX_T *)handle;

    PR_DEBUG("button %s set level %d", p_ctx->name, level);

    return OPRT_OK;
}