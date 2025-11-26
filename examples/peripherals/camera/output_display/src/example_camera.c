/**
 * @file example_camera.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tkl_dma2d.h"
#endif

#include "board_com_api.h"

#include "tdl_display_manage.h"
#include "tdl_display_draw.h"
#include "tdl_camera_manage.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define DEFAULT_FIXED_THRESHOLD 128 // Default fixed threshold value

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Binary conversion method types
 */
typedef enum {
    BINARY_METHOD_FIXED,    // Fixed threshold (user-defined)
    BINARY_METHOD_ADAPTIVE, // Adaptive threshold (average-based)
    BINARY_METHOD_OTSU,     // Otsu's method (automatic optimal threshold)
} BINARY_METHOD_E;

/**
 * @brief Binary conversion configuration
 */
typedef struct {
    BINARY_METHOD_E method;  // Conversion method
    uint8_t fixed_threshold; // Fixed threshold value (0-255), used when method is BINARY_METHOD_FIXED
} BINARY_CONFIG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_2 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_rotat = NULL;

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;

// Binary conversion configuration
static BINARY_CONFIG_T sg_binary_config = {
    .method = BINARY_METHOD_ADAPTIVE,
    .fixed_threshold = DEFAULT_FIXED_THRESHOLD,
};

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TKL_DMA2D_FRAME_INFO_T sg_in_frame = {0};
static TKL_DMA2D_FRAME_INFO_T sg_out_frame = {0};
static SEM_HANDLE sg_convert_sem;
#endif
/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static void __dma2d_irq_cb(TUYA_DMA2D_IRQ_E type, VOID_T *args)
{
    if (sg_convert_sem) {
        tal_semaphore_post(sg_convert_sem);
    }
}

static OPERATE_RET __dma2d_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_convert_sem, 0, 1));

    TUYA_DMA2D_BASE_CFG_T dma2d_cfg = {
        .cb = __dma2d_irq_cb,
        .arg = NULL,
    };

    return tkl_dma2d_init(&dma2d_cfg);
}
#endif

OPERATE_RET __get_camera_raw_frame_rgb565_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_p_display_fb) {
        return OPRT_COM_ERROR;
    }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL;

    sg_in_frame.type = TUYA_FRAME_FMT_YUV422;
    sg_in_frame.width = frame->width;
    sg_in_frame.height = frame->height;
    sg_in_frame.axis.x_axis = 0;
    sg_in_frame.axis.y_axis = 0;
    sg_in_frame.width_cp = 0;
    sg_in_frame.height_cp = 0;
    sg_in_frame.pbuf = frame->data;

    sg_out_frame.type = TUYA_FRAME_FMT_RGB565;
    sg_out_frame.width = sg_p_display_fb->width;
    sg_out_frame.height = sg_p_display_fb->height;
    sg_out_frame.axis.x_axis = 0;
    sg_out_frame.axis.y_axis = 0;
    sg_out_frame.width_cp = 0;
    sg_out_frame.height_cp = 0;
    sg_out_frame.pbuf = sg_p_display_fb->frame;

    TUYA_CALL_ERR_RETURN(tkl_dma2d_convert(&sg_in_frame, &sg_out_frame));

    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_convert_sem, 100));

    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        tdl_disp_draw_rotate(sg_display_info.rotation, sg_p_display_fb, sg_p_display_fb_rotat, sg_display_info.is_swap);
        target_fb = sg_p_display_fb_rotat;
    } else {
        if (true == sg_display_info.is_swap) {
            tdl_disp_dev_rgb565_swap((uint16_t *)sg_p_display_fb->frame, sg_p_display_fb->len / 2);
        }
        target_fb = sg_p_display_fb;
    }

    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

    sg_p_display_fb = (sg_p_display_fb == sg_p_display_fb_1) ? sg_p_display_fb_2 : sg_p_display_fb_1;
#endif

    return rt;
}

