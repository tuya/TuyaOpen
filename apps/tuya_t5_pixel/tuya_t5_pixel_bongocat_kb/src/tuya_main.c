#include "tal_api.h"
#include "tkl_output.h"
#include "tal_uart.h"
#include "board_com_api.h"
#include "board_pixel_api.h"
#include "tdl_button_manage.h"
#include "pixel_art/pixel_art_types.h"
#include "pixel_art/resource/cute_cat_white.h"
#include "pixel_art/resource/wooden_block.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/***********************************************************
************************macro define************************
***********************************************************/

#define LED_PIXELS_TOTAL_NUM 1027
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.05f // 5% brightness

// Serial command definitions
#define KEY_UP             0x00
#define KEY_DOWN           0x01
#define SERIAL_BUFFER_SIZE 32

// UART configuration
#define USR_UART_NUM TUYA_UART_NUM_0

/***********************************************************
***********************variable define**********************
***********************************************************/

static PIXEL_HANDLE_T g_pixels_handle = NULL;
static THREAD_HANDLE g_pixels_thrd = NULL;
static volatile bool g_animation_running = false;

// Animation mode
typedef enum {
    MODE_CUTE_CAT,    // Cute cat white mode
    MODE_WOODEN_BLOCK // Wooden block mode
} anim_mode_t;

static volatile anim_mode_t g_anim_mode = MODE_CUTE_CAT;

// Animation state
// For cute_cat: 0 = frame 1, 1 = frame 2 (hands up), 2 = frame 3
// For wooden_block: 0 = frame 0 (up), 4 = frame 4 (down, 5th frame)
static volatile uint32_t g_current_frame = 1; // Start with frame 2 (hands up) for cute_cat

// Button handle
static TDL_BUTTON_HANDLE g_button_ok_handle = NULL;

// Serial input buffer
static uint8_t g_serial_buffer[SERIAL_BUFFER_SIZE];
static uint32_t g_serial_buffer_pos = 0;

// Counter for 0x01 key down events in wooden block mode
static volatile uint32_t g_key_down_count = 0;

/***********************************************************
********************function declaration********************
***********************************************************/

static void pixel_led_animation_task(void *args);
static OPERATE_RET pixel_led_init(void);
static void render_pixel_art_frame(const pixel_art_t *art, uint32_t frame_index);
static void handle_serial_command(uint8_t key_state);
static void echo_hex_loopback(const uint8_t *data, uint32_t len);
static void button_ok_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void init_buttons(void);

/***********************************************************
********************function implementation*****************
***********************************************************/

/**
 * @brief Render a single frame of pixel art (generic function)
 * Uses frame buffer to support text overlays
 */
