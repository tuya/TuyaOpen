/**
 * @file example_led-pixels.c
 * @brief LED pixel driver example for SDK.
 *
 * This file provides an example implementation of LED pixel control using the Tuya SDK.
 * It demonstrates the configuration and usage of various LED pixel types (WS2812, SK6812, SM16703P, etc.)
 * for creating colorful lighting effects. The example focuses on setting up pixel drivers, managing color
 * sequences, and controlling LED strips with different timing patterns for various applications.
 *
 * The LED pixel driver example is designed to help developers understand how to integrate LED pixel
 * functionality into their Tuya-based IoT projects, enabling dynamic lighting control and visual effects
 * for smart lighting applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_system.h"
#include "tal_api.h"
#include <math.h>

#include "tkl_output.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI)
#include "tdd_pixel_ws2812.h"
#include "tdd_pixel_sm16703p.h"
#include "tdd_pixel_yx1903b.h"
#endif

#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#include "example_led-pixels.h"
#include "led_font.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define LED_PIXELS_TOTAL_NUM 256
#define LED_CHANGE_TIME      800 // ms
#define COLOR_RESOLUTION     1000u

/*
This demo uses single wire connection for all LEDs.
For this demo, we use a 16x16 LED matrix. Light 1xN strip is also compatible with simplier animations.

2D Single wire connection LED matrix:
16x16 LED matrix layout: coordinates range from top-left (0,0) to bottom-right (15,15).
LEDs are arranged in a zigzag pattern, with each row alternating direction (left-to-right, then right-to-left).

---- Top Row ----
[0]  [31] [32] ... [255] [256]
[1]  [30] [33] ... [254] [255]
[2]  [29] [34] ... [253] [254]
...
...
[15] [16] [47] ... [240] [241]
---- Bottom Row ----

Note: The actual LED index mapping may vary depending on hardware wiring, but this pattern assumes a standard zigzag
configuration for a 16x16 matrix.
*/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_HANDLE_T sg_pixels_handle = NULL;
static THREAD_HANDLE sg_pixels_thrd = NULL;

/***********************************************************
*********************** const define ***********************
***********************************************************/
static const PIXEL_COLOR_T cCOLOR_ARR[] = {{
                                               // red
                                               .warm = 0,
                                               .cold = 0,
                                               .red = COLOR_RESOLUTION,
                                               .green = 0,
                                               .blue = 0,
                                           },
                                           {
                                               // green
                                               .warm = 0,
                                               .cold = 0,
                                               .red = 0,
                                               .green = COLOR_RESOLUTION,
                                               .blue = 0,
                                           },
                                           {
                                               // blue
                                               .warm = 0,
                                               .cold = 0,
                                               .red = 0,
                                               .green = 0,
                                               .blue = COLOR_RESOLUTION,
                                           }};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __breathing_color_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T current_color = {0};
    uint32_t step = 20;
    uint32_t max_cycles = 3;
    uint32_t color_num = CNTSOF(cCOLOR_ARR);

    // Static variables for frame-by-frame animation
    static uint32_t static_intensity = 0;
    static int32_t static_direction = 1;
    static uint32_t static_cycle_count = 0;
    static uint32_t static_color_index = 0;
    static bool animation_complete = false;

    // Reset animation if complete
    if (animation_complete) {
        static_intensity = 0;
        static_direction = 1;
        static_cycle_count = 0;
        static_color_index = 0;
        animation_complete = false;
    }

    // Single frame update
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

    current_color.red = (cCOLOR_ARR[static_color_index].red * static_intensity) / COLOR_RESOLUTION;
    current_color.green = (cCOLOR_ARR[static_color_index].green * static_intensity) / COLOR_RESOLUTION;
    current_color.blue = (cCOLOR_ARR[static_color_index].blue * static_intensity) / COLOR_RESOLUTION;
    current_color.warm = (cCOLOR_ARR[static_color_index].warm * static_intensity) / COLOR_RESOLUTION;
    current_color.cold = (cCOLOR_ARR[static_color_index].cold * static_intensity) / COLOR_RESOLUTION;

    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &current_color), __ERROR);
    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

