/**
 * @file camera_screen.c
 * @brief Implementation of the camera screen with binary conversion control
 *
 * This file contains the implementation of the camera screen which displays
 * camera feed on the left side and binary conversion settings on the right side.
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#include "camera_screen.h"
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_LVGL_HARDWARE
#include "tkl_output.h"
#include "yuv422_to_binary.h"

#include "tdl_display_manage.h"
#include "tdl_display_draw.h"
#include "tdl_camera_manage.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CAMERA_WIDTH  480
#define CAMERA_HEIGHT 480
#define CAMERA_FPS    20

#define CAMERA_AREA_WIDTH  240                                 // Left side for camera (240 pixels wide)
#define CAMERA_AREA_HEIGHT 168                                 // Display area height (crop from camera height)
#define INFO_AREA_X        240                                 // Right side starts at x=240
#define INFO_AREA_WIDTH    (AI_PET_SCREEN_WIDTH - INFO_AREA_X) // Right side width (384-240=144)
#define INFO_AREA_HEIGHT   168                                 // Info area height to match camera

#define PRINT_WIDTH  384
#define PRINT_HEIGHT 384

#define THRESHOLD_STEP 4   // Threshold adjustment step
#define THRESHOLD_MIN  0   // Minimum threshold
#define THRESHOLD_MAX  255 // Maximum threshold

// Font definitions - easily customizable
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_obj_t   *ui_camera_screen;
static lv_obj_t   *camera_canvas;   // Canvas for camera display
static lv_obj_t   *method_label;    // Binary method label
static lv_obj_t   *threshold_label; // Threshold value label
static lv_obj_t   *status_label;    // Camera status label
static lv_timer_t *update_timer;    // Timer for updating display

#ifdef ENABLE_LVGL_HARDWARE
static uint8_t               *canvas_buffer     = NULL; // Canvas buffer for monochrome image
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb   = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_2 = NULL;

// YUV422 raw data buffers (double buffering for camera input)
static uint8_t *sg_yuv422_buffer_1     = NULL; // YUV422 buffer 1 (240x240x2 bytes)
static uint8_t *sg_yuv422_buffer_2     = NULL; // YUV422 buffer 2 (240x240x2 bytes)
static uint8_t *sg_yuv422_write_buffer = NULL; // Current write buffer pointer

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl  = NULL;
static bool                camera_running     = false;
static bool                camera_initialized = false;
static volatile bool       frame_ready        = false; // Flag indicating new frame is ready
static volatile uint8_t    write_buffer_index = 0;     // Buffer being written by camera (0 or 1)
static volatile uint8_t    read_buffer_index  = 0;     // Buffer being read for display (0 or 1)
static MUTEX_HANDLE        sg_buffer_mutex    = NULL;  // Mutex to protect buffer access

static BINARY_CONFIG_T sg_binary_config = {
    .method          = BINARY_METHOD_FLOYD_STEINBERG,
    .fixed_threshold = 128,
};

// Calculated threshold for adaptive and otsu methods
static uint8_t sg_calculated_threshold = 128;

// Lifecycle callback
static camera_screen_lifecycle_cb_t sg_lifecycle_callback = NULL;

// Photo print callback
static camera_photo_print_cb_t sg_print_callback = NULL;
#endif

Screen_t camera_screen = {
    .init       = camera_screen_init,
    .deinit     = camera_screen_deinit,
    .screen_obj = &ui_camera_screen,
    .name       = "camera",
};

/***********************************************************
********************function declaration********************
***********************************************************/
static void        keyboard_event_cb(lv_event_t *e);
static void        update_info_display(void);
static void        update_timer_cb(lv_timer_t *timer);
static OPERATE_RET camera_init(void);
static OPERATE_RET camera_start(void);
static void        camera_stop(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Register lifecycle callback for camera screen
 * @param callback Callback function, NULL to unregister
 */
void camera_screen_register_lifecycle_cb(camera_screen_lifecycle_cb_t callback)
{
    sg_lifecycle_callback = callback;
    printf("[Camera] Lifecycle callback %s\n", callback ? "registered" : "unregistered");
}

/**
 * @brief Register photo print callback for camera screen
 * @param callback Callback function, NULL to unregister
 */
void camera_screen_register_print_cb(camera_photo_print_cb_t callback)
{
    sg_print_callback = callback;
    printf("[Camera] Print callback %s\n", callback ? "registered" : "unregistered");
}

/**
 * @brief Get method name string
 */
static const char *get_method_name(BINARY_METHOD_E method)
{
#ifdef ENABLE_LVGL_HARDWARE
    switch (method) {
    case BINARY_METHOD_FIXED:
        return "Fixed";
    case BINARY_METHOD_ADAPTIVE:
        return "Adaptive";
    case BINARY_METHOD_OTSU:
        return "Otsu";
    case BINARY_METHOD_BAYER4_DITHER:
        return "Bayer8";
    case BINARY_METHOD_BAYER8_DITHER:
        return "Bayer4";
    case BINARY_METHOD_BAYER16_DITHER:
        return "Bayer16";
    case BINARY_METHOD_FLOYD_STEINBERG:
        return "Floyd-Steinberg";
    case BINARY_METHOD_STUCKI:
        return "Stucki";
    case BINARY_METHOD_JARVIS:
        return "Jarvis";
    default:
        return "Unknown";
    }
#else
    return "N/A";
#endif
}

/**
 * @brief Update info area display
 */
static void update_info_display(void)
{
    char buf[64];

#ifdef ENABLE_LVGL_HARDWARE
    // Update method label
    snprintf(buf, sizeof(buf), "Method:\n%s", get_method_name(sg_binary_config.method));
    lv_label_set_text(method_label, buf);

    // Update threshold label based on method
    if (sg_binary_config.method == BINARY_METHOD_BAYER8_DITHER ||
        sg_binary_config.method == BINARY_METHOD_BAYER4_DITHER ||
        sg_binary_config.method == BINARY_METHOD_BAYER16_DITHER ||
        sg_binary_config.method == BINARY_METHOD_FLOYD_STEINBERG || sg_binary_config.method == BINARY_METHOD_STUCKI ||
        sg_binary_config.method == BINARY_METHOD_JARVIS) {
        snprintf(buf, sizeof(buf), "Threshold:\nN/A");
    } else {
        // For adaptive and otsu, show calculated threshold
        snprintf(buf, sizeof(buf), "Threshold:\n%d", sg_calculated_threshold);
    }
    lv_label_set_text(threshold_label, buf);

    // Update status
    snprintf(buf, sizeof(buf), "Status:\n%s", camera_running ? "Running" : "Stopped");
    lv_label_set_text(status_label, buf);
#else
    lv_label_set_text(method_label, "Method:\nN/A");
    lv_label_set_text(threshold_label, "Threshold:\nN/A");
    lv_label_set_text(status_label, "Status:\nDisabled");
#endif
}

/**
 * @brief Timer callback for updating display (runs in LVGL context)
 * @note Now handles image processing (YUV422 -> binary conversion)
 */
static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

#ifdef ENABLE_LVGL_HARDWARE

    // Check if new frame is ready and process it
    if (frame_ready && canvas_buffer && camera_canvas && sg_buffer_mutex) {
        // Lock mutex to safely access buffer indices and data
        tal_mutex_lock(sg_buffer_mutex);

        // Get the YUV422 buffer with latest frame data
        uint8_t *yuv422_source = (write_buffer_index == 0) ? sg_yuv422_buffer_1 : sg_yuv422_buffer_2;
        read_buffer_index      = write_buffer_index;
        frame_ready            = false; // Clear flag

        tal_mutex_unlock(sg_buffer_mutex);

        // Process YUV422 data to binary in LVGL timer context (safe for LVGL operations)
        // Get output buffer (toggle between two buffers for double buffering)
        TDL_DISP_FRAME_BUFF_T *output_fb = (read_buffer_index == 0) ? sg_p_display_fb_1 : sg_p_display_fb_2;

        // Convert YUV422 to binary using unified module
        YUV422_TO_BINARY_PARAMS_T params = {
            .yuv422_data   = yuv422_source,
            .src_width     = CAMERA_WIDTH,
            .src_height    = CAMERA_HEIGHT,
            .binary_data   = output_fb->frame,
            .dst_width     = CAMERA_AREA_WIDTH,
            .dst_height    = CAMERA_AREA_HEIGHT,
            .config        = &sg_binary_config,
            .invert_colors = 0 // Will be overridden by yuv422_to_lvgl_binary
        };
        yuv422_to_lvgl_binary(&params);

        // Copy processed binary data to canvas buffer
        // For LVGL I1 format: palette (8 bytes) + bitmap data
        uint32_t bitmap_size = (CAMERA_AREA_WIDTH + 7) / 8 * CAMERA_AREA_HEIGHT;
        memcpy(canvas_buffer + 8, output_fb->frame, bitmap_size);

        // Invalidate canvas to trigger redraw
        lv_obj_invalidate(camera_canvas);
    }
#endif

    update_info_display();
}