static void render_pixel_art_frame(const pixel_art_t *art, uint32_t frame_index)
{
    if (g_pixels_handle == NULL || art == NULL) {
        return;
    }

    // Clamp frame index to valid range
    if (frame_index >= art->frame_count) {
        frame_index = 0;
    }

    // Create frame buffer for rendering (with text overlays)
    PIXEL_FRAME_HANDLE_T frame = board_pixel_frame_create();
    if (frame == NULL) {
        PR_ERR("Failed to create frame buffer");
        return;
    }

    // Clear frame
    board_pixel_frame_clear(frame);

    // Get current frame
    const pixel_frame_t *pixel_frame = &art->frames[frame_index];
    const pixel_rgb_t *pixels = pixel_frame->pixels;

    // Calculate Y offset to align to bottom if art height is less than matrix height (32)
    // For wooden_block (32x25), offset = 32 - 25 = 7, so it aligns to bottom
    uint32_t y_offset = 0;
    if (pixel_frame->height < 32) {
        y_offset = 32 - pixel_frame->height;
    }

    // Convert pixel art to RGB bitmap format (row-major, RGB interleaved)
    // Allocate buffer for bitmap data
    uint8_t *bitmap_data = (uint8_t *)tal_malloc(pixel_frame->width * pixel_frame->height * 3);
    if (bitmap_data == NULL) {
        PR_ERR("Failed to allocate bitmap buffer");
        board_pixel_frame_destroy(frame);
        return;
    }

    // Convert pixel art data to bitmap format
    for (uint32_t y = 0; y < pixel_frame->height; y++) {
        for (uint32_t x = 0; x < pixel_frame->width; x++) {
            uint32_t pixel_idx = y * pixel_frame->width + x;
            uint32_t bitmap_idx = (y * pixel_frame->width + x) * 3;

            if (pixel_idx < (pixel_frame->width * pixel_frame->height) &&
                bitmap_idx < (pixel_frame->width * pixel_frame->height * 3)) {
                bitmap_data[bitmap_idx] = pixels[pixel_idx].r;
                bitmap_data[bitmap_idx + 1] = pixels[pixel_idx].g;
                bitmap_data[bitmap_idx + 2] = pixels[pixel_idx].b;
            }
        }
    }

    // Draw bitmap to frame buffer at position (0, y_offset) to align to bottom
    board_pixel_draw_bitmap(frame, 0, y_offset, bitmap_data, pixel_frame->width, pixel_frame->height);

    // Free bitmap buffer
    tal_free(bitmap_data);

    // For wooden block mode, add text overlays
    if (g_anim_mode == MODE_WOODEN_BLOCK) {
        // Top-left: Counter (blue) - always visible, moved down by 6px
        char count_str[16];
        if (g_key_down_count > 9999) {
            snprintf(count_str, sizeof(count_str), "9999+");
        } else {
            snprintf(count_str, sizeof(count_str), "%u", g_key_down_count);
        }
        board_pixel_draw_text(frame, 0, 6, count_str, PIXEL_COLOR_BLUE, PIXEL_FONT_PICOPIXEL);

        // Top-right: "+1" text (red) - only on frame 4 (index 4, which is the 5th frame), moved down by 6px
        // Picopixel font is small (~4px per char), so "+1" is ~8px, position at x=26 for right alignment
        if (frame_index == 4) {
            board_pixel_draw_text(frame, 26, 6, "+1", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
        }
    }

    // Render frame to LED matrix
    board_pixel_frame_render(frame);

    // Destroy frame buffer
    board_pixel_frame_destroy(frame);
}

/**
 * @brief Handle serial command
 * @param key_state 0x00 = key up, 0x01 = key down
 */
static void handle_serial_command(uint8_t key_state)
{
    if (g_anim_mode == MODE_CUTE_CAT) {
        // Cute cat mode (inverted logic)
        if (key_state == KEY_UP) { // 0x00 - Randomly show frame 1 or 3
            // Randomly choose frame 1 (index 0) or frame 3 (index 2)
            uint32_t random_choice = rand() % 2;
            if (random_choice == 0) {
                g_current_frame = 0; // Frame 1
                PR_NOTICE("Serial command: Key UP - Randomly show frame 1");
            } else {
                g_current_frame = 2; // Frame 3
                PR_NOTICE("Serial command: Key UP - Randomly show frame 3");
            }
        } else if (key_state == KEY_DOWN) { // 0x01 - Show frame 2 (hands up)
            // Frame 2 is at index 1 (0-indexed)
            g_current_frame = 1;
            PR_NOTICE("Serial command: Key DOWN - Show frame 2 (hands up)");
        }
    } else if (g_anim_mode == MODE_WOODEN_BLOCK) {
        // Wooden block mode
        if (key_state == KEY_UP) { // 0x00 - Show frame 0 (up)
            g_current_frame = 0;
            PR_NOTICE("Serial command: Key UP - Show wooden block frame 0 (up)");
        } else if (key_state == KEY_DOWN) { // 0x01 - Show frame 4 (down, 5th frame)
            g_current_frame = 4;            // Frame 4 is the 5th frame (0-indexed)
            // Increment counter (cap at 9999)
            if (g_key_down_count < 9999) {
                g_key_down_count++;
            }
            PR_NOTICE("Serial command: Key DOWN - Show wooden block frame 4 (down), count: %u", g_key_down_count);
        }
    }
}

/**
 * @brief Initialize pixel LED driver using BSP
 */
static OPERATE_RET pixel_led_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_system_sleep(100);
    rt = board_pixel_get_handle(&g_pixels_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to get pixel device handle: %d", rt);
        return rt;
    }

    PR_NOTICE("Pixel LED initialized: %d pixels", LED_PIXELS_TOTAL_NUM);
    return rt;
}

/**
 * @brief Pixel LED animation task thread
 * Continuously loops and renders the current frame (no delays)
 */
static void pixel_led_animation_task(void *args)
{
    g_animation_running = true;
    PR_NOTICE("Pixel LED animation task started");

    while (g_animation_running) {
        // Continuously render the current frame (no delays, immediate switching)
        const pixel_art_t *current_art = (g_anim_mode == MODE_CUTE_CAT) ? &cute_cat_white : &wooden_block;
        render_pixel_art_frame(current_art, g_current_frame);

        // Small sleep to prevent CPU spinning, but keep it minimal for immediate response
        tal_system_sleep(10);
    }

    PR_NOTICE("Pixel LED animation task stopped");
    g_pixels_thrd = NULL;
}

/**
 * @brief Echo received data as hex string via UART
 */
static void echo_hex_loopback(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    // Format: "RX: 0xXX 0xYY ...\r\n"
    // Maximum: "RX: " + (len * 5) + "\r\n" = 4 + len*5 + 2
    char hex_buffer[256]; // Enough for up to 50 bytes
    char *p = hex_buffer;

    p += sprintf(p, "RX: ");

    for (uint32_t i = 0; i < len && (p - hex_buffer) < (sizeof(hex_buffer) - 10); i++) {
        p += sprintf(p, "0x%02X ", data[i]);
    }

    p += sprintf(p, "\r\n");

    // Send hex loopback
    tal_uart_write(USR_UART_NUM, (const uint8_t *)hex_buffer, p - hex_buffer);
}

/**
 * @brief Serial input processing task (runs in main loop)
 * This function should be called periodically to read serial input
 */