__ERROR:
    PR_ERR("breathing color effect error");
    return;
}

static void __running_light_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T current_color = {0};
    PIXEL_COLOR_T off_color = {0};

    // Static variables for frame-by-frame animation
    static uint32_t current_led = 1;
    static uint32_t cycle_count = 0;
    static uint32_t max_cycles = 1;
    static uint32_t color_index = 0;
    static uint32_t color_num = CNTSOF(cCOLOR_ARR);
    static uint32_t color_change_interval = 50;
    static bool animation_complete = false;

    // Reset animation if complete
    if (animation_complete) {
        current_led = 1;
        cycle_count = 0;
        color_index = 0;
        animation_complete = false;
    }

    // Clear all LEDs
    off_color.red = 0;
    off_color.green = 0;
    off_color.blue = 0;
    off_color.warm = 0;
    off_color.cold = 0;
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Single frame update
    if ((current_led - 1) % color_change_interval == 0) {
        color_index = (color_index + 1) % color_num;
    }

    current_color.red = cCOLOR_ARR[color_index].red;
    current_color.green = cCOLOR_ARR[color_index].green;
    current_color.blue = cCOLOR_ARR[color_index].blue;
    current_color.warm = cCOLOR_ARR[color_index].warm;
    current_color.cold = cCOLOR_ARR[color_index].cold;

    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, current_led, 1, &current_color), __ERROR);
    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

    // Update for next frame
    current_led++;
    if (current_led > 255) {
        current_led = 1;
        cycle_count++;

        if (cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }

__ERROR:
    PR_ERR("running light effect error");
    return;
}

static void __color_wave_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T current_color = {0};
    PIXEL_COLOR_T off_color = {0};
    uint32_t wave_position = 0;
    uint32_t wave_length = 20;
    uint32_t cycle_count = 0;
    uint32_t max_cycles = 2;
    uint32_t color_index = 0;
    uint32_t color_num = CNTSOF(cCOLOR_ARR);

    off_color.red = 0;
    off_color.green = 0;
    off_color.blue = 0;
    off_color.warm = 0;
    off_color.cold = 0;

    while (cycle_count < max_cycles) {
        TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

        for (uint32_t i = 0; i < wave_length; i++) {
            uint32_t led_pos = (wave_position + i) % LED_PIXELS_TOTAL_NUM;

            color_index = (i * color_num) / wave_length;
            current_color.red = cCOLOR_ARR[color_index].red;
            current_color.green = cCOLOR_ARR[color_index].green;
            current_color.blue = cCOLOR_ARR[color_index].blue;
            current_color.warm = cCOLOR_ARR[color_index].warm;
            current_color.cold = cCOLOR_ARR[color_index].cold;

            TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_pos, 1, &current_color), __ERROR);
        }

        TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

        wave_position++;
        if (wave_position >= LED_PIXELS_TOTAL_NUM) {
            wave_position = 0;
            cycle_count++;
        }

        // tal_system_sleep(50); // Removed - main loop controls timing
    }

__ERROR:
    PR_ERR("color wave effect error");
    return;
}

/**
 * @brief Convert 2D matrix coordinates to LED index
 *
 * @param x X coordinate (0-15)
 * @param y Y coordinate (0-15)
 * @return LED index (1-256)
 */
static uint32_t __matrix_coord_to_led_index(uint32_t x, uint32_t y)
{
    if (x >= 16 || y >= 16)
        return 0;

    // Convert to 1D index based on the column-based zigzag pattern:
    // Column 0: LEDs 0→15 (top to bottom)
    // Column 1: LEDs 31→16 (bottom to top)
    // Column 2: LEDs 32→47 (top to bottom)
    // Column 3: LEDs 63→48 (bottom to top)
    // etc.

    uint32_t led_index;
    if (x % 2 == 0) {
        // Even column: top to bottom
        led_index = (x * 16 + y);
    } else {
        // Odd column: bottom to top
        led_index = (x + 1) * 16 - 1 - y;
    }

    // Convert from 0-based to 1-based indexing for LED driver
    return led_index;
}

/**
 * @brief Calculate distance from center point
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @return Distance from center (7.5, 7.5)
 */