/**
 * @brief Converts YUV422 image to binary image using luminance threshold.
 *
 * @param yuv422_data Pointer to YUV422 input data (YUYV format).
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param binary_data Pointer to output binary data buffer.
 * @param threshold Luminance threshold (0-255), pixels >= threshold become white (1).
 * @return int 0 on success, -1 on failure.
 */
int yuv422_to_binary(uint8_t *yuv422_data, int width, int height, uint8_t *binary_data, uint8_t threshold)
{
    if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
        return -1;
    }

    int binary_stride = (width + 7) / 8; // Bytes per row for packed binary data

    // Initialize buffer to 0xFF (all white, since bit=0 means white on this display)
    memset(binary_data, 0xFF, binary_stride * height);

    // Process YUV422 data (YUYV format: Y0 U Y1 V)
    // YUV422 packing: [Y0][U0][Y1][V0] [Y2][U1][Y3][V1] ...
    // Y values are at byte positions: 0, 2, 4, 6, 8, 10...
    // U values are at byte positions: 1, 5, 9, 13...
    // V values are at byte positions: 3, 7, 11, 15...
    //
    // NOTE: If format is actually UYVY (U Y V Y), then:
    // Y values would be at: 1, 3, 5, 7, 9, 11...
    for (int y = 0; y < height; y++) {
        int row_offset = y * width * 2; // Each row has width*2 bytes
        for (int x = 0; x < width; x++) {
            // Try UYVY format first (Y at odd positions: 1, 3, 5, 7...)
            int yuv_index = row_offset + x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index]; // Y component

            // Apply threshold
            // Note: Display hardware uses inverted logic - bit=1 is black, bit=0 is white
            // So: luminance >= threshold means white, clear the bit (set to 0)
            //     luminance < threshold means black, keep the bit (leave as 1)
            if (luminance >= threshold) {
                // Set corresponding bit in binary data
                int binary_index = y * binary_stride + x / 8;
                int bit_position = x % 8;
                binary_data[binary_index] &= ~(1 << bit_position);
            }
            // Else: luminance < threshold, leave bit as 1 for black pixel
        }
    }

    return 0;
}

/**
 * @brief Calculate adaptive threshold using average luminance.
 *
 * @param yuv422_data Pointer to YUV422 input data.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @return uint8_t Calculated threshold value.
 */
static uint8_t __calculate_adaptive_threshold(uint8_t *yuv422_data, int width, int height)
{
    uint32_t luminance_sum = 0;
    int total_pixels = width * height;

    for (int y = 0; y < height; y++) {
        int row_offset = y * width * 2;
        for (int x = 0; x < width; x++) {
            int yuv_index = row_offset + x * 2 + 1; // UYVY format: Y at odd positions
            luminance_sum += yuv422_data[yuv_index];
        }
    }

    return (uint8_t)(luminance_sum / total_pixels);
}

/**
 * @brief Calculate optimal threshold using Otsu's method.
 *
 * Otsu's method automatically calculates the optimal threshold by maximizing
 * the between-class variance of the binary image.
 *
 * @param yuv422_data Pointer to YUV422 input data.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @return uint8_t Calculated optimal threshold value.
 */
static uint8_t __calculate_otsu_threshold(uint8_t *yuv422_data, int width, int height)
{
    int histogram[256] = {0};
    int total_pixels = width * height;

    // Step 1: Build histogram
    for (int y = 0; y < height; y++) {
        int row_offset = y * width * 2;
        for (int x = 0; x < width; x++) {
            int yuv_index = row_offset + x * 2 + 1; // UYVY format: Y at odd positions
            uint8_t luminance = yuv422_data[yuv_index];
            histogram[luminance]++;
        }
    }

    // Step 2: Calculate total mean
    float sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += i * histogram[i];
    }

    // Step 3: Find threshold with maximum between-class variance
    float sum_background = 0;
    int weight_background = 0;
    float max_variance = 0;
    uint8_t optimal_threshold = 0;

    for (int t = 0; t < 256; t++) {
        weight_background += histogram[t];
        if (weight_background == 0)
            continue;

        int weight_foreground = total_pixels - weight_background;
        if (weight_foreground == 0)
            break;

        sum_background += t * histogram[t];

        float mean_background = sum_background / weight_background;
        float mean_foreground = (sum - sum_background) / weight_foreground;

        // Calculate between-class variance
        float variance = (float)weight_background * weight_foreground * (mean_background - mean_foreground) *
                         (mean_background - mean_foreground);

        if (variance > max_variance) {
            max_variance = variance;
            optimal_threshold = t;
        }
    }

    return optimal_threshold;
}

