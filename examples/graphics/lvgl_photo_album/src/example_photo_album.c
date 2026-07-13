/**
 * @file example_lvgl_photo_album.c
 * @brief LVGL touch UI for camera & album: home screen, camera capture, album browse
 *
 * Screens:
 *   - Home:   two buttons — "Take Photo" and "View Album"
 *   - Camera: live preview with red shutter button and back button
 *   - Album:  full-screen photo viewer by default; a toggle button switches to
 *             thumbnail grid view. Tap a thumbnail to jump back to that photo.
 *             Swipe left/right navigates in viewer mode.
 *
 * @version 0.3
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "lvgl.h"
#include "lv_vendor.h"

#include "tal_image.h"

#include "example_camera.h"
#include "example_album.h"
#include "example_ui.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* Thumbnail size used when generating the grid view */
#define THUMB_WIDTH   80
#define THUMB_HEIGHT  60

/***********************************************************
***********************variable define**********************
***********************************************************/
static char s_current_photo_name[ALBUM_FILENAME_MAX_LEN + 1];

typedef enum {
    ALBUM_MODE_VIEWER = 0,  /**< Full-screen photo viewer */
    ALBUM_MODE_GRID,        /**< Thumbnail grid */
} ALBUM_MODE_E;

static ALBUM_MODE_E s_album_mode = ALBUM_MODE_VIEWER;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __album_show_next(void)
{
    OPERATE_RET rt = OPRT_OK;
    EXAMPLE_ALBUM_PHOTO_T photo;
    uint8_t *jpeg_data = NULL;
    size_t   jpeg_len = 0;

    TUYA_CALL_ERR_LOG(example_album_browse_next(&photo, &jpeg_data, &jpeg_len));
    if (rt != OPRT_OK) {
        return;
    }

    strncpy(s_current_photo_name, photo.name, ALBUM_FILENAME_MAX_LEN);
    s_current_photo_name[ALBUM_FILENAME_MAX_LEN] = '\0';

    example_ui_view_photo(photo.width, photo.height, jpeg_data, jpeg_len, photo.index);

    example_album_release_jpeg_data(jpeg_data);
}

static void __album_show_prev(void)
{
    OPERATE_RET rt = OPRT_OK;
    EXAMPLE_ALBUM_PHOTO_T photo;
    uint8_t *jpeg_data = NULL;
    size_t   jpeg_len = 0;

    TUYA_CALL_ERR_LOG(example_album_browse_prev(&photo, &jpeg_data, &jpeg_len));
    if (rt != OPRT_OK) {
        return;
    }

    strncpy(s_current_photo_name, photo.name, ALBUM_FILENAME_MAX_LEN);
    s_current_photo_name[ALBUM_FILENAME_MAX_LEN] = '\0';

    example_ui_view_photo(photo.width, photo.height, jpeg_data, jpeg_len, photo.index);

    example_album_release_jpeg_data(jpeg_data);
}

/**
 * @brief Load all thumbnails and populate the grid view.
 *
 * Thumbnails are generated in small batches; each batch is freed immediately
 * after the pixels are copied into the UI layer, keeping peak RAM bounded.
 */
static void __load_grid_thumbs(uint32_t count)
{
    ALBUM_THUMB_ITER_HANDLE iter = NULL;
    OPERATE_RET rt;

    example_ui_grid_show(count);

    if (OPRT_OK != example_album_thumb_start(&iter)) {
        return;
    }

    ALBUM_THUMB_CFG_T cfg = {
        .width  = THUMB_WIDTH,
        .height = THUMB_HEIGHT,
        .fmt    = ALBUM_THUMB_FMT_RGB565,
        .fit    = ALBUM_THUMB_FIT_COVER,
    };

    ALBUM_THUMB_BATCH_T batch = {0};
    uint32_t global_idx = 0;

    for (;;) {
        rt = image_album_thumb_iter_next(iter, &cfg, 4, &batch);
        if (rt == OPRT_NOT_FOUND) {
            break; /* exhausted */
        }
        for (uint32_t i = 0; i < batch.count; i++) {
            ALBUM_THUMB_T *t = &batch.items[i].thumb;
            if (t->buf != NULL) {
                example_ui_grid_add_thumb(global_idx, t->width, t->height,
                                          t->buf, t->size);
            }
            global_idx++;
        }
        image_album_thumb_batch_free(&batch);
        if (rt != OPRT_OK && rt != OPRT_COM_ERROR) {
            break;
        }
    }

    example_album_thumb_end(iter);
}