static float __distance_from_center(uint32_t x, uint32_t y)
{
    float dx = (float)x - 7.5f;
    float dy = (float)y - 7.5f;
    return sqrtf(dx * dx + dy * dy);
}

/**
 * @brief Calculate angle for 8-directional tinting
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @return Angle in degrees (0-360)
 */
static float __calculate_angle(uint32_t x, uint32_t y)
{
    float dx = (float)x - 7.5f;
    float dy = (float)y - 7.5f;
    float angle = atan2f(dy, dx) * 180.0f / 3.14159f;
    if (angle < 0)
        angle += 360.0f;
    return angle;
}

/**
 * @brief Apply 8-directional color tinting
 *
 * @param base_color Base RGB color
 * @param angle Angle in degrees
 * @param tinted_color Output tinted color
 */
static void __apply_directional_tint(PIXEL_COLOR_T *base_color, float angle, PIXEL_COLOR_T *tinted_color)
{
    // Define 8 directional tints (45 degrees each)
    float tint_strength = 0.3f; // 30% tint strength
    float red_tint = 0.0f, green_tint = 0.0f, blue_tint = 0.0f;

    // Calculate tint based on angle (8 directions)
    if (angle >= 337.5f || angle < 22.5f) {
        // Right (0°): Red tint
        red_tint = tint_strength;
    } else if (angle >= 22.5f && angle < 67.5f) {
        // Up-Right (45°): Yellow tint
        red_tint = tint_strength * 0.5f;
        green_tint = tint_strength * 0.5f;
    } else if (angle >= 67.5f && angle < 112.5f) {
        // Up (90°): Green tint
        green_tint = tint_strength;
    } else if (angle >= 112.5f && angle < 157.5f) {
        // Up-Left (135°): Cyan tint
        green_tint = tint_strength * 0.5f;
        blue_tint = tint_strength * 0.5f;
    } else if (angle >= 157.5f && angle < 202.5f) {
        // Left (180°): Blue tint
        blue_tint = tint_strength;
    } else if (angle >= 202.5f && angle < 247.5f) {
        // Down-Left (225°): Magenta tint
        red_tint = tint_strength * 0.5f;
        blue_tint = tint_strength * 0.5f;
    } else if (angle >= 247.5f && angle < 292.5f) {
        // Down (270°): Purple tint
        red_tint = tint_strength * 0.3f;
        blue_tint = tint_strength * 0.7f;
    } else {
        // Down-Right (315°): Orange tint
        red_tint = tint_strength * 0.7f;
        green_tint = tint_strength * 0.3f;
    }

    // Apply tint to base color
    tinted_color->red = (uint32_t)(base_color->red * (1.0f + red_tint));
    tinted_color->green = (uint32_t)(base_color->green * (1.0f + green_tint));
    tinted_color->blue = (uint32_t)(base_color->blue * (1.0f + blue_tint));
    tinted_color->warm = base_color->warm;
    tinted_color->cold = base_color->cold;

    // Clamp values to maximum
    if (tinted_color->red > COLOR_RESOLUTION)
        tinted_color->red = COLOR_RESOLUTION;
    if (tinted_color->green > COLOR_RESOLUTION)
        tinted_color->green = COLOR_RESOLUTION;
    if (tinted_color->blue > COLOR_RESOLUTION)
        tinted_color->blue = COLOR_RESOLUTION;
}