static void serial_input_task(void)
{
    // Read from UART0
    int read_len =
        tal_uart_read(USR_UART_NUM, g_serial_buffer + g_serial_buffer_pos, SERIAL_BUFFER_SIZE - g_serial_buffer_pos);

    if (read_len > 0) {
        // Echo received data as hex
        echo_hex_loopback(g_serial_buffer + g_serial_buffer_pos, read_len);

        g_serial_buffer_pos += read_len;

        // Process if we have at least 1 byte (key up/down command)
        if (g_serial_buffer_pos >= 1) {
            handle_serial_command(g_serial_buffer[0]);
            // Reset buffer after processing
            g_serial_buffer_pos = 0;
            memset(g_serial_buffer, 0, sizeof(g_serial_buffer));
        }

        // Prevent buffer overflow
        if (g_serial_buffer_pos >= SERIAL_BUFFER_SIZE) {
            g_serial_buffer_pos = 0;
            memset(g_serial_buffer, 0, sizeof(g_serial_buffer));
        }
    } else if (read_len < 0) {
        // UART read error, reset buffer
        g_serial_buffer_pos = 0;
        memset(g_serial_buffer, 0, sizeof(g_serial_buffer));
    }
}

/**
 * @brief OK button callback - switches between cute cat and wooden block modes
 */
static void button_ok_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        // Switch mode
        if (g_anim_mode == MODE_CUTE_CAT) {
            g_anim_mode = MODE_WOODEN_BLOCK;
            g_current_frame = 0; // Start with frame 0 (up) for wooden block
            PR_NOTICE("OK Button: Switched to wooden block mode (frame 0 - up)");
        } else {
            g_anim_mode = MODE_CUTE_CAT;
            g_current_frame = 1; // Start with frame 2 (hands up) for cute cat
            PR_NOTICE("OK Button: Switched to cute cat mode (frame 2 - hands up)");
        }
    }
}

/**
 * @brief Initialize buttons and register callbacks
 */
static void init_buttons(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 2000, // 2 seconds for long press
                                   .long_keep_timer = 500,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};

    // Initialize OK button
    rt = tdl_button_create(BUTTON_NAME, &button_cfg, &g_button_ok_handle);
    if (OPRT_OK == rt) {
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_ok_cb);
        PR_NOTICE("OK button initialized");
    } else {
        PR_ERR("Failed to create OK button: %d", rt);
    }
}

/**
 * @brief Main user function
 */
static void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("==========================================");
    PR_NOTICE("Tuya T5AI Pixel BongoCat KB Demo");
    PR_NOTICE("==========================================");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("==========================================");

    // Initialize hardware
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }
    PR_NOTICE("Hardware initialized");

    // Initialize buttons
    init_buttons();

    // Initialize UART0 for serial input
    TAL_UART_CFG_T uart_cfg = {0};
    uart_cfg.base_cfg.baudrate = 115200;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.open_mode = O_BLOCK;

    rt = tal_uart_init(USR_UART_NUM, &uart_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("UART initialization failed: %d", rt);
        // Continue anyway, but serial input won't work
    } else {
        PR_NOTICE("UART0 initialized successfully (115200 baud)");
    }

    // Initialize pixel LED
    rt = pixel_led_init();
    if (OPRT_OK == rt) {
        PR_NOTICE("Pixel LED initialized successfully");

        // Start pixel LED animation thread
        THREAD_CFG_T thrd_param = {0};
        thrd_param.stackDepth = 1024 * 4;
        thrd_param.priority = THREAD_PRIO_2;
        thrd_param.thrdname = "pixel_anim";

        rt = tal_thread_create_and_start(&g_pixels_thrd, NULL, NULL, pixel_led_animation_task, NULL, &thrd_param);
        if (OPRT_OK == rt) {
            PR_NOTICE("Pixel LED animation thread started");
        } else {
            PR_ERR("Failed to start pixel LED animation thread: %d", rt);
        }
    } else {
        PR_ERR("Pixel LED initialization failed: %d", rt);
    }

    // Initialize random seed
    srand(tal_system_get_millisecond());

    PR_NOTICE("==========================================");
    PR_NOTICE("Demo Ready!");
    PR_NOTICE("==========================================");
    PR_NOTICE("Controls:");
    PR_NOTICE("  OK Button: Switch between cute cat and wooden block modes");
    PR_NOTICE("Serial Commands:");
    PR_NOTICE("  0x00 - Key UP");
    PR_NOTICE("  0x01 - Key DOWN");
    PR_NOTICE("  Cute Cat Mode:");
    PR_NOTICE("    0x00 (UP) - Randomly show frame 1 or 3");
    PR_NOTICE("    0x01 (DOWN) - Show frame 2 (hands up)");
    PR_NOTICE("  Wooden Block Mode:");
    PR_NOTICE("    0x00 (UP) - Show frame 0 (up)");
    PR_NOTICE("    0x01 (DOWN) - Show frame 4 (down)");
    PR_NOTICE("==========================================");

    // Main loop - process serial input
    int cnt = 0;
    while (1) {
        // Process serial input periodically
        serial_input_task();

        // Print status every 10 seconds
        if (cnt % 100 == 0) {
            PR_DEBUG("Demo running... (count: %d, frame: %d)", cnt, g_current_frame);
        }
        tal_system_sleep(100);
        cnt++;
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
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
