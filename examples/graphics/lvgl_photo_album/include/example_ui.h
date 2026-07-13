/**
 * @file example_ui.h
 * @brief example_ui module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __EXAMPLE_UI_H__
#define __EXAMPLE_UI_H__

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
    UI_ACTION_OPEN_CAMERA,
    UI_ACTION_TAKE_PHOTO,
    UI_ACTION_CLOSE_CAMERA,
    UI_ACTION_OPEN_ALBUM,
    UI_ACTION_CLOSE_ALBUM,
    UI_ACTION_VIEW_NEXT_PHOTO,
    UI_ACTION_VIEW_PREV_PHOTO,
    UI_ACTION_DELETE_PHOTO,
    UI_ACTION_SELECT_PHOTO,  /**< Thumbnail tapped; data = (void*)(uintptr_t)index (0-based) */
    UI_ACTION_TOGGLE_GRID,   /**< Toggle-grid button pressed on the album screen */
}UI_ACTION_EVENT_E;

typedef void (*EXAMPLE_UI_ACTION_CB)(UI_ACTION_EVENT_E event, void *data);

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET example_ui_init(EXAMPLE_UI_ACTION_CB action_cb);

void example_ui_video_start(uint16_t width, uint16_t height);

void example_ui_video_flush(uint16_t width, uint16_t height, uint8_t *data);

void example_ui_video_status(char *string);

void example_ui_video_end(void);

void example_ui_view_album_start(uint32_t count);

void example_ui_view_album_update_count(uint32_t count);

void example_ui_view_photo(uint16_t width, uint16_t height, uint8_t *jpeg, uint32_t len, uint32_t idx);

void example_ui_view_album_end(void);

/**
 * @brief Switch the album screen to thumbnail grid view.
 *
 * Hides the full-screen photo viewer, clears any old cells, and shows the
 * scrollable grid container. Call example_ui_grid_add_thumb() for each photo.
 *
 * @param[in] count Total number of photos (shown in the info label)
 */
void example_ui_grid_show(uint32_t count);

/**
 * @brief Add one thumbnail cell to the grid.
 *
 * Copies @a rgb565 internally; the caller may free it immediately after return.
 *
 * @param[in] idx     0-based photo index, echoed back via UI_ACTION_SELECT_PHOTO
 * @param[in] w       Thumbnail width in pixels
 * @param[in] h       Thumbnail height in pixels
 * @param[in] rgb565  Packed RGB565 pixel data
 * @param[in] size    Byte size of @a rgb565
 */
void example_ui_grid_add_thumb(uint32_t idx, uint16_t w, uint16_t h,
                                const uint8_t *rgb565, uint32_t size);

/**
 * @brief Switch the album screen back to full-screen photo viewer.
 *
 * Hides the grid container and reveals the previously loaded photo.
 * Does NOT reload or decode the photo — it must have been loaded via
 * example_ui_view_photo() already.
 */
void example_ui_show_viewer(void);


#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_UI_H__ */