int yuv422_to_binary_adaptive(uint8_t *yuv422_data, int width, int height, uint8_t *binary_data)
{
    if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
        return -1;
    }

    uint8_t avg_threshold = __calculate_adaptive_threshold(yuv422_data, width, height);

    // Apply adaptive threshold
    return yuv422_to_binary(yuv422_data, width, height, binary_data, avg_threshold);
}

/**
 * @brief Convert YUV422 to binary using Otsu's method.
 *
 * @param yuv422_data Pointer to YUV422 input data.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param binary_data Pointer to output binary data buffer.
 * @return int 0 on success, -1 on failure.
 */
int yuv422_to_binary_otsu(uint8_t *yuv422_data, int width, int height, uint8_t *binary_data)
{
    if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
        return -1;
    }

    uint8_t otsu_threshold = __calculate_otsu_threshold(yuv422_data, width, height);

    PR_DEBUG("Otsu threshold calculated: %d", otsu_threshold);

    // Apply Otsu threshold
    return yuv422_to_binary(yuv422_data, width, height, binary_data, otsu_threshold);
}

/**
 * @brief Convert YUV422 to binary using configured method.
 *
 * @param yuv422_data Pointer to YUV422 input data.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param binary_data Pointer to output binary data buffer.
 * @param config Pointer to binary conversion configuration.
 * @return int 0 on success, -1 on failure.
 */
int yuv422_to_binary_with_config(uint8_t *yuv422_data, int width, int height, uint8_t *binary_data,
                                 BINARY_CONFIG_T *config)
{
    if (!yuv422_data || !binary_data || !config || width <= 0 || height <= 0) {
        return -1;
    }

    switch (config->method) {
    case BINARY_METHOD_FIXED:
        return yuv422_to_binary(yuv422_data, width, height, binary_data, config->fixed_threshold);

    case BINARY_METHOD_ADAPTIVE:
        return yuv422_to_binary_adaptive(yuv422_data, width, height, binary_data);

    case BINARY_METHOD_OTSU:
        return yuv422_to_binary_otsu(yuv422_data, width, height, binary_data);

    default:
        PR_ERR("Unknown binary conversion method: %d", config->method);
        return -1;
    }
}

OPERATE_RET __get_camera_raw_frame_mono_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL;

    if (NULL == hdl || NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == sg_p_display_fb) {
        return OPRT_COM_ERROR;
    }

    // Use configured binary conversion method
    yuv422_to_binary_with_config(frame->data, frame->width, frame->height, sg_p_display_fb->frame, &sg_binary_config);

    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        tdl_disp_draw_rotate(sg_display_info.rotation, sg_p_display_fb, sg_p_display_fb_rotat, sg_display_info.is_swap);
        target_fb = sg_p_display_fb_rotat;
    } else {
        target_fb = sg_p_display_fb;
    }

    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

    return OPRT_OK;
}