#ifdef ENABLE_LVGL_HARDWARE

/**
 * @brief Camera frame callback - only receive and save raw YUV422 data
 * @note This callback runs in camera thread context, should not call LVGL APIs
 * @note Image processing is now done in timer callback
 */
static OPERATE_RET camera_frame_callback(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    if (NULL == hdl || NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (!camera_running || !sg_buffer_mutex || !sg_yuv422_write_buffer) {
        return OPRT_OK;
    }

    // Lock mutex to safely copy data
    tal_mutex_lock(sg_buffer_mutex);

    // Determine current write buffer index
    uint8_t current_write_index = (sg_yuv422_write_buffer == sg_yuv422_buffer_1) ? 0 : 1;

    // Copy raw YUV422 data to buffer
    uint32_t       yuv422_size = frame->width * frame->height * 2;
    static uint8_t log_count   = 0;
    if (log_count < 3) {
        PR_NOTICE("Frame size: %dx%d, yuv422_size=%d bytes", frame->width, frame->height, yuv422_size);
        log_count++;
    }
    memcpy(sg_yuv422_write_buffer, frame->data, yuv422_size);

    // Mark which buffer contains the new frame
    write_buffer_index = current_write_index;

    // Toggle YUV422 buffer for next capture
    sg_yuv422_write_buffer = (sg_yuv422_write_buffer == sg_yuv422_buffer_1) ? sg_yuv422_buffer_2 : sg_yuv422_buffer_1;

    // Set flag to notify LVGL timer that new frame is ready
    frame_ready = true;

    tal_mutex_unlock(sg_buffer_mutex);

    return OPRT_OK;
}

/**
 * @brief Initialize camera hardware
 */
static OPERATE_RET camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("Camera init starting...");

    // Create mutex for buffer synchronization
    rt = tal_mutex_create_init(&sg_buffer_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create buffer mutex: %d", rt);
        return rt;
    }
    PR_DEBUG("Buffer mutex created");

    // Allocate YUV422 raw data buffers (384x384x2 = 294912 bytes each)
    uint32_t yuv422_size = CAMERA_WIDTH * CAMERA_HEIGHT * 2;
    sg_yuv422_buffer_1   = (uint8_t *)tal_psram_malloc(yuv422_size);
    if (NULL == sg_yuv422_buffer_1) {
        PR_ERR("Failed to allocate YUV422 buffer 1");
        return OPRT_MALLOC_FAILED;
    }

    sg_yuv422_buffer_2 = (uint8_t *)tal_psram_malloc(yuv422_size);
    if (NULL == sg_yuv422_buffer_2) {
        PR_ERR("Failed to allocate YUV422 buffer 2");
        tal_psram_free(sg_yuv422_buffer_1);
        return OPRT_MALLOC_FAILED;
    }

    sg_yuv422_write_buffer = sg_yuv422_buffer_1;
    PR_DEBUG("YUV422 buffers allocated: %d bytes each", yuv422_size);

    // Create frame buffers for binary output (240x168 after crop)
    uint32_t frame_len = (CAMERA_AREA_WIDTH + 7) / 8 * CAMERA_AREA_HEIGHT;
    PR_DEBUG("Binary frame buffer size: %d bytes", frame_len);

    sg_p_display_fb_1 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_1) {
        PR_ERR("create frame buff 1 failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_1->fmt    = TUYA_PIXEL_FMT_MONOCHROME;
    sg_p_display_fb_1->width  = CAMERA_AREA_WIDTH;
    sg_p_display_fb_1->height = CAMERA_AREA_HEIGHT;

    sg_p_display_fb_2 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_2) {
        PR_ERR("create frame buff 2 failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_2->fmt    = TUYA_PIXEL_FMT_MONOCHROME;
    sg_p_display_fb_2->width  = CAMERA_AREA_WIDTH;
    sg_p_display_fb_2->height = CAMERA_AREA_HEIGHT;

    sg_p_display_fb = sg_p_display_fb_1;

    // Find camera device
    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    TDL_CAMERA_CFG_T cfg;
    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));
    cfg.fps          = CAMERA_FPS;
    cfg.width        = CAMERA_WIDTH;
    cfg.height       = CAMERA_HEIGHT;
    cfg.out_fmt      = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = camera_frame_callback;

    PR_DEBUG("Camera config: %dx%d @ %d fps, callback=%p", cfg.width, cfg.height, cfg.fps, cfg.get_frame_cb);

    if (camera_initialized == false) {
        rt                 = tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg);
        camera_initialized = true;
    } else {
        rt = OPRT_OK;
    }

    if (OPRT_OK == rt) {
        camera_running = true;
        update_info_display(); // Update UI to show running status
        PR_NOTICE("Camera started successfully");
    } else {
        PR_ERR("Camera start failed: %d", rt);
    }

    PR_NOTICE("camera init success");
    return OPRT_OK;
}

