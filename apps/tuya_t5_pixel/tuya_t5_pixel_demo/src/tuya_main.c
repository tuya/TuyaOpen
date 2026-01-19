#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "board_com_api.h"
#include "board_pixel_api.h"
#include "board_buzzer_api.h"
#include "board_bmi270_api.h"
#include "tdl_button_manage.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "led_font.h"
#include "pixel_art/pixel_art_types.h"
#include "pixel_art/resource/laughing_cat.h"
#include "pixel_art/resource/rolling_cat.h"
#include "pixel_art/resource/super_mario_kart_mario.h"
#include "pixel_art/resource/cute_cat_white.h"
#include "pixel_art/resource/smallbwop_bwop.h"
#include "pixel_art/resource/wander.h"
#include "pixel_art/resource/Italian_Beach.h"
#include "pixel_art/resource/Italian_Pixel_Art.h"
#include "pixel_art/resource/Nintendo_Mario.h"
#include "pixel_art/resource/Cat_Meme.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define LED_PIXELS_TOTAL_NUM 1027
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.05f // 10% brightness

/***********************************************************
***********************variable define**********************
***********************************************************/

static TDL_BUTTON_HANDLE g_button_ok_handle = NULL;
static TDL_BUTTON_HANDLE g_button_a_handle = NULL;
static TDL_BUTTON_HANDLE g_button_b_handle = NULL;

static PIXEL_HANDLE_T g_pixels_handle = NULL;
static THREAD_HANDLE g_pixels_thrd = NULL;
static volatile uint32_t g_animation_mode = 0;
static volatile bool g_animation_loop = false;
static volatile bool g_animation_running = false;
static volatile uint32_t g_pixel_art_index = 0;

// Pixel art animation registration system
#define MAX_PIXEL_ART_ANIMATIONS 16
static const pixel_art_t *g_registered_pixel_arts[MAX_PIXEL_ART_ANIMATIONS] = {NULL};
static uint32_t g_registered_pixel_art_count = 0;
#define EFFECT_ANIMATION_COUNT 9      // Number of effect animations (0-8)
#define SAND_PHYSICS_MODE      0xFFFF // Special mode for sand physics demo

// Sand physics system
#define MAX_SAND_PARTICLES 512 // 50% of 1024 total pixels
#define SAND_SPAWN_RATE_MS 100 // 10 particles per second (faster feeding)
#define MATRIX_WIDTH       32
#define MATRIX_HEIGHT      32
#define GRAVITY_SCALE      0.5f // Scale factor for gravity from accelerometer

typedef struct {
    float x;         // X position (0-31)
    float y;         // Y position (0-31)
    float vx;        // X velocity
    float vy;        // Y velocity
    uint8_t r, g, b; // Particle color
    bool active;     // Whether particle is active
} sand_particle_t;

static sand_particle_t g_sand_particles[MAX_SAND_PARTICLES];
static uint32_t g_last_sand_spawn_time = 0;
static bmi270_dev_t *g_bmi270_dev = NULL;
static bool g_sand_initialized = false;

/***********************************************************
********************function declaration********************
***********************************************************/

static void buzzer_demo_play_startup_melody(void);
static void pixel_art_register_animation(const pixel_art_t *art);
static void pixel_art_init_registrations(void);
static void buzzer_demo_init_buttons(void);
static void buzzer_button_ok_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);

static void pixel_led_animation_task(void *args);
static OPERATE_RET pixel_led_init(void);
static void __breathing_color_effect(void);
static void __running_light_effect(void);
static void __color_wave_effect(void);
static void __2d_wave_effect(void);
static void __snowflake_effect(void);
static void __breathing_circle_effect(void);
static void __ripple_effect(void);
static void __scan_animation_effect(void);
static void __scrolling_text_effect(void);
static void render_char(int32_t x, int32_t y, char ch, float hue);
static void __pixel_art_effect(const pixel_art_t *art);
static void __sand_physics_effect(void);
static void sand_init_particle(sand_particle_t *particle);
static void sand_update_physics(void);
static void sand_render(void);

static void buzzer_button_a_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void buzzer_button_b_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Register a pixel art animation
 */
static void pixel_art_register_animation(const pixel_art_t *art)
{
    if (art == NULL) {
        PR_ERR("Cannot register NULL pixel art");
        return;
    }

    if (g_registered_pixel_art_count >= MAX_PIXEL_ART_ANIMATIONS) {
        PR_ERR("Maximum pixel art animations (%d) reached", MAX_PIXEL_ART_ANIMATIONS);
        return;
    }

    g_registered_pixel_arts[g_registered_pixel_art_count] = art;
    g_registered_pixel_art_count++;
    PR_NOTICE("Registered pixel art animation %d (frames: %d)", g_registered_pixel_art_count - 1, art->frame_count);
}

/**
 * @brief Initialize all pixel art animation registrations
 */
static void pixel_art_init_registrations(void)
{
    // Register all pixel art animations
    pixel_art_register_animation(&laughing_cat);
    pixel_art_register_animation(&rolling_cat);
    pixel_art_register_animation(&super_mario_kart_mario);
    pixel_art_register_animation(&cute_cat_white);
    pixel_art_register_animation(&smallbwop_bwop);
    pixel_art_register_animation(&wander);
    pixel_art_register_animation(&Italian_Beach);
    pixel_art_register_animation(&Italian_Pixel_Art);
    pixel_art_register_animation(&Nintendo_Mario);
    pixel_art_register_animation(&Cat_Meme);

    PR_NOTICE("Registered %d pixel art animations", g_registered_pixel_art_count);
}

/**
 * @brief Play a startup melody to demonstrate the buzzer
 */
static void buzzer_demo_play_startup_melody(void)
{
    PR_NOTICE("Playing startup melody...");

    // Play a simple melody: C5 -> E5 -> G5 (C major chord)
    board_buzzer_play_note_duration(NOTE_C5, 200);
    tal_system_sleep(50);
    board_buzzer_play_note_duration(NOTE_E5, 200);
    tal_system_sleep(50);
    board_buzzer_play_note_duration(NOTE_G5, 400);
    tal_system_sleep(100);

    PR_NOTICE("Startup melody complete");
}