static void __2d_wave_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T tinted_color = {0};
    uint32_t max_cycles = 2;  // Reduced cycles to match other effects
    float max_radius = 11.0f; // Maximum radius to cover the 16x16 matrix
    float wave_speed = 0.5f;  // Faster wave speed for smoother transitions
    float color_saturation = 1.0f;
    float color_value = 1.0f;

    // Static variables for frame-by-frame animation
    static uint32_t static_cycle_count = 0;
    static float static_wave_radius = 0.0f;
    static float static_color_hue = 0.0f;
    static bool animation_complete = false;

    // Reset animation if complete
    if (animation_complete) {
        static_cycle_count = 0;
        static_wave_radius = 0.0f;
        static_color_hue = 0.0f;
        animation_complete = false;
    }

    // Clear LEDs for each frame
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Calculate current wave radius
    static_wave_radius += wave_speed;
    if (static_wave_radius > max_radius) {
        static_wave_radius = 0.0f;
        static_cycle_count++;

        if (static_cycle_count >= max_cycles) {
            animation_complete = true;
        }
    }

    // Continuous color spectrum transition at center
    static_color_hue += 2.0f; // Faster color transition for smoother effect
    if (static_color_hue >= 360.0f) {
        static_color_hue = 0.0f;
    }

    // Base color from spectrum is calculated per LED in the loop below

    // Apply wave effect to each LED
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            float distance = __distance_from_center(x, y);
            float angle = __calculate_angle(x, y);

            // Check if LED is within the expanding wave
            if (distance <= static_wave_radius) {
                // Calculate hue based on distance from center for constant color bands
                float distance_hue = (distance / max_radius) * 180.0f; // Reduced range for smoother transitions
                float current_hue = static_color_hue - distance_hue;   // Hue shifts based on distance
                if (current_hue < 0.0f)
                    current_hue += 360.0f;

                // Convert hue to RGB for constant color
                float h = current_hue / 60.0f;
                float c = color_value * color_saturation;
                float x_val = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
                float m = color_value - c;

                float r, g, b;
                if (h < 1.0f) {
                    r = c;
                    g = x_val;
                    b = 0;
                } else if (h < 2.0f) {
                    r = x_val;
                    g = c;
                    b = 0;
                } else if (h < 3.0f) {
                    r = 0;
                    g = c;
                    b = x_val;
                } else if (h < 4.0f) {
                    r = 0;
                    g = x_val;
                    b = c;
                } else if (h < 5.0f) {
                    r = x_val;
                    g = 0;
                    b = c;
                } else {
                    r = c;
                    g = 0;
                    b = x_val;
                }

                // Set constant color (no intensity fade)
                tinted_color.red = (uint32_t)((r + m) * COLOR_RESOLUTION);
                tinted_color.green = (uint32_t)((g + m) * COLOR_RESOLUTION);
                tinted_color.blue = (uint32_t)((b + m) * COLOR_RESOLUTION);
                tinted_color.warm = 0;
                tinted_color.cold = 0;

                // Apply 8-directional tinting
                __apply_directional_tint(&tinted_color, angle, &tinted_color);

                // Set LED color
                uint32_t led_index = __matrix_coord_to_led_index(x, y);
                if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &tinted_color),
                                       __ERROR);
                }
            }
        }
    }

    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);
    // tal_system_sleep(30); // Removed - main loop controls timing // Faster refresh for smoother transitions

__ERROR:
    PR_ERR("2D wave effect error");
    return;
}

// ===== CREATIVE 2D MATRIX EFFECTS =====

/**
 * @brief Snowflake pattern effect
 */
static void __snowflake_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T color = {0};
    static float angle = 0.0f;

    angle += 0.05f;

    // Clear matrix
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Draw snowflake
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            float dx = (float)x - 7.5f;
            float dy = (float)y - 7.5f;
            float distance = sqrtf(dx * dx + dy * dy);
            float point_angle = atan2f(dy, dx) + angle;

            // Snowflake pattern: 6-fold symmetry
            float snowflake = sinf(6.0f * point_angle) * 0.3f + 0.7f;
            float radius = 6.0f * snowflake;

            if (distance <= radius) {
                float intensity = 1.0f - (distance / radius) * 0.3f;
                color.red = (uint32_t)(COLOR_RESOLUTION * intensity * 0.9f);
                color.green = (uint32_t)(COLOR_RESOLUTION * intensity * 0.9f);
                color.blue = (uint32_t)(COLOR_RESOLUTION * intensity * 1.0f);
                color.warm = 0;
                color.cold = (uint32_t)(COLOR_RESOLUTION * intensity * 0.6f);

                uint32_t led_index = __matrix_coord_to_led_index(x, y);
                if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &color), __ERROR);
                }
            }
        }
    }

    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);
    // tal_system_sleep(80); // Removed - main loop controls timing

__ERROR:
    PR_ERR("Snowflake effect error");
    return;
}