/**
 * @brief Start camera capture
 */
static OPERATE_RET camera_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    PR_NOTICE("Starting camera...");

    if (camera_running) {
        PR_WARN("Camera already running");
        return OPRT_OK;
    }

    if (!sg_buffer_mutex) {
        PR_ERR("Buffer mutex not initialized");
        return OPRT_INVALID_PARM;
    }

    camera_running = true;
    update_info_display();

    return rt;
}

/**
 * @brief Stop camera capture
 */
static void camera_stop(void)
{
    if (camera_running && sg_tdl_camera_hdl) {
        camera_running = false; // Set flag first to stop frame processing
        tdl_camera_dev_close(sg_tdl_camera_hdl);
        PR_NOTICE("camera stopped");
    }
}
#endif // ENABLE_LVGL_HARDWARE

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t    key = lv_event_get_key(e);
    OPERATE_RET rt  = OPRT_OK;
    printf("[%s] Key pressed: %d\n", camera_screen.name, key);

    switch (key) {
    case KEY_UP:
#ifdef ENABLE_LVGL_HARDWARE
        // Increase threshold (only in fixed mode)
        if (sg_binary_config.method == BINARY_METHOD_FIXED) {
            if (sg_binary_config.fixed_threshold <= THRESHOLD_MAX - THRESHOLD_STEP) {
                sg_binary_config.fixed_threshold += THRESHOLD_STEP;
            } else {
                sg_binary_config.fixed_threshold = THRESHOLD_MAX;
            }
            printf("Threshold increased to %d\n", sg_binary_config.fixed_threshold);
        }
#endif
        break;

    case KEY_DOWN:
#ifdef ENABLE_LVGL_HARDWARE
        // Decrease threshold (only in fixed mode)
        if (sg_binary_config.method == BINARY_METHOD_FIXED) {
            if (sg_binary_config.fixed_threshold >= THRESHOLD_MIN + THRESHOLD_STEP) {
                sg_binary_config.fixed_threshold -= THRESHOLD_STEP;
            } else {
                sg_binary_config.fixed_threshold = THRESHOLD_MIN;
            }
            printf("Threshold decreased to %d\n", sg_binary_config.fixed_threshold);
        }
#endif
        break;

    case KEY_LEFT:
#ifdef ENABLE_LVGL_HARDWARE
        // Previous method
        if (sg_binary_config.method > 0) {
            sg_binary_config.method--;
        } else {
            sg_binary_config.method = BINARY_METHOD_COUNT - 1;
        }
        printf("Method changed to %s\n", get_method_name(sg_binary_config.method));
#endif
        break;

    case KEY_RIGHT:
#ifdef ENABLE_LVGL_HARDWARE
        // Next method
        sg_binary_config.method = (sg_binary_config.method + 1) % BINARY_METHOD_COUNT;
        printf("Method changed to %s\n", get_method_name(sg_binary_config.method));
#endif
        break;

    case KEY_ENTER:
#ifdef ENABLE_LVGL_HARDWARE
        if (camera_running) {
            // Camera is running: stop and print
            if (sg_print_callback) {
                // Stop camera first
                camera_stop();
                // Update status
                char buf[32];
                snprintf(buf, sizeof(buf), "Status:\n%s", "Printing");
                lv_label_set_text(status_label, buf);

                // Clear frame_ready flag to prevent timer from updating buffers during print
                tal_mutex_lock(sg_buffer_mutex);
                frame_ready                  = false;
                uint8_t current_buffer_index = write_buffer_index;
                tal_mutex_unlock(sg_buffer_mutex);

                // Get the YUV422 buffer with latest frame data
                uint8_t *yuv422_source = (current_buffer_index == 0) ? sg_yuv422_buffer_1 : sg_yuv422_buffer_2;

                // Allocate printer bitmap buffer before calling callback
                int      bitmap_size    = (PRINT_WIDTH + 7) / 8 * PRINT_HEIGHT;
                uint8_t *printer_bitmap = (uint8_t *)tal_psram_malloc(bitmap_size);
                if (!printer_bitmap) {
                    printf("Failed to allocate printer bitmap buffer (%d bytes)\n", bitmap_size);
                    rt = camera_start();
                    if (rt != OPRT_OK) {
                        printf("Failed to restart camera: %d\n", rt);
                    }
                } else {
                    YUV422_TO_BINARY_PARAMS_T print_params = {
                        .yuv422_data   = yuv422_source, // Direct pointer, no copy
                        .src_width     = CAMERA_WIDTH,
                        .src_height    = CAMERA_HEIGHT,
                        .binary_data   = printer_bitmap, // Pre-allocated buffer
                        .dst_width     = PRINT_WIDTH,
                        .dst_height    = PRINT_HEIGHT,
                        .config        = &sg_binary_config, // Use global config directly
                        .invert_colors = 0                  // Will be overridden by printer
                    };
                    sg_print_callback(&print_params); // Call print callback
                    tal_psram_free(printer_bitmap);
                    printf("bitmap_size %d bytes free succeed\n", bitmap_size);
                }
            } else {
                printf("ENTER key pressed but callback not ready\n");
            }
        } else {
            // Camera is stopped: restart
            printf("ENTER pressed: Restarting camera\n");
            rt = camera_start();
            if (rt != OPRT_OK) {
                printf("Failed to restart camera: %d\n", rt);
            }
        }
#endif
        break;

    case KEY_ESC:
        screen_back();
        break;

    default:
        break;
    }
}