/**
 * @brief Button OK callback - controls pixel LED animations
 */
static void buzzer_button_ok_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        // Single press: change to next animation
        uint32_t total_animations = EFFECT_ANIMATION_COUNT + g_registered_pixel_art_count;
        g_animation_mode = (g_animation_mode + 1) % total_animations;
        PR_NOTICE("OK Button: Changed to animation mode %d", g_animation_mode);
    } else if (event == TDL_BUTTON_PRESS_DOUBLE_CLICK) {
        // Double click: also change animation
        uint32_t total_animations = EFFECT_ANIMATION_COUNT + g_registered_pixel_art_count;
        g_animation_mode = (g_animation_mode + 1) % total_animations;
        PR_NOTICE("OK Button: Changed to animation mode %d", g_animation_mode);
    } else if (event == TDL_BUTTON_LONG_PRESS_START) {
        // Long press: toggle loop mode
        g_animation_loop = !g_animation_loop;
        PR_NOTICE("OK Button: Animation loop %s", g_animation_loop ? "enabled" : "disabled");
    } else if (event == TDL_BUTTON_PRESS_UP) {
        // Button release: no action
    }
}

/**
 * @brief Button A callback - switches between pixel art animations
 */
static void buzzer_button_a_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK || event == TDL_BUTTON_PRESS_DOUBLE_CLICK) {
        // Switch to next pixel art animation
        g_pixel_art_index = (g_pixel_art_index + 1) % g_registered_pixel_art_count;
        // Set animation mode to pixel art (starts after effect animations)
        g_animation_mode = EFFECT_ANIMATION_COUNT + g_pixel_art_index;
        PR_NOTICE("A Button: Changed to pixel art %d (mode %d)", g_pixel_art_index, g_animation_mode);
    }
}

/**
 * @brief Button B callback - plays Twinkle Twinkle Little Star
 */
static void buzzer_button_b_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    // Log ALL events to debug
    PR_NOTICE("B Button callback triggered! name=%s, event=%d", name ? name : "NULL", event);

    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        PR_NOTICE("B Button: Single click detected");

        // First, test with a simple tone to verify buzzer works
        PR_NOTICE("B Button: Testing buzzer with simple tone");
        OPERATE_RET rt = board_buzzer_play_note_duration(NOTE_C5, 200);
        if (OPRT_OK != rt) {
            PR_ERR("Failed to play test tone: %d", rt);
            return;
        }
        tal_system_sleep(50);

        // Stop any currently playing sequence
        if (board_buzzer_is_sequence_playing()) {
            PR_NOTICE("B Button: Stopping existing sequence");
            board_buzzer_stop_sequence();
            tal_system_sleep(200);
        }

        // Play the full Twinkle Twinkle Little Star melody using the sequencer
        PR_NOTICE("B Button: Playing Twinkle Twinkle Little Star");
        rt = board_buzzer_play_twinkle_twinkle_little_star();
        if (OPRT_OK != rt) {
            PR_ERR("Failed to play Twinkle Twinkle Little Star: %d", rt);
        } else {
            PR_NOTICE("B Button: Twinkle Twinkle Little Star started successfully");
        }
    } else if (event == TDL_BUTTON_PRESS_DOUBLE_CLICK) {
        PR_NOTICE("B Button: Double click - stopping any playing sequence");
        if (board_buzzer_is_sequence_playing()) {
            board_buzzer_stop_sequence();
        }
        board_buzzer_stop();
    } else if (event == TDL_BUTTON_LONG_PRESS_START) {
        PR_NOTICE("B Button: Long press - starting sand physics demo");
        if (board_buzzer_is_sequence_playing()) {
            board_buzzer_stop_sequence();
        }
        board_buzzer_stop();

        // Switch to sand physics mode and reset sand state
        g_sand_initialized = false; // Reset sand state for fresh start
        g_animation_mode = SAND_PHYSICS_MODE;
        PR_NOTICE("B Button: Switched to sand physics mode");
    } else if (event == TDL_BUTTON_PRESS_DOWN) {
        PR_NOTICE("B Button: Press DOWN detected");
    } else if (event == TDL_BUTTON_PRESS_UP) {
        PR_NOTICE("B Button: Press UP detected");
    } else {
        PR_NOTICE("B Button: Unknown event type: %d", event);
    }
}

/**
 * @brief Initialize buttons and register callbacks
 */