/**
 * @brief Set binary conversion method.
 *
 * @param method Binary conversion method to use.
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET set_binary_method(BINARY_METHOD_E method)
{
    if (method >= BINARY_METHOD_FIXED && method <= BINARY_METHOD_OTSU) {
        sg_binary_config.method = method;
        PR_NOTICE("Binary method set to: %d", method);
        return OPRT_OK;
    }

    PR_ERR("Invalid binary method: %d", method);
    return OPRT_INVALID_PARM;
}

/**
 * @brief Set fixed threshold value.
 *
 * @param threshold Threshold value (0-255).
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET set_fixed_threshold(uint8_t threshold)
{
    sg_binary_config.fixed_threshold = threshold;
    PR_NOTICE("Fixed threshold set to: %d", threshold);
    return OPRT_OK;
}

/**
 * @brief Get current binary conversion configuration.
 *
 * @param config Pointer to receive configuration.
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET get_binary_config(BINARY_CONFIG_T *config)
{
    if (!config) {
        return OPRT_INVALID_PARM;
    }

    memcpy(config, &sg_binary_config, sizeof(BINARY_CONFIG_T));
    return OPRT_OK;
}

static OPERATE_RET __display_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t frame_len = 0;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info));

    if (sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565 && sg_display_info.fmt != TUYA_PIXEL_FMT_MONOCHROME) {
        PR_ERR("display pixel format %d not supported", sg_display_info.fmt);
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(sg_tdl_disp_hdl));

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    /*create frame buffer*/
    if (sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        frame_len = (EXAMPLE_CAMERA_WIDTH + 7) / 8 * EXAMPLE_CAMERA_HEIGHT;
    } else {
        frame_len = EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2; // RGB565 is 2 bytes per pixel
    }
    sg_p_display_fb_1 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_1) {
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_1->fmt = sg_display_info.fmt;
    sg_p_display_fb_1->width = EXAMPLE_CAMERA_WIDTH;
    sg_p_display_fb_1->height = EXAMPLE_CAMERA_HEIGHT;

    sg_p_display_fb_2 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_2) {
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_2->fmt = sg_display_info.fmt;
    sg_p_display_fb_2->width = EXAMPLE_CAMERA_WIDTH;
    sg_p_display_fb_2->height = EXAMPLE_CAMERA_HEIGHT;

    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        sg_p_display_fb_rotat = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
        if (NULL == sg_p_display_fb_rotat) {
            PR_ERR("create display frame buff failed");
            return OPRT_MALLOC_FAILED;
        }
        sg_p_display_fb_rotat->fmt = sg_display_info.fmt;
        sg_p_display_fb_rotat->width = EXAMPLE_CAMERA_WIDTH;
        sg_p_display_fb_rotat->height = EXAMPLE_CAMERA_HEIGHT;
    }

    sg_p_display_fb = sg_p_display_fb_1;

    return OPRT_OK;
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    cfg.fps = EXAMPLE_CAMERA_FPS;
    cfg.width = EXAMPLE_CAMERA_WIDTH;
    cfg.height = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;

    if (sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        cfg.get_frame_cb = __get_camera_raw_frame_mono_cb;
    } else {
        cfg.get_frame_cb = __get_camera_raw_frame_rgb565_cb;
    }

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));

    PR_NOTICE("camera init success");

    return OPRT_OK;
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    TUYA_CALL_ERR_LOG(__dma2d_init());
#endif

    TUYA_CALL_ERR_LOG(__display_init());

    TUYA_CALL_ERR_LOG(__camera_init());

    // Configure binary conversion method
    if (sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        // You can change the method here:
        set_binary_method(BINARY_METHOD_FIXED);
        set_fixed_threshold(128);
        // set_binary_method(BINARY_METHOD_ADAPTIVE);
        // set_binary_method(BINARY_METHOD_OTSU);

        PR_NOTICE("Binary conversion initialized with method: %s",
                  sg_binary_config.method == BINARY_METHOD_FIXED      ? "FIXED"
                  : sg_binary_config.method == BINARY_METHOD_ADAPTIVE ? "ADAPTIVE"
                                                                      : "OTSU");
        if (sg_binary_config.method == BINARY_METHOD_FIXED) {
            PR_NOTICE("Fixed threshold: %d", sg_binary_config.fixed_threshold);
        }
    }

    while (1) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