/**
 * @brief Initialize camera screen
 */
void camera_screen_init(void)
{
    printf("[%s] Initializing camera screen\n", camera_screen.name);

    // Create full screen container (must be full screen for screen object)
    // But set it completely transparent so it doesn't interfere with camera area
    ui_camera_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_camera_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    // lv_obj_set_style_bg_color(ui_camera_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_camera_screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_camera_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_camera_screen, 0, 0);
    lv_obj_clear_flag(ui_camera_screen, LV_OBJ_FLAG_SCROLLABLE);

#ifdef ENABLE_LVGL_HARDWARE
    camera_canvas = lv_canvas_create(ui_camera_screen);
    lv_obj_set_pos(camera_canvas, 0, 0);
    lv_obj_set_size(camera_canvas, CAMERA_AREA_WIDTH, CAMERA_AREA_HEIGHT);

    // Allocate canvas buffer for monochrome 1-bit indexed image
    // For LVGL v9: I1 format needs palette (8 bytes) + bitmap data
    uint32_t bitmap_size     = ((CAMERA_AREA_WIDTH + 7) / 8) * CAMERA_AREA_HEIGHT;
    uint32_t canvas_buf_size = bitmap_size + 8; // 8 bytes for 2-color palette (2 * 4 bytes)
    canvas_buffer            = (uint8_t *)tal_psram_malloc(canvas_buf_size);
    if (canvas_buffer) {
        // Initialize buffer: clear palette area and fill bitmap with test pattern
        memset(canvas_buffer, 0, 8);                  // Clear palette area
        memset(canvas_buffer + 8, 0x00, bitmap_size); // Initialize bitmap (0x00 = all index 0)

        lv_canvas_set_buffer(camera_canvas, canvas_buffer, CAMERA_AREA_WIDTH, CAMERA_AREA_HEIGHT, LV_COLOR_FORMAT_I1);

        // Set palette: LVGL I1 format: bit=0->palette[0], bit=1->palette[1]
        // Our logic: luminance >= threshold -> bit=1 (bright/white)
        //            luminance < threshold -> bit=0 (dark/black)
        // Therefore: palette[0]=black (for bit=0), palette[1]=white (for bit=1)
        lv_canvas_set_palette(camera_canvas, 0, lv_color32_make(0x00, 0x00, 0x00, 0xFF)); // black for bit=0
        lv_canvas_set_palette(camera_canvas, 1, lv_color32_make(0xFF, 0xFF, 0xFF, 0xFF)); // white for bit=1

        // PR_NOTICE("Canvas initialized: %dx%d, buffer size=%d (palette:8 + bitmap:%d)", CAMERA_AREA_WIDTH,
        //           CAMERA_AREA_HEIGHT, canvas_buf_size, bitmap_size);
    } else {
        PR_ERR("Failed to allocate canvas buffer");
    }
#endif

    // Info display area positioned at right side ONLY
    lv_obj_t *info_panel = lv_obj_create(ui_camera_screen);
    lv_obj_set_pos(info_panel, INFO_AREA_X, 0); // Position at x=240 (right side)
    lv_obj_set_size(info_panel, INFO_AREA_WIDTH, INFO_AREA_HEIGHT);
    lv_obj_set_style_bg_color(info_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(info_panel, 2, 0);
    lv_obj_set_style_border_color(info_panel, lv_color_black(), 0);
    lv_obj_set_style_pad_all(info_panel, 0, 0);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Method label
    method_label = lv_label_create(info_panel);
    lv_obj_set_pos(method_label, 10, 10);
    lv_obj_set_width(method_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(method_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(method_label, SCREEN_CONTENT_FONT, 0);

    // Threshold label
    threshold_label = lv_label_create(info_panel);
    lv_obj_set_pos(threshold_label, 10, 60);
    lv_obj_set_width(threshold_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(threshold_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(threshold_label, SCREEN_CONTENT_FONT, 0);

    // Status label
    status_label = lv_label_create(info_panel);
    lv_obj_set_pos(status_label, 10, 110);
    lv_obj_set_width(status_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(status_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(status_label, SCREEN_CONTENT_FONT, 0);

#ifdef ENABLE_LVGL_HARDWARE
    // Initialize camera hardware
    OPERATE_RET rt = camera_init();
    if (OPRT_OK != rt) {
        PR_ERR("Camera initialization failed: %d", rt);
    } else {
        // Auto-start camera
        camera_start();
    }
#endif

    // Create 100ms timer for updating display
    update_timer = lv_timer_create(update_timer_cb, 20, NULL);

    // Add keyboard event callback
    lv_obj_add_event_cb(ui_camera_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_camera_screen);
    lv_group_focus_obj(ui_camera_screen);

    printf("[%s] Camera screen initialized\n", camera_screen.name);

    // Notify lifecycle callback
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(TRUE);
    }
}

/**
 * @brief Deinitialize camera screen
 */
void camera_screen_deinit(void)
{
    printf("[%s] Deinitializing camera screen\n", camera_screen.name);

    // Notify lifecycle callback
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(FALSE);
    }

    // Delete timer
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = NULL;
    }

#ifdef ENABLE_LVGL_HARDWARE
    // Stop camera if running
    camera_stop();

    // Free YUV422 buffers
    if (sg_yuv422_buffer_1) {
        tal_psram_free(sg_yuv422_buffer_1);
        sg_yuv422_buffer_1 = NULL;
    }
    if (sg_yuv422_buffer_2) {
        tal_psram_free(sg_yuv422_buffer_2);
        sg_yuv422_buffer_2 = NULL;
    }
    sg_yuv422_write_buffer = NULL;

    // Clean up frame buffers
    if (sg_p_display_fb_1) {
        tdl_disp_free_frame_buff(sg_p_display_fb_1);
        sg_p_display_fb_1 = NULL;
    }
    if (sg_p_display_fb_2) {
        tdl_disp_free_frame_buff(sg_p_display_fb_2);
        sg_p_display_fb_2 = NULL;
    }
    sg_p_display_fb = NULL;

    // Free canvas buffer
    if (canvas_buffer) {
        tal_psram_free(canvas_buffer);
        canvas_buffer = NULL;
    }

    // Release mutex
    if (sg_buffer_mutex) {
        tal_mutex_release(sg_buffer_mutex);
        sg_buffer_mutex = NULL;
    }
#endif

    // Remove event callback and clean up UI
    if (ui_camera_screen) {
        lv_obj_remove_event_cb(ui_camera_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_camera_screen);
    }

    printf("[%s] Camera screen deinitialized\n", camera_screen.name);
}