static void buzzer_demo_init_buttons(void)
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
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, buzzer_button_ok_cb);
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_PRESS_DOUBLE_CLICK, buzzer_button_ok_cb);
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_LONG_PRESS_START, buzzer_button_ok_cb);
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_PRESS_UP, buzzer_button_ok_cb);
        PR_NOTICE("OK button initialized");
    } else {
        PR_ERR("Failed to create OK button: %d", rt);
    }

    // Initialize A button
    rt = tdl_button_create(BUTTON_NAME_2, &button_cfg, &g_button_a_handle);
    if (OPRT_OK == rt) {
        tdl_button_event_register(g_button_a_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, buzzer_button_a_cb);
        tdl_button_event_register(g_button_a_handle, TDL_BUTTON_PRESS_DOUBLE_CLICK, buzzer_button_a_cb);
        tdl_button_event_register(g_button_a_handle, TDL_BUTTON_LONG_PRESS_START, buzzer_button_a_cb);
        tdl_button_event_register(g_button_a_handle, TDL_BUTTON_PRESS_UP, buzzer_button_a_cb);
        PR_NOTICE("A button initialized");
    } else {
        PR_ERR("Failed to create A button: %d", rt);
    }

    // Initialize B button
    PR_NOTICE("Initializing B button with name: %s", BUTTON_NAME_3);
    rt = tdl_button_create(BUTTON_NAME_3, &button_cfg, &g_button_b_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("B button created successfully, handle: %p", g_button_b_handle);
        // Register all possible events to catch any button activity
        tdl_button_event_register(g_button_b_handle, TDL_BUTTON_PRESS_DOWN, buzzer_button_b_cb);
        tdl_button_event_register(g_button_b_handle, TDL_BUTTON_PRESS_UP, buzzer_button_b_cb);
        tdl_button_event_register(g_button_b_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, buzzer_button_b_cb);
        tdl_button_event_register(g_button_b_handle, TDL_BUTTON_PRESS_DOUBLE_CLICK, buzzer_button_b_cb);
        tdl_button_event_register(g_button_b_handle, TDL_BUTTON_LONG_PRESS_START, buzzer_button_b_cb);
        PR_NOTICE("B button initialized and all events registered successfully");
    } else {
        PR_ERR("Failed to create B button '%s': %d", BUTTON_NAME_3, rt);
        PR_ERR("Make sure BUTTON_NAME_3 is registered in board_register_hardware()");
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
 * @brief Breathing color effect
 */
static void __breathing_color_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    static const PIXEL_COLOR_T cCOLOR_ARR[] = {
        {.warm = 0, .cold = 0, .red = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .green = 0, .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = 0, .blue = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS)},
    };

    static uint32_t static_intensity = 0;
    static int32_t static_direction = 1;
    static uint32_t static_cycle_count = 0;
    static uint32_t static_color_index = 0;
    static bool animation_complete = false;
    static uint32_t max_cycles = 3;
    uint32_t step = 20;
    uint32_t color_num = 3;

    if (animation_complete && !g_animation_loop) {
        return;
    }

    if (animation_complete) {
        static_intensity = 0;
        static_direction = 1;
        static_cycle_count = 0;
        static_color_index = 0;
        animation_complete = false;
    }

    static_intensity += (static_direction * step);

    if (static_intensity >= COLOR_RESOLUTION) {
        static_intensity = COLOR_RESOLUTION;
        static_direction = -1;
    } else if (static_intensity <= 0) {
        static_intensity = 0;
        static_direction = 1;
        static_cycle_count++;
        static_color_index = (static_color_index + 1) % color_num;

        if (static_cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }

    PIXEL_COLOR_T current_color = {0};
    current_color.red = (cCOLOR_ARR[static_color_index].red * static_intensity) / COLOR_RESOLUTION;
    current_color.green = (cCOLOR_ARR[static_color_index].green * static_intensity) / COLOR_RESOLUTION;
    current_color.blue = (cCOLOR_ARR[static_color_index].blue * static_intensity) / COLOR_RESOLUTION;

    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &current_color);
    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Running light effect
 */
static void __running_light_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static const PIXEL_COLOR_T cCOLOR_ARR[] = {
        {.warm = 0, .cold = 0, .red = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .green = 0, .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = 0, .blue = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS)},
    };

    static uint32_t current_led = 1;
    static uint32_t cycle_count = 0;
    static uint32_t max_cycles = 1;
    static uint32_t color_index = 0;
    static uint32_t color_num = 3;
    static uint32_t color_change_interval = 50;
    static bool animation_complete = false;

    if (animation_complete && !g_animation_loop) {
        return;
    }

    if (animation_complete) {
        current_led = 1;
        cycle_count = 0;
        color_index = 0;
        animation_complete = false;
    }

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    if ((current_led - 1) % color_change_interval == 0) {
        color_index = (color_index + 1) % color_num;
    }

    PIXEL_COLOR_T current_color = cCOLOR_ARR[color_index];
    tdl_pixel_set_single_color(g_pixels_handle, current_led, 1, &current_color);
    tdl_pixel_dev_refresh(g_pixels_handle);

    current_led++;
    if (current_led > 1023) {
        current_led = 1;
        cycle_count++;
        if (cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }
}

/**
 * @brief Color wave effect
 */
static void __color_wave_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static const PIXEL_COLOR_T cCOLOR_ARR[] = {
        {.warm = 0, .cold = 0, .red = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .green = 0, .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .blue = 0},
        {.warm = 0, .cold = 0, .red = 0, .green = 0, .blue = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS)},
    };

    static uint32_t wave_position = 0;
    static uint32_t cycle_count = 0;
    static uint32_t max_cycles = 2;
    static bool animation_complete = false;
    uint32_t wave_length = 20;
    uint32_t color_num = 3;

    if (animation_complete && !g_animation_loop) {
        return;
    }

    if (animation_complete) {
        wave_position = 0;
        cycle_count = 0;
        animation_complete = false;
    }

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    for (uint32_t i = 0; i < wave_length; i++) {
        uint32_t led_pos = (wave_position + i) % LED_PIXELS_TOTAL_NUM;
        uint32_t color_index = (i * color_num) / wave_length;
        PIXEL_COLOR_T current_color = cCOLOR_ARR[color_index];
        tdl_pixel_set_single_color(g_pixels_handle, led_pos, 1, &current_color);
    }

    tdl_pixel_dev_refresh(g_pixels_handle);

    wave_position++;
    if (wave_position >= LED_PIXELS_TOTAL_NUM) {
        wave_position = 0;
        cycle_count++;
        if (cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }
}

/**
 * @brief 2D wave effect
 */
