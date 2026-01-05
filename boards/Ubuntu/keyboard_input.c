/**
 * @file keyboard_input.c
 * @brief Implementation of keyboard input handler for Ubuntu platform
 *
 * Provides non-blocking keyboard input handling to trigger chat conversations.
 * Press 'S' to start/stop conversation, 'Q' to quit.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(OPERATING_SYSTEM) && (OPERATING_SYSTEM == 100) // Linux

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#include "tal_api.h"
#include "keyboard_input.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool is_running;
    pthread_t thread;
    KEYBOARD_EVENT_CB callback;
    void *user_arg;
    struct termios old_termios;
    bool terminal_modified;  // Track if terminal was modified
} KEYBOARD_INPUT_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static KEYBOARD_INPUT_CTX_T g_keyboard_ctx = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Restore terminal to original mode
 */
static void __restore_terminal(void)
{
    if (g_keyboard_ctx.terminal_modified) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_keyboard_ctx.old_termios);
        g_keyboard_ctx.terminal_modified = false;
        PR_INFO("Terminal settings restored");
    }
}

/**
 * @brief Signal handler for termination signals
 */
static void __signal_handler(int signum)
{
    PR_WARN("Received signal %d, cleaning up...", signum);
    __restore_terminal();
    signal(signum, SIG_DFL);  // Restore default handler
    raise(signum);            // Re-raise the signal
}

/**
 * @brief Set terminal to non-canonical mode for character-by-character input
 */
static int __set_nonblocking_input(void)
{
    struct termios new_termios;

    // Get current terminal settings
    if (tcgetattr(STDIN_FILENO, &g_keyboard_ctx.old_termios) < 0) {
        PR_ERR("Failed to get terminal attributes");
        return -1;
    }

    // Copy to new settings
    new_termios = g_keyboard_ctx.old_termios;

    // Disable canonical mode and echo
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 0;   // Non-blocking read
    new_termios.c_cc[VTIME] = 1;  // 0.1 second timeout

    // Apply new settings
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) < 0) {
        PR_ERR("Failed to set terminal attributes");
        return -1;
    }

    g_keyboard_ctx.terminal_modified = true;

    // Register signal handlers to restore terminal on abnormal termination
    signal(SIGINT, __signal_handler);   // Ctrl+C
    signal(SIGTERM, __signal_handler);  // kill command
    signal(SIGQUIT, __signal_handler);  // Ctrl+backslash
    signal(SIGHUP, __signal_handler);   // Terminal hangup

    return 0;
}

/**
 * @brief Keyboard monitoring thread
 */
static void *__keyboard_monitor_thread(void *arg)
{
    char ch;
    KEYBOARD_EVENT_E event;

    PR_INFO("Keyboard input handler started");
    PR_INFO("Commands:");
    PR_INFO("  [S] - Start listening");
    PR_INFO("  [X] - Stop listening");
    PR_INFO("  [V] - Volume up");
    PR_INFO("  [D] - Volume down");
    PR_INFO("  [Q] - Quit application");
    PR_INFO("----------------------------------------");

    while (g_keyboard_ctx.is_running) {
        // Read one character (non-blocking)
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            // Convert to uppercase
            if (ch >= 'a' && ch <= 'z') {
                ch = ch - 'a' + 'A';
            }

            // Process key press
            switch (ch) {
            case 'S':
                event = KEYBOARD_EVENT_PRESS_S;
                PR_NOTICE("Key pressed: [S] - Start listening");
                if (g_keyboard_ctx.callback) {
                    g_keyboard_ctx.callback(event, g_keyboard_ctx.user_arg);
                }
                break;

            case 'X':
                event = KEYBOARD_EVENT_PRESS_X;
                PR_NOTICE("Key pressed: [X] - Stop listening");
                if (g_keyboard_ctx.callback) {
                    g_keyboard_ctx.callback(event, g_keyboard_ctx.user_arg);
                }
                break;

            case 'V':
                event = KEYBOARD_EVENT_PRESS_V;
                PR_NOTICE("Key pressed: [V] - Volume up");
                if (g_keyboard_ctx.callback) {
                    g_keyboard_ctx.callback(event, g_keyboard_ctx.user_arg);
                }
                break;

            case 'D':
                event = KEYBOARD_EVENT_PRESS_D;
                PR_NOTICE("Key pressed: [D] - Volume down");
                if (g_keyboard_ctx.callback) {
                    g_keyboard_ctx.callback(event, g_keyboard_ctx.user_arg);
                }
                break;

            case 'Q':
                event = KEYBOARD_EVENT_PRESS_Q;
                PR_NOTICE("Key pressed: [Q] - Quit");
                if (g_keyboard_ctx.callback) {
                    g_keyboard_ctx.callback(event, g_keyboard_ctx.user_arg);
                }
                // Let the callback handle app exit gracefully
                // The atexit handler will ensure terminal is restored
                break;

            case '\n':
            case '\r':
                // Ignore newline
                break;

            default:
                // Unknown key, ignore
                break;
            }
        }

        // Small delay to reduce CPU usage
        usleep(10000); // 10ms
    }

    PR_INFO("Keyboard input handler stopped");
    return NULL;
}

/**
 * @brief Initialize keyboard input handler
 */
OPERATE_RET keyboard_input_init(KEYBOARD_EVENT_CB callback, void *arg)
{
    if (g_keyboard_ctx.is_running) {
        PR_WARN("Keyboard input already initialized");
        return OPRT_OK;
    }

    if (NULL == callback) {
        PR_ERR("Callback is NULL");
        return OPRT_INVALID_PARM;
    }

    // Set non-blocking input mode
    if (__set_nonblocking_input() < 0) {
        PR_ERR("Failed to set non-blocking input");
        return OPRT_COM_ERROR;
    }

    // Initialize context
    g_keyboard_ctx.callback = callback;
    g_keyboard_ctx.user_arg = arg;
    g_keyboard_ctx.is_running = true;

    // Create monitoring thread
    if (pthread_create(&g_keyboard_ctx.thread, NULL, __keyboard_monitor_thread, NULL) != 0) {
        PR_ERR("Failed to create keyboard monitoring thread");
        __restore_terminal();
        g_keyboard_ctx.is_running = false;
        return OPRT_COM_ERROR;
    }

    PR_INFO("Keyboard input handler initialized");

    return OPRT_OK;
}

/**
 * @brief Deinitialize keyboard input handler
 */
OPERATE_RET keyboard_input_deinit(void)
{
    if (!g_keyboard_ctx.is_running) {
        return OPRT_OK;
    }

    // Stop the thread
    g_keyboard_ctx.is_running = false;

    // Wait for thread to exit
    pthread_join(g_keyboard_ctx.thread, NULL);

    // Restore terminal settings
    __restore_terminal();

    PR_INFO("Keyboard input handler deinitialized");

    return OPRT_OK;
}

/**
 * @brief Check if keyboard input is active
 */
bool keyboard_input_is_active(void)
{
    return g_keyboard_ctx.is_running;
}

#endif /* OPERATING_SYSTEM == 100 */