static void __enter_album_mode(void)
{
    OPERATE_RET rt = OPRT_OK;

    s_album_mode = ALBUM_MODE_VIEWER;

    example_camera_preview_enable(FALSE);

    TUYA_CALL_ERR_LOG(example_album_browse_start());
    if (rt != OPRT_OK) {
        return;
    }

    example_ui_view_album_start(example_album_get_count());

    __album_show_next();
}

static void __exit_album_mode(void)
{
    s_album_mode = ALBUM_MODE_VIEWER;
    example_album_browse_end();
    example_ui_view_album_end();
}

static void __enter_camera_mode(void)
{
    example_camera_preview_enable(TRUE);
    example_ui_video_start(EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT);
}

static void __camera_frame_cb(uint8_t *yuv_data, uint16_t width, uint16_t height)
{
    example_ui_video_flush(width, height, yuv_data);
}

static void __exit_camera_mode(void)
{
    example_camera_preview_enable(FALSE);
    example_ui_video_end();
}

static void __camera_take_photo(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *jpeg_data = NULL;
    uint32_t jpeg_len = 0;

    example_ui_video_status("Capturing...");

    rt = example_camera_capture_jpeg(&jpeg_data, &jpeg_len);
    if (rt != OPRT_OK || jpeg_data == NULL) {
        example_ui_video_status("Capture failed!");
        return;
    }

    rt = example_album_save_photo(jpeg_data, jpeg_len);
    example_camera_release_jpeg(jpeg_data);

    if (rt != OPRT_OK) {
        example_ui_video_status("Save failed!");
    } else {
        uint32_t count = example_album_get_count();
        char buf[48];
        snprintf(buf, sizeof(buf), "Saved! (%u / %d)", count, EXAMPLE_ALBUM_MAX_PHOTOS);
        example_ui_video_status(buf);
    }
}

static void __example_ui_action(UI_ACTION_EVENT_E event, void *data)
{
    switch(event) {
        case UI_ACTION_OPEN_CAMERA: {
            __enter_camera_mode();
        } break;

        case UI_ACTION_TAKE_PHOTO: {
            __camera_take_photo();
        } break;

        case UI_ACTION_CLOSE_CAMERA: {
            __exit_camera_mode();
        } break;

        case UI_ACTION_OPEN_ALBUM: {
            __enter_album_mode();
        } break;

        case UI_ACTION_CLOSE_ALBUM: {
            __exit_album_mode();
        } break;

        /* Toggle-grid button: switch between full-screen viewer and thumbnail grid */
        case UI_ACTION_TOGGLE_GRID: {
            if (s_album_mode == ALBUM_MODE_VIEWER) {
                s_album_mode = ALBUM_MODE_GRID;
                __load_grid_thumbs(example_album_get_count());
            } else {
                s_album_mode = ALBUM_MODE_VIEWER;
                example_ui_show_viewer();
            }
        } break;

        /* Thumbnail tapped: seek the browse session to that photo and show it */
        case UI_ACTION_SELECT_PHOTO: {
            uint32_t idx = (uint32_t)(uintptr_t)data;
            s_album_mode = ALBUM_MODE_VIEWER;
            if (OPRT_OK == example_album_browse_seek(idx)) {
                __album_show_next();
            }
        } break;

        /* Swipe navigation: only active in viewer mode */
        case UI_ACTION_VIEW_NEXT_PHOTO: {
            if (s_album_mode == ALBUM_MODE_VIEWER) {
                __album_show_next();
            }
        } break;

        case UI_ACTION_VIEW_PREV_PHOTO: {
            if (s_album_mode == ALBUM_MODE_VIEWER) {
                __album_show_prev();
            }
        } break;

        case UI_ACTION_DELETE_PHOTO: {
            OPERATE_RET rt = example_album_delete_current(s_current_photo_name);
            if (rt != OPRT_OK) {
                break;
            }
            uint32_t count = example_album_get_count();
            if (count == 0) {
                __exit_album_mode();
            } else {
                s_album_mode = ALBUM_MODE_VIEWER;
                example_ui_view_album_update_count(count);
                __album_show_next();
            }
        } break;

        default:
            break;
    }
}

/**
 * @brief Application entry point
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();
    tal_workq_init();

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    board_register_hardware();

    TUYA_CALL_ERR_LOG(example_album_init());

    TUYA_CALL_ERR_LOG(example_camera_init());
    example_camera_set_frame_cb(__camera_frame_cb);

    TUYA_CALL_ERR_LOG(example_ui_init(__example_ui_action));

    while (1) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
        PR_DEBUG("psram free: %d, sram free: %d",
                 tal_psram_get_free_heap_size(), tal_system_get_free_heap_size());
#else
        PR_DEBUG("sram free: %d", tal_system_get_free_heap_size());
#endif
        tal_system_sleep(5000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