static void __2d_wave_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static uint32_t static_cycle_count = 0;
    static float static_wave_radius = 0.0f;
    static float static_color_hue = 0.0f;
    static bool animation_complete = false;
    uint32_t max_cycles = 2;
    float max_radius = 23.0f;
    float wave_speed = 0.5f;

    if (animation_complete && !g_animation_loop) {
        return;
    }

    if (animation_complete) {
        static_cycle_count = 0;
        static_wave_radius = 0.0f;
        static_color_hue = 0.0f;
        animation_complete = false;
    }

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    static_wave_radius += wave_speed;
    if (static_wave_radius > max_radius) {
        static_wave_radius = 0.0f;
        static_cycle_count++;
        if (static_cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }

    static_color_hue += 2.0f;
    if (static_color_hue >= 360.0f) {
        static_color_hue = 0.0f;
    }

    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 32; x++) {
            float dx = (float)x - 15.5f;
            float dy = (float)y - 15.5f;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance <= static_wave_radius) {
                float distance_hue = (distance / max_radius) * 180.0f;
                float current_hue = static_color_hue - distance_hue;
                if (current_hue < 0.0f)
                    current_hue += 360.0f;

                PIXEL_COLOR_T color =
                    board_pixel_hsv_to_pixel_color(current_hue, 1.0f, 1.0f, BRIGHTNESS, COLOR_RESOLUTION);

                uint32_t led_index = board_pixel_matrix_coord_to_led_index(x, y);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Snowflake effect
 */
static void __snowflake_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static float angle = 0.0f;
    angle += 0.05f;

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 32; x++) {
            float dx = (float)x - 15.5f;
            float dy = (float)y - 15.5f;
            float distance = sqrtf(dx * dx + dy * dy);
            float point_angle = atan2f(dy, dx) + angle;

            float snowflake = sinf(6.0f * point_angle) * 0.3f + 0.7f;
            float radius = 12.0f * snowflake;

            if (distance <= radius) {
                float intensity = 1.0f - (distance / radius) * 0.3f;
                PIXEL_COLOR_T color = {.red = (uint32_t)(COLOR_RESOLUTION * intensity * 0.9f * BRIGHTNESS),
                                       .green = (uint32_t)(COLOR_RESOLUTION * intensity * 0.9f * BRIGHTNESS),
                                       .blue = (uint32_t)(COLOR_RESOLUTION * intensity * 1.0f * BRIGHTNESS),
                                       .warm = 0,
                                       .cold = (uint32_t)(COLOR_RESOLUTION * intensity * 0.6f * BRIGHTNESS)};

                uint32_t led_index = board_pixel_matrix_coord_to_led_index(x, y);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Breathing circle effect
 */
static void __breathing_circle_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static float breath = 0.0f;
    breath += 0.1f;
    float radius = 6.0f + 4.0f * sinf(breath);

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 32; x++) {
            float dx = (float)x - 15.5f;
            float dy = (float)y - 15.5f;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance <= radius) {
                float intensity = 1.0f - (distance / radius) * 0.5f;
                float hue = fmodf((breath * 0.5f + distance * 0.3f) * 60.0f, 360.0f);
                float saturation = 0.9f;
                float value = intensity;

                PIXEL_COLOR_T color =
                    board_pixel_hsv_to_pixel_color(hue, saturation, value, BRIGHTNESS, COLOR_RESOLUTION);

                uint32_t led_index = board_pixel_matrix_coord_to_led_index(x, y);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Ripple effect
 */
static void __ripple_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static float time = 0.0f;
    static float ripple_center_x = 16.0f, ripple_center_y = 16.0f;
    time += 0.2f;

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 32; x++) {
            float dx = (float)x - ripple_center_x;
            float dy = (float)y - ripple_center_y;
            float distance = sqrtf(dx * dx + dy * dy);

            float ripple = sinf(distance * 0.8f - time * 2.0f) * 0.5f + 0.5f;

            if (ripple > 0.3f) {
                float intensity = (ripple - 0.3f) / 0.7f;
                PIXEL_COLOR_T color = {.red = (uint32_t)(COLOR_RESOLUTION * intensity * 0.1f * BRIGHTNESS),
                                       .green = (uint32_t)(COLOR_RESOLUTION * intensity * 0.6f * BRIGHTNESS),
                                       .blue = (uint32_t)(COLOR_RESOLUTION * intensity * 1.0f * BRIGHTNESS),
                                       .warm = 0,
                                       .cold = (uint32_t)(COLOR_RESOLUTION * intensity * 0.8f * BRIGHTNESS)};

                uint32_t led_index = board_pixel_matrix_coord_to_led_index(x, y);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Scan animation effect
 */
static void __scan_animation_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    static uint32_t frame_count = 0;
    static uint32_t column_index = 0;
    static uint32_t row_index = 0;
    static bool column_phase = true;

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    frame_count++;
    if (frame_count >= 10) {
        frame_count = 0;

        if (column_phase) {
            column_index++;
            if (column_index >= 32) {
                column_index = 0;
                column_phase = false;
            }
        } else {
            row_index++;
            if (row_index >= 32) {
                row_index = 0;
                column_phase = true;
            }
        }
    }

    if (column_phase) {
        PIXEL_COLOR_T red_color = {
            .red = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .green = 0, .blue = 0, .warm = 0, .cold = 0};
        for (uint32_t y = 0; y < 32; y++) {
            uint32_t led_index = board_pixel_matrix_coord_to_led_index(column_index, y);
            if (led_index < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &red_color);
            }
        }
    } else {
        PIXEL_COLOR_T blue_color = {
            .red = 0, .green = 0, .blue = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS), .warm = 0, .cold = 0};
        for (uint32_t x = 0; x < 32; x++) {
            uint32_t led_index = board_pixel_matrix_coord_to_led_index(x, row_index);
            if (led_index < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &blue_color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Render a single character
 */
static void render_char(int32_t x, int32_t y, char ch, float hue)
{
    char upper_ch = (ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch;
    const LED_FONT_CHAR_T *font_char = get_font_char(upper_ch);

    for (int row = 0; row < 8; row++) {
        int display_y = y + row;
        if (display_y < 0 || display_y >= 32)
            continue;

        uint8_t row_data = font_char->data[row];
        for (int col = 0; col < 8; col++) {
            int display_x = x + col;
            if (display_x < 0 || display_x >= 32)
                continue;

            if (row_data & (0x80 >> col)) {
                float pixel_hue = fmodf(hue + (float)display_x * 12.0f, 360.0f);

                PIXEL_COLOR_T color =
                    board_pixel_hsv_to_pixel_color(pixel_hue, 1.0f, 1.0f, BRIGHTNESS, COLOR_RESOLUTION);

                uint32_t led_index = board_pixel_matrix_coord_to_led_index((uint32_t)display_x, (uint32_t)display_y);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }
}

/**
 * @brief Scrolling text effect
 */
static void __scrolling_text_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }
    const char *message = "Hi! it's TuyaOpen";

    static int32_t scroll_pos = 32;
    static float base_hue = 0.0f;
    static uint32_t frame_count = 0;
    static uint32_t text_width = 0;
    static bool text_width_calculated = false;

    if (!text_width_calculated) {
        text_width = calculate_text_width(message);
        text_width_calculated = true;
    }

    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    frame_count++;
    if (frame_count >= 1) {
        frame_count = 0;
        scroll_pos--;
        if (scroll_pos < -(int32_t)text_width) {
            scroll_pos = 32;
        }
    }

    int32_t char_x = scroll_pos;
    for (const char *p = message; *p; p++) {
        char ch = *p;
        const LED_FONT_CHAR_T *font_char = get_font_char((ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch);

        if (char_x + (int32_t)font_char->width >= 0 && char_x < 32) {
            render_char(char_x, 12, ch, base_hue);
        }

        char_x += font_char->width;
    }

    tdl_pixel_dev_refresh(g_pixels_handle);

    base_hue += 3.0f;
    if (base_hue > 360.0f)
        base_hue -= 360.0f;
}

/**
 * @brief Render pixel art animation
 */
static void __pixel_art_effect(const pixel_art_t *art)
{
    if (g_pixels_handle == NULL || art == NULL) {
        return;
    }

    static uint32_t frame_index[MAX_PIXEL_ART_ANIMATIONS] = {0}; // Separate frame index for each pixel art
    static uint32_t frame_counter = 0;
    static int last_art_index = -1;
    const uint32_t frame_delay = 0; // Delay between frames (2 * 50ms = 100ms per frame) - faster animation

    // Find the art index in the registered array by comparing pointers
    int art_index = -1;
    for (uint32_t i = 0; i < g_registered_pixel_art_count; i++) {
        if (g_registered_pixel_arts[i] == art) {
            art_index = (int)i;
            break;
        }
    }

    if (art_index < 0) {
        PR_ERR("Pixel art not found in registered animations");
        return; // Unknown pixel art
    }

    // Reset frame index if switching to a different pixel art
    if (last_art_index != art_index) {
        frame_index[art_index] = 0;
        last_art_index = art_index;
    }

    // Get the appropriate frame index for this pixel art
    uint32_t current_frame_index = frame_index[art_index];

    // Clear all pixels
    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    // Get current frame
    if (current_frame_index >= art->frame_count) {
        current_frame_index = 0;
    }

    const pixel_frame_t *frame = &art->frames[current_frame_index];
    const pixel_rgb_t *pixels = frame->pixels;

    // Render frame to LED matrix (32x32)
    for (uint32_t y = 0; y < frame->height && y < 32; y++) {
        for (uint32_t x = 0; x < frame->width && x < 32; x++) {
            uint32_t pixel_idx = y * frame->width + x;
            if (pixel_idx >= (frame->width * frame->height)) {
                continue;
            }

            const pixel_rgb_t *pixel = &pixels[pixel_idx];

            // Convert RGB (0-255) to COLOR_RESOLUTION scale with brightness
            // Note: LED hardware expects GRB order, so we swap red and green
            PIXEL_COLOR_T color = {0};
            color.red =
                (uint32_t)((pixel->g * COLOR_RESOLUTION * BRIGHTNESS) / 255); // Swap: red channel gets green data
            color.green =
                (uint32_t)((pixel->r * COLOR_RESOLUTION * BRIGHTNESS) / 255); // Swap: green channel gets red data
            color.blue = (uint32_t)((pixel->b * COLOR_RESOLUTION * BRIGHTNESS) / 255); // Blue stays the same

            // Convert 2D matrix coordinates to 1D LED index
            uint32_t led_idx = board_pixel_matrix_coord_to_led_index(x, y);
            if (led_idx < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_idx, 1, &color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);

    // Advance to next frame
    frame_counter++;
    if (frame_counter >= frame_delay) {
        frame_counter = 0;
        current_frame_index++;
        if (current_frame_index >= art->frame_count) {
            // Always loop pixel art animations
            current_frame_index = 0;
        }

        // Update the appropriate frame index
        frame_index[art_index] = current_frame_index;
    }
}

/**
 * @brief Initialize a new sand particle at row 2 center
 */
static void sand_init_particle(sand_particle_t *particle)
{
    if (particle == NULL) {
        return;
    }

    // Spawn at row 2 (y=1, 0-indexed) at center (x=16)
    particle->x = (float)(MATRIX_WIDTH / 2); // Center: 16.0
    particle->y = 1.0f;                      // Row 2 (0-indexed)

    // Small random horizontal velocity
    particle->vx = ((float)(rand() % 5) - 2.0f) * 0.1f; // -0.2 to 0.2
    particle->vy = 0.0f;

    // Sand color (yellowish/brownish tones)
    uint8_t base = 180 + (rand() % 40); // 180-220
    particle->r = base;
    particle->g = base - 20 + (rand() % 20); // Slightly less red
    particle->b = 60 + (rand() % 20);        // Low blue

    particle->active = true;
}

/**
 * @brief Update sand particle physics based on BMI270 sensor data
 */
static void sand_update_physics(void)
{
    bmi270_sensor_data_t sensor_data = {0};
    float acc_x = 0.0f, acc_y = 0.0f;
    float gyr_x = 0.0f, gyr_y = 0.0f;

    // Read sensor data if available
    if (g_bmi270_dev != NULL && board_bmi270_is_ready(g_bmi270_dev)) {
        if (board_bmi270_read_data(g_bmi270_dev, &sensor_data) == OPRT_OK) {
            acc_x = sensor_data.acc_x;
            acc_y = sensor_data.acc_y;
            // acc_z = sensor_data.acc_z;
            gyr_x = sensor_data.gyr_x;
            gyr_y = sensor_data.gyr_y;
            // gyr_z = sensor_data.gyr_z;
        }
    }

    // Calculate gravity direction from accelerometer
    // Hardware orientation: X axis points to sky (vertical), Y axis is left-right (horizontal)
    // acc_x -> vertical tilt (up/down on screen, affects Y movement)
    // acc_y -> horizontal tilt (left/right on screen, affects X movement)
    // Screen coordinates: Y=0 is top, Y=31 is bottom (positive Y goes down)
    float gravity_x = -acc_y * GRAVITY_SCALE; // Y accelerometer affects X movement (left/right, inverted)
    float gravity_y = acc_x * GRAVITY_SCALE;  // X accelerometer affects Y movement (down is positive)

    // Add base gravity (always pull down - positive Y)
    float base_gravity = 0.15f; // Base downward gravity (positive Y goes down)
    gravity_y += base_gravity;

    // Add gyroscope influence for rotation effects
    // gyr_x -> rotation around X axis (affects Y movement)
    // gyr_y -> rotation around Y axis (affects X movement)
    float rotation_x = gyr_y * 0.01f; // Y gyro affects X movement
    float rotation_y = gyr_x * 0.01f; // X gyro affects Y movement

    // Create a grid to check for particle collisions (simple stacking)
    static bool particle_grid[MATRIX_WIDTH][MATRIX_HEIGHT] = {false};

    // First pass: Build collision grid from current positions (before movement)
    // Also ensure all active particles have valid positions
    for (uint32_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint32_t y = 0; y < MATRIX_HEIGHT; y++) {
            particle_grid[x][y] = false;
        }
    }

    for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
        sand_particle_t *p = &g_sand_particles[i];
        if (!p->active) {
            continue;
        }

        // Ensure particle position is always valid before building grid
        if (p->x < 0.0f)
            p->x = 0.0f;
        if (p->x >= (MATRIX_WIDTH - 1.0f))
            p->x = (MATRIX_WIDTH - 1.0f);
        if (p->y < 0.0f)
            p->y = 0.0f;
        if (p->y >= (MATRIX_HEIGHT - 1.0f))
            p->y = (MATRIX_HEIGHT - 1.0f);

        int32_t px = (int32_t)(p->x + 0.5f);
        int32_t py = (int32_t)(p->y + 0.5f);
        if (px >= 0 && px < (int32_t)MATRIX_WIDTH && py >= 0 && py < (int32_t)MATRIX_HEIGHT) {
            particle_grid[px][py] = true;
        }
    }

    // Second pass: Update all particle positions with strict collision detection
    // Use a temporary grid to track new positions and prevent overlaps
    static bool temp_grid[MATRIX_WIDTH][MATRIX_HEIGHT] = {false};
    for (uint32_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint32_t y = 0; y < MATRIX_HEIGHT; y++) {
            temp_grid[x][y] = false;
        }
    }

    for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
        sand_particle_t *p = &g_sand_particles[i];
        if (!p->active) {
            continue;
        }

        // Get current grid position
        int32_t old_px = (int32_t)(p->x + 0.5f);
        int32_t old_py = (int32_t)(p->y + 0.5f);

        // Clamp old position to bounds
        if (old_px < 0)
            old_px = 0;
        if (old_px >= (int32_t)MATRIX_WIDTH)
            old_px = MATRIX_WIDTH - 1;
        if (old_py < 0)
            old_py = 0;
        if (old_py >= (int32_t)MATRIX_HEIGHT)
            old_py = MATRIX_HEIGHT - 1;

        // Apply gravity from accelerometer
        p->vx += gravity_x + rotation_x;
        p->vy += gravity_y + rotation_y;

        // Add some friction
        p->vx *= 0.95f;
        p->vy *= 0.95f;

        // Limit velocity
        if (p->vx > 2.0f)
            p->vx = 2.0f;
        if (p->vx < -2.0f)
            p->vx = -2.0f;
        if (p->vy > 2.0f)
            p->vy = 2.0f;
        if (p->vy < -2.0f)
            p->vy = -2.0f;

        // Calculate new position
        float new_x = p->x + p->vx;
        float new_y = p->y + p->vy;

        // Boundary collision - strict bounds (0.0 to 31.0 inclusive)
        if (new_x < 0.0f) {
            new_x = 0.0f;
            p->vx = 0.0f;
        } else if (new_x >= (MATRIX_WIDTH - 1.0f)) {
            new_x = (MATRIX_WIDTH - 1.0f);
            p->vx = 0.0f;
        }

        // Top and bottom boundaries (0 is top, 31 is bottom)
        if (new_y < 0.0f) {
            new_y = 0.0f;
            p->vy = 0.0f;
        } else if (new_y >= (MATRIX_HEIGHT - 1.0f)) {
            new_y = (MATRIX_HEIGHT - 1.0f);
            p->vy = 0.0f;
        }

        // Calculate target grid position
        int32_t new_px = (int32_t)(new_x + 0.5f);
        int32_t new_py = (int32_t)(new_y + 0.5f);

        // Clamp grid coordinates to valid bounds
        if (new_px < 0)
            new_px = 0;
        if (new_px >= (int32_t)MATRIX_WIDTH)
            new_px = MATRIX_WIDTH - 1;
        if (new_py < 0)
            new_py = 0;
        if (new_py >= (int32_t)MATRIX_HEIGHT)
            new_py = MATRIX_HEIGHT - 1;

        // Check if there's a particle below (for stacking/falling) - priority check
        bool blocked_below = false;
        if (new_py < (MATRIX_HEIGHT - 1) && particle_grid[new_px][new_py + 1]) {
            // Blocked below - stop vertical movement
            blocked_below = true;
            p->vy = 0.0f;
            new_y = (float)new_py; // Snap to exact grid position above blocking particle
            new_py = (int32_t)(new_y + 0.5f);
            if (new_py < 0)
                new_py = 0;
            if (new_py >= (int32_t)MATRIX_HEIGHT)
                new_py = MATRIX_HEIGHT - 1;
        }

        // Check if target cell is occupied (prevent any overlap)
        bool can_move = true;
        if (particle_grid[new_px][new_py] && (new_px != old_px || new_py != old_py)) {
            // Target cell is occupied - can't move there
            can_move = false;

            // If blocked below, try to slide horizontally
            if (blocked_below) {
                if (p->vx < -0.1f && new_px > 0 && !particle_grid[new_px - 1][new_py]) {
                    // Can slide left
                    new_px = new_px - 1;
                    new_x = (float)new_px;
                    can_move = true;
                } else if (p->vx > 0.1f && new_px < (MATRIX_WIDTH - 1) && !particle_grid[new_px + 1][new_py]) {
                    // Can slide right
                    new_px = new_px + 1;
                    new_x = (float)new_px;
                    can_move = true;
                } else {
                    // Can't slide, stop horizontal movement
                    p->vx = 0.0f;
                }
            } else {
                // Not blocked below, but target is occupied - stop movement
                p->vx = 0.0f;
                p->vy = 0.0f;
            }
        }

        // Check if the new position would overlap with particles already moved this frame
        if (can_move && temp_grid[new_px][new_py]) {
            // Another particle already moved here this frame - can't move
            can_move = false;
            p->vx = 0.0f;
            p->vy = 0.0f;
        }

        // Update position if movement is allowed
        if (can_move) {
            p->x = new_x;
            p->y = new_y;
            // Mark this cell as occupied in temp grid
            temp_grid[new_px][new_py] = true;
        } else {
            // Keep current position, ensure it's valid
            if (p->x < 0.0f)
                p->x = 0.0f;
            if (p->x >= (MATRIX_WIDTH - 1.0f))
                p->x = (MATRIX_WIDTH - 1.0f);
            if (p->y < 0.0f)
                p->y = 0.0f;
            if (p->y >= (MATRIX_HEIGHT - 1.0f))
                p->y = (MATRIX_HEIGHT - 1.0f);
            // Mark old position in temp grid to prevent other particles from moving here
            temp_grid[old_px][old_py] = true;
        }

        // Final validation - ensure position is always valid
        if (p->x < 0.0f)
            p->x = 0.0f;
        if (p->x >= (MATRIX_WIDTH - 1.0f))
            p->x = (MATRIX_WIDTH - 1.0f);
        if (p->y < 0.0f)
            p->y = 0.0f;
        if (p->y >= (MATRIX_HEIGHT - 1.0f))
            p->y = (MATRIX_HEIGHT - 1.0f);
    }

    // Third pass: Rebuild collision grid from final positions (for next frame)
    for (uint32_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint32_t y = 0; y < MATRIX_HEIGHT; y++) {
            particle_grid[x][y] = false;
        }
    }

    for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
        sand_particle_t *p = &g_sand_particles[i];
        if (!p->active) {
            continue;
        }

        // Ensure position is valid
        if (p->x < 0.0f)
            p->x = 0.0f;
        if (p->x >= (MATRIX_WIDTH - 1.0f))
            p->x = (MATRIX_WIDTH - 1.0f);
        if (p->y < 0.0f)
            p->y = 0.0f;
        if (p->y >= (MATRIX_HEIGHT - 1.0f))
            p->y = (MATRIX_HEIGHT - 1.0f);

        int32_t px = (int32_t)(p->x + 0.5f);
        int32_t py = (int32_t)(p->y + 0.5f);
        if (px >= 0 && px < (int32_t)MATRIX_WIDTH && py >= 0 && py < (int32_t)MATRIX_HEIGHT) {
            // If multiple particles end up in same cell, keep the first one, others stay in previous position
            if (!particle_grid[px][py]) {
                particle_grid[px][py] = true;
            } else {
                // Collision detected - keep particle in previous valid position
                // Find nearest empty cell or keep current if no empty cell nearby
                bool found = false;
                for (int32_t dy = -1; dy <= 1 && !found; dy++) {
                    for (int32_t dx = -1; dx <= 1 && !found; dx++) {
                        int32_t try_px = px + dx;
                        int32_t try_py = py + dy;
                        if (try_px >= 0 && try_px < (int32_t)MATRIX_WIDTH && try_py >= 0 &&
                            try_py < (int32_t)MATRIX_HEIGHT && !particle_grid[try_px][try_py]) {
                            p->x = (float)try_px;
                            p->y = (float)try_py;
                            particle_grid[try_px][try_py] = true;
                            found = true;
                        }
                    }
                }
                if (!found) {
                    // No empty cell found, keep previous position (already validated)
                    particle_grid[px][py] = true; // Mark as occupied anyway
                }
            }
        }
    }
}

/**
 * @brief Render sand particles (no border)
 */
static void sand_render(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    // Clear all pixels
    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    // Render all active sand particles
    for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
        sand_particle_t *p = &g_sand_particles[i];
        if (!p->active) {
            continue;
        }

        // Clamp position to valid range (full 32x32)
        int32_t px = (int32_t)(p->x + 0.5f);
        int32_t py = (int32_t)(p->y + 0.5f);

        if (px >= 0 && px < MATRIX_WIDTH && py >= 0 && py < MATRIX_HEIGHT) {
            uint32_t led_idx = board_pixel_matrix_coord_to_led_index((uint32_t)px, (uint32_t)py);
            if (led_idx < LED_PIXELS_TOTAL_NUM) {
                PIXEL_COLOR_T color = {0};
                // Note: LED hardware expects GRB order
                color.red = (uint32_t)((p->g * COLOR_RESOLUTION * BRIGHTNESS) / 255);
                color.green = (uint32_t)((p->r * COLOR_RESOLUTION * BRIGHTNESS) / 255);
                color.blue = (uint32_t)((p->b * COLOR_RESOLUTION * BRIGHTNESS) / 255);
                tdl_pixel_set_single_color(g_pixels_handle, led_idx, 1, &color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Sand physics effect - main function
 */
static void __sand_physics_effect(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    // Initialize sand state on first entry
    if (!g_sand_initialized) {
        // Initialize all particles as inactive
        for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
            g_sand_particles[i].active = false;
        }

        // Spawn initial 80 particles immediately
        uint32_t spawned = 0;
        for (uint32_t i = 0; i < MAX_SAND_PARTICLES && spawned < 80; i++) {
            sand_init_particle(&g_sand_particles[i]);
            spawned++;
        }

        g_last_sand_spawn_time = 0;
        g_sand_initialized = true;
        PR_NOTICE("Sand physics demo initialized with %d particles", spawned);
    }

    // Get current time (approximate using frame counter)
    static uint32_t frame_counter = 0;
    frame_counter++;
    uint32_t current_time = frame_counter * 1; // Approximate: 20ms per frame (faster)

    // Spawn new sand particles (3 per second = 1 every 333ms)
    if ((current_time - g_last_sand_spawn_time) >= SAND_SPAWN_RATE_MS) {
        // Find an inactive particle and spawn it
        for (uint32_t i = 0; i < MAX_SAND_PARTICLES; i++) {
            if (!g_sand_particles[i].active) {
                sand_init_particle(&g_sand_particles[i]);
                g_last_sand_spawn_time = current_time;
                break;
            }
        }
    }

    // Update physics
    sand_update_physics();

    // Render everything
    sand_render();
}

/**
 * @brief Pixel LED animation task thread
 */
static void pixel_led_animation_task(void *args)
{
    g_animation_running = true;
    PR_NOTICE("Pixel LED animation task started");

    while (g_animation_running) {
        switch (g_animation_mode) {
        case 0:
            __scrolling_text_effect();
            break;
        case 1:
            __breathing_color_effect();
            break;
        case 2:
            __ripple_effect();
            break;
        case 3:
            __2d_wave_effect();
            break;
        case 4:
            __snowflake_effect();
            break;
        case 5:
            __scan_animation_effect();
            break;
        case 6:
            __breathing_circle_effect();
            break;
        case 7:
            __running_light_effect();
            break;
        case 8:
            __color_wave_effect();
            break;
        case SAND_PHYSICS_MODE:
            __sand_physics_effect();
            break;
        default:
            // Handle pixel art animations (mode >= EFFECT_ANIMATION_COUNT)
            if (g_animation_mode >= EFFECT_ANIMATION_COUNT && g_animation_mode != SAND_PHYSICS_MODE) {
                uint32_t pixel_art_idx = g_animation_mode - EFFECT_ANIMATION_COUNT;
                if (pixel_art_idx < g_registered_pixel_art_count) {
                    __pixel_art_effect(g_registered_pixel_arts[pixel_art_idx]);
                } else {
                    // Invalid pixel art index, reset to mode 0
                    g_animation_mode = 0;
                    continue;
                }
            } else {
                // Invalid animation mode, reset to mode 0
                g_animation_mode = 0;
                continue;
            }
            break;
        }

        // tal_system_sleep(20); // Frame delay (faster framerate: ~50 FPS)
    }

    PR_NOTICE("Pixel LED animation task stopped");
    g_pixels_thrd = NULL;
}

/**
 * @brief Main user function
 */
static void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("==========================================");
    PR_NOTICE("Tuya T5AI Pixel Buzzer Demo");
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

    // Initialize BMI270 sensor
    g_bmi270_dev = board_bmi270_get_handle();
    if (g_bmi270_dev != NULL) {
        PR_NOTICE("BMI270 sensor handle obtained");
        // Wait a bit for hardware registration to complete
        tal_system_sleep(200);

        // Check if sensor is ready
        if (!board_bmi270_is_ready(g_bmi270_dev)) {
            PR_NOTICE("Initializing BMI270 sensor...");
            rt = board_bmi270_init(g_bmi270_dev);
            if (OPRT_OK == rt) {
                PR_NOTICE("BMI270 sensor initialized successfully");
            } else {
                PR_WARN("BMI270 sensor initialization failed: %d (will continue without sensor)", rt);
                g_bmi270_dev = NULL;
            }
        } else {
            PR_NOTICE("BMI270 sensor already initialized");
        }
    } else {
        PR_WARN("BMI270 sensor not available (will continue without sensor)");
    }

    // Initialize buzzer
    rt = board_buzzer_init();
    if (OPRT_OK != rt) {
        PR_ERR("board_buzzer_init failed: %d", rt);
        return;
    }
    PR_NOTICE("Buzzer initialized");

    // Initialize buttons
    buzzer_demo_init_buttons();

    // Play startup melody
    tal_system_sleep(500);
    buzzer_demo_play_startup_melody();

    // Initialize pixel art animation registrations
    pixel_art_init_registrations();

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

    PR_NOTICE("==========================================");
    PR_NOTICE("Demo Ready!");
    PR_NOTICE("==========================================");
    PR_NOTICE("Pixel LED Controls:");
    PR_NOTICE("  OK Button:");
    PR_NOTICE("    - Single/Double Click: Change animation");
    PR_NOTICE("    - Long Press: Toggle loop mode");
    PR_NOTICE("  A Button:");
    PR_NOTICE("    - Single/Double Click: Switch pixel art animations");
    PR_NOTICE("  B Button:");
    PR_NOTICE("    - Single Click: Play Twinkle Twinkle Little Star");
    PR_NOTICE("    - Long Press: Start sand physics demo");
    PR_NOTICE("==========================================");

    // Main loop
    int cnt = 0;
    while (1) {
        // Print status every 10 seconds
        if (cnt % 100 == 0) {
            PR_DEBUG("Demo running... (count: %d)", cnt);
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