/**
 * @brief Breathing circle effect
 */
static void __breathing_circle_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T color = {0};
    static float breath = 0.0f;

    breath += 0.1f;
    float radius = 3.0f + 2.0f * sinf(breath);

    // Clear matrix
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Draw breathing circle
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            float dx = (float)x - 7.5f;
            float dy = (float)y - 7.5f;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance <= radius) {
                float intensity = 1.0f - (distance / radius) * 0.5f;
                // Create a rainbow breathing effect
                float hue = (breath * 0.5f + distance * 0.3f) * 60.0f;
                float h = fmodf(hue, 360.0f) / 60.0f;
                float c = intensity * 0.9f;
                float x_val = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));

                if (h < 1.0f) {
                    color.red = (uint32_t)(c * COLOR_RESOLUTION);
                    color.green = (uint32_t)(x_val * COLOR_RESOLUTION);
                    color.blue = 0;
                } else if (h < 2.0f) {
                    color.red = (uint32_t)(x_val * COLOR_RESOLUTION);
                    color.green = (uint32_t)(c * COLOR_RESOLUTION);
                    color.blue = 0;
                } else if (h < 3.0f) {
                    color.red = 0;
                    color.green = (uint32_t)(c * COLOR_RESOLUTION);
                    color.blue = (uint32_t)(x_val * COLOR_RESOLUTION);
                } else if (h < 4.0f) {
                    color.red = 0;
                    color.green = (uint32_t)(x_val * COLOR_RESOLUTION);
                    color.blue = (uint32_t)(c * COLOR_RESOLUTION);
                } else if (h < 5.0f) {
                    color.red = (uint32_t)(x_val * COLOR_RESOLUTION);
                    color.green = 0;
                    color.blue = (uint32_t)(c * COLOR_RESOLUTION);
                } else {
                    color.red = (uint32_t)(c * COLOR_RESOLUTION);
                    color.green = 0;
                    color.blue = (uint32_t)(x_val * COLOR_RESOLUTION);
                }

                color.warm = (uint32_t)(COLOR_RESOLUTION * intensity * 0.2f);
                color.cold = (uint32_t)(COLOR_RESOLUTION * intensity * 0.1f);

                uint32_t led_index = __matrix_coord_to_led_index(x, y);
                if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &color), __ERROR);
                }
            }
        }
    }

    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);
    // tal_system_sleep(80); // Removed - main loop controls timing

__ERROR:
    PR_ERR("Breathing circle effect error");
    return;
}

/**
 * @brief Water ripple effect
 */
static void __ripple_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_COLOR_T color = {0};
    static float time = 0.0f;
    static float ripple_center_x = 8.0f, ripple_center_y = 8.0f;

    time += 0.2f;

    // Clear matrix
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Create ripples
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            float dx = (float)x - ripple_center_x;
            float dy = (float)y - ripple_center_y;
            float distance = sqrtf(dx * dx + dy * dy);

            float ripple = sinf(distance * 0.8f - time * 2.0f) * 0.5f + 0.5f;

            if (ripple > 0.3f) {
                float intensity = (ripple - 0.3f) / 0.7f;
                // Create vibrant blue-green ripples
                color.red = (uint32_t)(COLOR_RESOLUTION * intensity * 0.1f);
                color.green = (uint32_t)(COLOR_RESOLUTION * intensity * 0.6f);
                color.blue = (uint32_t)(COLOR_RESOLUTION * intensity * 1.0f);
                color.warm = 0;
                color.cold = (uint32_t)(COLOR_RESOLUTION * intensity * 0.8f);

                uint32_t led_index = __matrix_coord_to_led_index(x, y);
                if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &color), __ERROR);
                }
            }
        }
    }

    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);
    // tal_system_sleep(70); // Removed - main loop controls timing

__ERROR:
    PR_ERR("Ripple effect error");
    return;
}

