/**
 * @file txt_viewer_screen.h
 * @brief Simple text file viewer screen header
 *
 * This file contains the declarations for a simple text viewer that reads
 * and displays text files using a single label widget.
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef TXT_VIEWER_SCREEN_H
#define TXT_VIEWER_SCREEN_H

#include "screen_manager.h"
#include <stdbool.h>

/***********************************************************
************************macro define************************
***********************************************************/

#define TXT_MAX_SIZE        (64 * 1024)     /**< Maximum text file size (64KB) */
#define TXT_DIR_PATH        "/home/share/samba/lv_port_pc_vscode/txt"  /**< Text files directory */

/***********************************************************
***********************type define**************************
***********************************************************/

/**
 * @brief Text viewer state structure
 */
typedef struct {
    char *content;              /**< Text content buffer */
    size_t content_size;        /**< Size of loaded content */
    bool content_loaded;        /**< Whether content is loaded */
    char current_file[256];     /**< Current file name */
} txt_viewer_state_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the text viewer screen
 */
void txt_viewer_screen_init(void);

/**
 * @brief Deinitialize the text viewer screen
 */
void txt_viewer_screen_deinit(void);

/**
 * @brief Load a text file for viewing
 *
 * @param filepath Path to the text file
 * @return true on success, false on failure
 */
bool txt_viewer_load_file(const char *filepath);

/**
 * @brief Clear the displayed text
 */
void txt_viewer_clear(void);

/***********************************************************
***********************extern variable**********************
***********************************************************/

extern Screen_t txt_viewer_screen;

#endif // TXT_VIEWER_SCREEN_H