// Render a single character at position
static void render_char(int32_t x, int32_t y, char ch, float hue)
{
    char upper_ch = (ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch;
    const LED_FONT_CHAR_T *font_char = get_font_char(upper_ch);

    // Render 8x8 font data directly (exactly 8 rows)
    for (int row = 0; row < 8; row++) {
        int display_y = y + row;

        if (display_y < 0 || display_y >= 16)
            continue;

        uint8_t row_data = font_char->data[row]; // Use 8x8 font data directly

        // Debug: Print font data for first character
        if (ch == 'H' && x == 0) {
            PR_NOTICE("Font row %d: 0x%02X", row, row_data);
        }
        for (int col = 0; col < 8; col++) {
            int display_x = x + col;
            if (display_x < 0 || display_x >= 16)
                continue;

            // Check if pixel should be lit (bit 7-col is set for 8-column font)
            if (row_data & (0x80 >> col)) {

                // Calculate color based on position and hue
                float pixel_hue = hue + (float)display_x * 12.0f;
                if (pixel_hue > 360.0f)
                    pixel_hue -= 360.0f;

                // HSV to RGB conversion
                float hf = pixel_hue / 60.0f;
                float c = 1.0f;
                float xv = c * (1.0f - fabsf(fmodf(hf, 2.0f) - 1.0f));
                float rr = 0.0f, gg = 0.0f, bb = 0.0f;

                if (hf < 1.0f) {
                    rr = c;
                    gg = xv;
                    bb = 0;
                } else if (hf < 2.0f) {
                    rr = xv;
                    gg = c;
                    bb = 0;
                } else if (hf < 3.0f) {
                    rr = 0;
                    gg = c;
                    bb = xv;
                } else if (hf < 4.0f) {
                    rr = 0;
                    gg = xv;
                    bb = c;
                } else if (hf < 5.0f) {
                    rr = xv;
                    gg = 0;
                    bb = c;
                } else {
                    rr = c;
                    gg = 0;
                    bb = xv;
                }

                PIXEL_COLOR_T color = {.red = (uint32_t)(rr * COLOR_RESOLUTION),
                                       .green = (uint32_t)(gg * COLOR_RESOLUTION),
                                       .blue = (uint32_t)(bb * COLOR_RESOLUTION),
                                       .warm = 0,
                                       .cold = 0};

                uint32_t led_index = __matrix_coord_to_led_index((uint32_t)display_x, (uint32_t)display_y);
                if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &color);
                }
            }
        }
    }
}

/**
 * @brief Column and row scan animation effect
 */
static void __scan_animation_effect(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Static variables for animation state
    static uint32_t frame_count = 0;
    static uint32_t column_index = 0;
    static uint32_t row_index = 0;
    static bool column_phase = true; // true = column scan, false = row scan

    // Clear display
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Update animation every 10 frames for slower, more visible movement
    frame_count++;
    if (frame_count >= 10) {
        frame_count = 0;

        if (column_phase) {
            // Column scan phase (red)
            column_index++;
            if (column_index >= 16) {
                column_index = 0;
                column_phase = false; // Switch to row scan
            }
        } else {
            // Row scan phase (blue)
            row_index++;
            if (row_index >= 16) {
                row_index = 0;
                column_phase = true; // Switch back to column scan
            }
        }
    }

    if (column_phase) {
        // Column scan: Light up entire column in red
        PIXEL_COLOR_T red_color = {.red = COLOR_RESOLUTION, .green = 0, .blue = 0, .warm = 0, .cold = 0};

        for (uint32_t y = 0; y < 16; y++) {
            uint32_t led_index = __matrix_coord_to_led_index(column_index, y);
            if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &red_color), __ERROR);
            }
        }
    } else {
        // Row scan: Light up entire row in blue
        PIXEL_COLOR_T blue_color = {.red = 0, .green = 0, .blue = COLOR_RESOLUTION, .warm = 0, .cold = 0};

        for (uint32_t x = 0; x < 16; x++) {
            uint32_t led_index = __matrix_coord_to_led_index(x, row_index);
            if (led_index > 0 && led_index <= LED_PIXELS_TOTAL_NUM) {
                TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, led_index, 1, &blue_color), __ERROR);
            }
        }
    }

    // Refresh display
    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

__ERROR:
    if (OPRT_OK != rt) {
        PR_ERR("scan animation effect failed: %d", rt);
    }
}

/**
 * @brief Optimized scrolling text effect for 16x16 LED matrix
 */
static void __scrolling_text_effect(void)
{
    OPERATE_RET rt = OPRT_OK;
    const char *message = "Hello World! 123 ABC xyz";

    // Static variables for scrolling state
    static int32_t scroll_pos = 16;  // Start off-screen to the right
    static float base_hue = 0.0f;    // Base hue for color cycling
    static uint32_t frame_count = 0; // Frame counter for scroll speed
    static uint32_t text_width = 0;  // Cached text width
    static bool text_width_calculated = false;

    // Calculate text width once
    if (!text_width_calculated) {
        text_width = calculate_text_width(message);
        text_width_calculated = true;
    }

    // Clear display
    PIXEL_COLOR_T off_color = {0};
    TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color), __ERROR);

    // Update scroll position (very fast scrolling)
    frame_count++;
    if (frame_count >= 1) { // Move every frame for very fast scrolling
        frame_count = 0;
        scroll_pos--;

        // Reset when text has scrolled completely off screen
        if (scroll_pos < -(int32_t)text_width) {
            scroll_pos = 16;
        }
    }

    // Render text
    int32_t char_x = scroll_pos;
    for (const char *p = message; *p; p++) {
        char ch = *p;
        const LED_FONT_CHAR_T *font_char = get_font_char((ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch);

        // Only render if character is visible on screen
        if (char_x + (int32_t)font_char->width >= 0 && char_x < 16) {
            render_char(char_x, 4, ch, base_hue); // y=4 centers 8-pixel font in 16x16 matrix (rows 4-11)
        }

        char_x += font_char->width; // Move to next character (no spacing)
    }

    // Refresh display
    TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

    // Update base hue for color cycling
    base_hue += 3.0f;
    if (base_hue > 360.0f)
        base_hue -= 360.0f;

__ERROR:
    if (OPRT_OK != rt) {
        PR_ERR("scrolling text effect failed: %d", rt);
    }
}

static void __example_pixels_task(void *args)
{
    uint32_t effect_mode = 0;
    uint32_t effect_cycles = 0;
    uint32_t max_cycles_per_effect = 200; // Run each effect for 200 cycles (longer duration)

    while (1) {
        switch (effect_mode) {
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

        default:
            effect_mode = 0;
            continue;
        }

        effect_cycles++;
        if (effect_cycles >= max_cycles_per_effect) {
            effect_cycles = 0;
            effect_mode = (effect_mode + 1) % 8; // 8 effects total
        }

        // Add a small delay to prevent too fast execution
        tal_system_sleep(50); // 50ms delay between frames
    }

    return;
}

/**
 * @brief    register hardware
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET reg_pixels_hardware(char *device_name)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SPI) && (ENABLE_SPI)
    /*
    Hardware Note: The LED pixel data line should be connected to the SPI0-MISO pin.
    This configuration allows the SPI peripheral to drive the LED strip using the MISO line as the data output.
    */
    PIXEL_DRIVER_CONFIG_T dev_init_cfg = {
        .port = TUYA_SPI_NUM_0,
        .line_seq = RGB_ORDER,
    };

    // Register WS2812 by default. If using other drivers, please replace with other chip driver interfaces
    TUYA_CALL_ERR_RETURN(tdd_ws2812_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_ws2812_opt_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_ws2814_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sk6812_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16703p_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16704pk_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16714p_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_yx1903b_driver_register(device_name, &dev_init_cfg));

    return OPRT_OK;
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

/**
 * @brief    open driver
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET open_pixels_driver(char *device_name)
{
    OPERATE_RET rt = OPRT_OK;

    /*find leds strip pixels device*/
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_find(device_name, &sg_pixels_handle));

    /*open leds strip pixels device*/
    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = COLOR_RESOLUTION,
    };
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_open(sg_pixels_handle, &pixels_cfg));

    return OPRT_OK;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    reg_pixels_hardware("pixel");

    open_pixels_driver("pixel");

    /*create example task*/
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};

    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&sg_pixels_thrd, NULL, NULL, __example_pixels_task, NULL, &thrd_param));

    return;
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
    /*create example task*/
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif