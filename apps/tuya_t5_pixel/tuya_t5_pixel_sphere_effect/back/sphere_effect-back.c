/**
 * @file sphere_effect.c
 * @brief 3D rotating sphere effect on 32x32 LED pixel display
 *
 * This demo creates a virtual 3D space (32x32x32) with a rotating sphere.
 * The sphere is projected onto a 2D LED matrix with color gradients.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_memory.h"
#include "tkl_mutex.h"
#include "board_com_api.h"
#include "tdl_audio_manage.h"
#include "tuya_ringbuf.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#define LED_PIXELS_TOTAL_NUM 1024 + 3 // 1024+4 to work around broken LEDs 1021-1023
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.1f // 2% brightness
#define MATRIX_WIDTH         32
#define MATRIX_HEIGHT        32
#endif

// 3D space configuration
#define SPACE_SIZE          32
#define SPHERE_RADIUS       16.0f
#define SPHERE_CENTER_X     16.0f
#define SPHERE_CENTER_Y     16.0f
#define SPHERE_CENTER_Z     16.0f
#define DISPLAY_PLANE_Z     10.0f // Display plane at ~1/3 of sphere height (z=10 out of 32)
#define BASE_ROTATION_SPEED 30.0f // Base rotation speed (pixels per second) - smooth and visible
#define MAX_ROTATION_SPEED  60.0f // Maximum rotation speed with audio boost
#define UV_SPEED_MULTIPLIER 2.0f  // How much UV power affects speed (1.0 = no effect, 2.0 = doubles speed)

// Audio configuration
#define SAMPLE_RATE        16000
#define CHANNELS           1
#define BYTES_PER_SAMPLE   2 // 16-bit PCM
#define FRAME_SIZE_MS      10
#define FRAME_SIZE_BYTES   (SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * FRAME_SIZE_MS / 1000) // 320 bytes
#define AUDIO_RINGBUF_SIZE (FRAME_SIZE_BYTES * 32) // Buffer for 32 frames (~320ms)

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "audio"
#endif

// Audio power calculation
#define AUDIO_BUFFER_SIZE   160      // Number of samples to average for power calculation
#define POWER_NORMALIZATION 50000.0f // Normalization factor for power (adjust based on audio levels)

/***********************************************************
***********************variable define**********************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
static PIXEL_HANDLE_T g_pixels_handle = NULL;
#endif

// Audio processing
static TDL_AUDIO_HANDLE_T g_audio_handle = NULL;
static TUYA_RINGBUFF_T g_audio_ringbuf = NULL;
static MUTEX_HANDLE g_audio_rb_mutex = NULL;
static MUTEX_HANDLE g_uv_power_mutex = NULL;

static int16_t g_audio_buffer[AUDIO_BUFFER_SIZE];
static float g_audio_power = 0.0f; // Audio power (0.0-1.0), protected by mutex

// 3D rendering buffers
static float g_display_buffer[MATRIX_WIDTH][MATRIX_HEIGHT]; // Accumulated brightness for each pixel
static float g_hue_buffer[MATRIX_WIDTH][MATRIX_HEIGHT];     // Hue for each pixel (0-360 degrees)

// Rotation state - random rotation around multiple axes
static float g_rotation_angle_x = 0.0f; // Rotation angle around X axis
static float g_rotation_angle_y = 0.0f; // Rotation angle around Y axis
static float g_rotation_angle_z = 0.0f; // Rotation angle around Z axis
static float g_rotation_speed_x = 0.0f; // Rotation speed around X axis (rad/s)
static float g_rotation_speed_y = 0.0f; // Rotation speed around Y axis (rad/s)
static float g_rotation_speed_z = 0.0f; // Rotation speed around Z axis (rad/s)
static uint32_t g_last_update_time = 0;
static uint32_t g_rotation_change_time = 0; // Time when rotation direction changes

// Audio-reactive effects
static float g_audio_power_smoothed = 0.0f; // Smoothed audio power for animations
static float g_sphere_breath = 0.0f;        // Sphere breathing/pulsing effect (0-1)
static float g_hue_shift = 0.0f;            // Dynamic hue shift based on audio

// Hot spots configuration (multiple glowing points on sphere)
#define NUM_HOT_SPOTS 4
static struct {
    float x, y, z;   // Position on sphere (normalized)
    float intensity; // Current intensity (0-1)
    float phase;     // Animation phase
    float speed;     // Animation speed
} g_hot_spots[NUM_HOT_SPOTS];

/***********************************************************
********************function declaration********************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
static uint32_t __matrix_coord_to_led_index(uint32_t x, uint32_t y);
static OPERATE_RET pixel_led_init(void);
static void sphere_display(void);
static void render_sphere_3d(void);
static float calculate_sphere_brightness(float x, float y, float z, float nx, float ny, float nz, float audio_power);
static float calculate_sphere_hue(float x, float y, float z, float audio_power);
static float calculate_hot_spot_intensity(float x, float y, float z, float audio_power);
static void hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b);
static void update_rotation_axes(void);
static void update_audio_reactive_effects(float audio_power);
static void initialize_hot_spots(void);
#endif

// Audio processing functions
static void audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len);
static void audio_processing_task(void *args);
static void process_audio_power(uint8_t *audio_data, uint32_t data_len);

/***********************************************************
***********************function implementation**************
***********************************************************/

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
/**
 * @brief Convert matrix coordinates to LED index
 */
static uint32_t __matrix_coord_to_led_index(uint32_t x, uint32_t y)
{
    if (x >= 32 || y >= 32)
        return LED_PIXELS_TOTAL_NUM;

    uint32_t led_index;
    if (y % 2 == 0) {
        // Even row: left to right
        led_index = y * 32 + x;
    } else {
        // Odd row: right to left
        led_index = (y + 1) * 32 - 1 - x;
    }
    return led_index;
}

/**
 * @brief Initialize pixel LED driver
 */
static OPERATE_RET pixel_led_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(PIXEL_DEVICE_NAME)
    tal_system_sleep(100);

    rt = tdl_pixel_dev_find(PIXEL_DEVICE_NAME, &g_pixels_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to find pixel device '%s': %d", PIXEL_DEVICE_NAME, rt);
        return rt;
    }

    if (g_pixels_handle == NULL) {
        PR_ERR("Pixel device handle is NULL after find");
        return OPRT_COM_ERROR;
    }

    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = COLOR_RESOLUTION,
    };
    rt = tdl_pixel_dev_open(g_pixels_handle, &pixels_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to open pixel device: %d", rt);
        g_pixels_handle = NULL;
        return rt;
    }

    PR_NOTICE("Pixel LED initialized: %d pixels", LED_PIXELS_TOTAL_NUM);
#else
    PR_ERR("PIXEL_DEVICE_NAME not defined");
    return OPRT_INVALID_PARM;
#endif

    return rt;
}

/**
 * @brief Initialize hot spots on the sphere
 */
static void initialize_hot_spots(void)
{
    // Create 4 hot spots at different positions on the sphere
    // Positions are normalized vectors (on unit sphere)
    g_hot_spots[0].x = 0.707f;
    g_hot_spots[0].y = 0.707f;
    g_hot_spots[0].z = 0.0f;
    g_hot_spots[1].x = -0.707f;
    g_hot_spots[1].y = 0.0f;
    g_hot_spots[1].z = 0.707f;
    g_hot_spots[2].x = 0.0f;
    g_hot_spots[2].y = -0.707f;
    g_hot_spots[2].z = 0.707f;
    g_hot_spots[3].x = 0.577f;
    g_hot_spots[3].y = 0.577f;
    g_hot_spots[3].z = 0.577f;

    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        g_hot_spots[i].intensity = 0.0f;
        g_hot_spots[i].phase = (float)i * 2.0f * M_PI / NUM_HOT_SPOTS;
        g_hot_spots[i].speed = 0.5f + (float)i * 0.2f; // Different speeds for each
    }
}

/**
 * @brief Update audio-reactive effects (breathing, hue shift, hot spots)
 */
static void update_audio_reactive_effects(float audio_power)
{
    // Smooth audio power with exponential moving average
    float alpha = 0.1f; // Smoothing factor
    g_audio_power_smoothed = alpha * audio_power + (1.0f - alpha) * g_audio_power_smoothed;

    // Sphere breathing effect - pulse radius with audio
    // Breathing follows audio with a slight delay for organic feel
    static float breath_target = 0.0f;
    breath_target = g_audio_power_smoothed * 0.3f; // Max 30% size change
    float breath_alpha = 0.05f;
    g_sphere_breath = breath_alpha * breath_target + (1.0f - breath_alpha) * g_sphere_breath;

    // Dynamic hue shift - colors shift based on audio power
    g_hue_shift += 0.5f + g_audio_power_smoothed * 2.0f; // Faster shift with more audio
    if (g_hue_shift >= 360.0f) {
        g_hue_shift -= 360.0f;
    }

    // Update hot spots - pulse with audio
    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        g_hot_spots[i].phase += g_hot_spots[i].speed * 0.01f;
        if (g_hot_spots[i].phase >= 2.0f * M_PI) {
            g_hot_spots[i].phase -= 2.0f * M_PI;
        }

        // Hot spot intensity pulses with audio and has its own animation
        float base_intensity = 0.3f + g_audio_power_smoothed * 0.7f; // Audio-reactive base
        float pulse = 0.5f * (1.0f + sinf(g_hot_spots[i].phase));    // Continuous pulse
        g_hot_spots[i].intensity = base_intensity * (0.5f + 0.5f * pulse);
    }
}

/**
 * @brief Calculate hot spot intensity contribution at a point
 */
static float calculate_hot_spot_intensity(float x, float y, float z, float audio_power)
{
    float total_intensity = 0.0f;

    // Normalize point to unit sphere
    float dist = sqrtf(x * x + y * y + z * z);
    if (dist < 0.001f)
        return 0.0f;
    float nx = x / dist;
    float ny = y / dist;
    float nz = z / dist;

    // Check distance to each hot spot
    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        // Dot product gives cosine of angle (1.0 = same direction, -1.0 = opposite)
        float dot = nx * g_hot_spots[i].x + ny * g_hot_spots[i].y + nz * g_hot_spots[i].z;

        // Convert to angle distance (0 = same point, π = opposite)
        float angle_dist = acosf(fmaxf(-1.0f, fminf(1.0f, dot)));

        // Hot spot has a falloff radius (about 60 degrees)
        float hotspot_radius = M_PI / 3.0f;
        if (angle_dist < hotspot_radius) {
            // Smooth falloff using cosine
            float normalized = angle_dist / hotspot_radius;
            float falloff = 0.5f * (1.0f + cosf(normalized * M_PI));
            total_intensity += g_hot_spots[i].intensity * falloff;
        }
    }

    // Clamp total intensity
    if (total_intensity > 1.0f) {
        total_intensity = 1.0f;
    }

    return total_intensity;
}

/**
 * @brief Calculate brightness for a point on the sphere surface (audio-reactive)
 * @param x, y, z: 3D coordinates (rotated)
 * @param nx, ny, nz: Normal vector at this point
 * @param audio_power: Current audio power (0-1)
 * @return Brightness value (0.0 to 1.0)
 */
static float calculate_sphere_brightness(float x, float y, float z, float nx, float ny, float nz, float audio_power)
{
    // Base brightness from sphere position (left/right bright regions)
    float angle = atan2f(y - SPHERE_CENTER_Y, x - SPHERE_CENTER_X);
    if (angle < 0.0f) {
        angle += 2.0f * M_PI;
    }

    float left_angle = M_PI;
    float right_angle = 0.0f;
    float dist_from_left = fabsf(angle - left_angle);
    if (dist_from_left > M_PI) {
        dist_from_left = 2.0f * M_PI - dist_from_left;
    }
    float dist_from_right = fabsf(angle - right_angle);
    if (dist_from_right > M_PI) {
        dist_from_right = 2.0f * M_PI - dist_from_right;
    }

    float bright_region = M_PI / 6.0f;
    float min_dist = (dist_from_left < dist_from_right) ? dist_from_left : dist_from_right;

    float base_brightness;
    if (min_dist < bright_region) {
        base_brightness = 1.0f;
    } else {
        float transition_start = bright_region;
        float transition_end = M_PI / 2.0f;
        float normalized_dist = (min_dist - transition_start) / (transition_end - transition_start);
        if (normalized_dist < 0.0f)
            normalized_dist = 0.0f;
        if (normalized_dist > 1.0f)
            normalized_dist = 1.0f;
        base_brightness = 0.5f * (1.0f + cosf(normalized_dist * M_PI));
    }

    // Add hot spot intensity
    float hotspot_intensity = calculate_hot_spot_intensity(x, y, z, audio_power);

    // Combine base brightness with hot spots
    // Hot spots add extra brightness, especially with audio
    float brightness = base_brightness + hotspot_intensity * (0.5f + audio_power * 0.5f);

    // Audio-reactive overall brightness boost
    brightness *= (0.7f + audio_power * 0.3f);

    // Clamp to valid range
    if (brightness > 1.0f)
        brightness = 1.0f;
    if (brightness < 0.0f)
        brightness = 0.0f;

    return brightness;
}

/**
 * @brief Calculate hue for a point on the sphere surface (audio-reactive full color spectrum)
 * @param x, y, z: 3D coordinates (rotated, sphere surface coordinates)
 * @param audio_power: Current audio power (0-1)
 * @return Hue value in degrees (0-360)
 */
static float calculate_sphere_hue(float x, float y, float z, float audio_power)
{
    // Map sphere position to hue using spherical coordinates
    float dx = x - SPHERE_CENTER_X;
    float dy = y - SPHERE_CENTER_Y;
    float dz = z - SPHERE_CENTER_Z;

    // Calculate spherical coordinates
    float azimuth = atan2f(dx, dz);
    if (azimuth < 0.0f) {
        azimuth += 2.0f * M_PI;
    }

    float dist_xz = sqrtf(dx * dx + dz * dz);
    float elevation = atan2f(dy, dist_xz);
    float elevation_normalized = (elevation + M_PI / 2.0f) / M_PI;

    // Base hue from position
    float base_hue = (azimuth / (2.0f * M_PI)) * 360.0f;
    float hue = base_hue + (elevation_normalized - 0.5f) * 180.0f;

    // Audio-reactive hue shift - colors flow and change with audio
    hue += g_hue_shift;

    // Hot spots add color variation
    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        float dist =
            sqrtf((x - SPHERE_CENTER_X) * (x - SPHERE_CENTER_X) + (y - SPHERE_CENTER_Y) * (y - SPHERE_CENTER_Y) +
                  (z - SPHERE_CENTER_Z) * (z - SPHERE_CENTER_Z));
        if (dist < 0.001f)
            continue;

        float nx = (x - SPHERE_CENTER_X) / dist;
        float ny = (y - SPHERE_CENTER_Y) / dist;
        float nz = (z - SPHERE_CENTER_Z) / dist;

        float dot = nx * g_hot_spots[i].x + ny * g_hot_spots[i].y + nz * g_hot_spots[i].z;
        float angle_dist = acosf(fmaxf(-1.0f, fminf(1.0f, dot)));

        if (angle_dist < M_PI / 3.0f) {
            // Hot spots shift hue towards warmer colors (red/orange)
            float influence = (1.0f - angle_dist / (M_PI / 3.0f)) * g_hot_spots[i].intensity;
            hue += influence * 30.0f * audio_power; // Shift towards red with audio
        }
    }

    // Keep hue in 0-360 range
    while (hue >= 360.0f) {
        hue -= 360.0f;
    }
    while (hue < 0.0f) {
        hue += 360.0f;
    }

    return hue;
}

/**
 * @brief Convert HSV to RGB
 * @param hue Hue in degrees (0-360)
 * @param saturation Saturation (0.0-1.0)
 * @param value Value/brightness (0.0-1.0)
 * @param r Output red component (0-255)
 * @param g Output green component (0-255)
 * @param b Output blue component (0-255)
 */
static void hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b)
{
    // Normalize hue to 0-360
    while (hue < 0.0f)
        hue += 360.0f;
    while (hue >= 360.0f)
        hue -= 360.0f;

    float h = hue / 60.0f;
    float c = value * saturation;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = value - c;

    float rf, gf, bf;
    if (h < 1.0f) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 2.0f) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 3.0f) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 4.0f) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 5.0f) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }

    *r = (uint32_t)((rf + m) * 255.0f);
    *g = (uint32_t)((gf + m) * 255.0f);
    *b = (uint32_t)((bf + m) * 255.0f);
}

/**
 * @brief Render 3D sphere and project to 2D display plane
 * All Z layers are summed into the display plane at z = DISPLAY_PLANE_Z
 */
static void render_sphere_3d(void)
{
    // Clear display buffers
    memset(g_display_buffer, 0, sizeof(g_display_buffer));
    memset(g_hue_buffer, 0, sizeof(g_hue_buffer));

    // For each pixel on the display plane, look through all Z layers
    // and sum up the brightness contributions
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float x_world = (float)x;
            float y_world = (float)y;

            // Iterate through all Z layers to sum contributions
            for (int z = 0; z < SPACE_SIZE; z++) {
                float z_world = (float)z;

                // Calculate distance from sphere center
                float dx = x_world - SPHERE_CENTER_X;
                float dy = y_world - SPHERE_CENTER_Y;
                float dz = z_world - SPHERE_CENTER_Z;

                // Apply rotation around X axis
                float cos_x = cosf(g_rotation_angle_x);
                float sin_x = sinf(g_rotation_angle_x);
                float dy_x = dy * cos_x - dz * sin_x;
                float dz_x = dy * sin_x + dz * cos_x;
                float dx_x = dx;

                // Apply rotation around Y axis
                float cos_y = cosf(g_rotation_angle_y);
                float sin_y = sinf(g_rotation_angle_y);
                float dx_y = dx_x * cos_y - dz_x * sin_y;
                float dz_y = dx_x * sin_y + dz_x * cos_y;
                float dy_y = dy_x;

                // Apply rotation around Z axis
                float cos_z = cosf(g_rotation_angle_z);
                float sin_z = sinf(g_rotation_angle_z);
                float x_rot = dx_y * cos_z - dy_y * sin_z;
                float y_rot = dx_y * sin_z + dy_y * cos_z;
                float z_rot = dz_y;

                // Get current audio power for reactive effects
                float audio_power = 0.0f;
                if (g_uv_power_mutex != NULL) {
                    if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
                        audio_power = g_audio_power;
                        tal_mutex_unlock(g_uv_power_mutex);
                    }
                }

                // Audio-reactive sphere breathing - radius pulses with audio
                float current_radius = SPHERE_RADIUS * (1.0f + g_sphere_breath);
                float radius_sq = current_radius * current_radius;

                // Check if point is inside sphere (with breathing effect)
                float dist_sq = x_rot * x_rot + y_rot * y_rot + z_rot * z_rot;

                if (dist_sq <= radius_sq) {
                    // Point is inside sphere, calculate surface point
                    float dist = sqrtf(dist_sq);
                    if (dist < 0.001f) {
                        dist = 0.001f; // Avoid division by zero
                    }

                    // Normalize to sphere surface (with breathing)
                    float scale = current_radius / dist;
                    float sx = x_rot * scale;
                    float sy = y_rot * scale;
                    float sz = z_rot * scale;

                    // Calculate normal vector (points outward from center)
                    float nx = sx / current_radius;
                    float ny = sy / current_radius;
                    float nz = sz / current_radius;

                    // Calculate brightness and hue for this point (audio-reactive)
                    float brightness = calculate_sphere_brightness(sx, sy, sz, nx, ny, nz, audio_power);
                    float hue = calculate_sphere_hue(sx, sy, sz, audio_power);

                    // Sum brightness and weighted hue from all layers into display buffer
                    g_display_buffer[x][y] += brightness;
                    // Weight hue by brightness for proper color mixing
                    g_hue_buffer[x][y] += hue * brightness;
                }
            }
        }
    }

    // Normalize accumulated brightness and calculate weighted average hue
    float max_brightness = 0.0f;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (g_display_buffer[x][y] > max_brightness) {
                max_brightness = g_display_buffer[x][y];
            }
        }
    }

    // Normalize brightness to 0-1 range and calculate average hue
    if (max_brightness > 0.001f) {
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                float brightness_sum = g_display_buffer[x][y];
                g_display_buffer[x][y] /= max_brightness;
                if (g_display_buffer[x][y] > 1.0f) {
                    g_display_buffer[x][y] = 1.0f;
                }
                // Calculate average hue (weighted by brightness)
                if (brightness_sum > 0.001f) {
                    g_hue_buffer[x][y] /= brightness_sum;
                } else {
                    g_hue_buffer[x][y] = 0.0f;
                }
                // Keep hue in 0-360 range
                while (g_hue_buffer[x][y] >= 360.0f) {
                    g_hue_buffer[x][y] -= 360.0f;
                }
                while (g_hue_buffer[x][y] < 0.0f) {
                    g_hue_buffer[x][y] += 360.0f;
                }
            }
        }
    }
}

/**
 * @brief Update rotation axes with random directions and speeds
 */
static void update_rotation_axes(void)
{
    uint32_t current_time = tal_time_get_posix();

    // Change rotation direction every 3-5 seconds randomly
    if (g_rotation_change_time == 0 || (current_time - g_rotation_change_time) > 4) {
        // Random rotation speeds for each axis (can be positive or negative)
        // Base speed: 0.3 to 0.8 radians per second (smooth and visible)
        float base_speed = 0.3f + ((float)(current_time % 100) / 100.0f) * 0.5f;

        // Random direction for each axis (-1 or 1)
        int dir_x = ((current_time % 2) == 0) ? 1 : -1;
        int dir_y = (((current_time / 2) % 2) == 0) ? 1 : -1;
        int dir_z = (((current_time / 3) % 2) == 0) ? 1 : -1;

        g_rotation_speed_x = base_speed * dir_x;
        g_rotation_speed_y = base_speed * dir_y * 1.2f; // Slightly different speed
        g_rotation_speed_z = base_speed * dir_z * 0.8f; // Slightly different speed

        g_rotation_change_time = current_time;
    }
}

/**
 * @brief Display sphere on LED matrix
 */
static void sphere_display(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    // Update rotation angles based on time and audio power
    uint32_t current_time = tal_time_get_posix();
    if (g_last_update_time > 0) {
        // Calculate elapsed time in seconds with better precision
        float elapsed_sec = (float)(current_time - g_last_update_time);
        if (elapsed_sec > 0.1f) {
            elapsed_sec = 0.1f; // Cap at 100ms for stability (prevents large jumps)
        }
        if (elapsed_sec < 0.0f) {
            elapsed_sec = 0.0f; // Prevent negative time
        }

        // Get audio power (non-blocking, with timeout protection)
        float audio_power = 0.0f;
        if (g_uv_power_mutex != NULL) {
            if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
                audio_power = g_audio_power;
                tal_mutex_unlock(g_uv_power_mutex);
            }
        }

        // Update audio-reactive effects (breathing, hue shift, hot spots)
        update_audio_reactive_effects(audio_power);

        // Update rotation axes (random directions)
        update_rotation_axes();

        // Calculate rotation speed multiplier based on audio power
        float speed_multiplier = 1.0f + (UV_SPEED_MULTIPLIER - 1.0f) * audio_power;
        if (speed_multiplier > 2.0f) {
            speed_multiplier = 2.0f;
        }

        // Update rotation angles for all three axes
        g_rotation_angle_x += g_rotation_speed_x * speed_multiplier * elapsed_sec;
        g_rotation_angle_y += g_rotation_speed_y * speed_multiplier * elapsed_sec;
        g_rotation_angle_z += g_rotation_speed_z * speed_multiplier * elapsed_sec;

        // Keep angles in 0-2π range
        while (g_rotation_angle_x >= 2.0f * M_PI) {
            g_rotation_angle_x -= 2.0f * M_PI;
        }
        while (g_rotation_angle_x < 0.0f) {
            g_rotation_angle_x += 2.0f * M_PI;
        }
        while (g_rotation_angle_y >= 2.0f * M_PI) {
            g_rotation_angle_y -= 2.0f * M_PI;
        }
        while (g_rotation_angle_y < 0.0f) {
            g_rotation_angle_y += 2.0f * M_PI;
        }
        while (g_rotation_angle_z >= 2.0f * M_PI) {
            g_rotation_angle_z -= 2.0f * M_PI;
        }
        while (g_rotation_angle_z < 0.0f) {
            g_rotation_angle_z += 2.0f * M_PI;
        }
    }
    g_last_update_time = current_time;

    // Render 3D sphere
    render_sphere_3d();

    // Clear all pixels
    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    // Display rendered sphere with full color spectrum
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float brightness = g_display_buffer[x][y];
            float hue = g_hue_buffer[x][y];

            // Convert HSV to RGB (full color spectrum)
            uint32_t r, g, b;
            hsv_to_rgb(hue, 1.0f, brightness, &r, &g, &b); // Full saturation for vibrant colors

            // Apply brightness scaling
            PIXEL_COLOR_T pixel_color = {.red = (uint32_t)(r * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .green = (uint32_t)(g * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .blue = (uint32_t)(b * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .warm = 0,
                                         .cold = 0};

            uint32_t led_index = __matrix_coord_to_led_index((uint32_t)x, (uint32_t)y);
            if (led_index < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &pixel_color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}
#endif

/**
 * @brief Process audio data and calculate power
 */
static void process_audio_power(uint8_t *audio_data, uint32_t data_len)
{
    // Convert bytes to samples (16-bit PCM, mono)
    uint32_t num_samples = data_len / BYTES_PER_SAMPLE;
    if (num_samples > AUDIO_BUFFER_SIZE) {
        num_samples = AUDIO_BUFFER_SIZE;
    }

    // Convert PCM bytes to int16 samples
    int16_t *pcm_samples = (int16_t *)audio_data;

    // Shift buffer and add new samples
    if (num_samples >= AUDIO_BUFFER_SIZE) {
        // Replace entire buffer
        memcpy(g_audio_buffer, pcm_samples, AUDIO_BUFFER_SIZE * sizeof(int16_t));
    } else {
        // Shift existing samples left
        memmove(g_audio_buffer, &g_audio_buffer[num_samples], (AUDIO_BUFFER_SIZE - num_samples) * sizeof(int16_t));
        // Add new samples at the end
        memcpy(&g_audio_buffer[AUDIO_BUFFER_SIZE - num_samples], pcm_samples, num_samples * sizeof(int16_t));
    }

    // Calculate RMS power of the audio buffer
    float sum_squares = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        float sample = (float)g_audio_buffer[i];
        sum_squares += sample * sample;
        valid_samples++;
    }

    // Calculate RMS (Root Mean Square)
    float rms = 0.0f;
    if (valid_samples > 0) {
        rms = sqrtf(sum_squares / (float)valid_samples);
    }

    // Normalize power to 0.0-1.0 range
    float normalized_power = rms / POWER_NORMALIZATION;
    if (normalized_power > 1.0f)
        normalized_power = 1.0f;
    if (normalized_power < 0.0f)
        normalized_power = 0.0f;

    // Apply logarithmic scaling for better response
    normalized_power = log10f(1.0f + normalized_power * 9.0f); // Maps 0-1 to 0-1 with log curve

    // Update audio power (protected by mutex, non-blocking)
    if (g_uv_power_mutex != NULL) {
        if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
            g_audio_power = normalized_power;
            tal_mutex_unlock(g_uv_power_mutex);
        }
    }
}

/**
 * @brief Audio frame callback - NON-BLOCKING
 * Only collects data into ring buffer
 */
static void audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    // Only process PCM audio frames
    if (type != TDL_AUDIO_FRAME_FORMAT_PCM) {
        return;
    }

    // NON-BLOCKING: Just write to ring buffer, no processing here
    if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
        tal_mutex_lock(g_audio_rb_mutex);
        uint32_t before_size = tuya_ring_buff_used_size_get(g_audio_ringbuf);
        // Discard old data if buffer is getting full (keep it small for real-time)
        if (before_size > AUDIO_RINGBUF_SIZE / 2) {
            tuya_ring_buff_discard(g_audio_ringbuf, before_size - AUDIO_RINGBUF_SIZE / 4);
        }
        tuya_ring_buff_write(g_audio_ringbuf, data, len);
        tal_mutex_unlock(g_audio_rb_mutex);
    }
}

/**
 * @brief Audio processing task - NON-BLOCKING
 * Reads from ring buffer and processes in separate task
 */
static void audio_processing_task(void *args)
{
    uint8_t *audio_buffer = NULL;

    audio_buffer = (uint8_t *)tal_malloc(FRAME_SIZE_BYTES);
    if (audio_buffer == NULL) {
        PR_ERR("Failed to allocate audio buffer");
        return;
    }

    PR_NOTICE("Audio processing task started");

    while (1) {
        uint32_t available = 0;

        // Check available data in ring buffer (non-blocking)
        if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
            tal_mutex_lock(g_audio_rb_mutex);
            available = tuya_ring_buff_used_size_get(g_audio_ringbuf);
            tal_mutex_unlock(g_audio_rb_mutex);
        }

        if (available >= FRAME_SIZE_BYTES) {
            // Read one frame from ring buffer
            uint32_t read_len = 0;
            if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
                tal_mutex_lock(g_audio_rb_mutex);
                read_len = tuya_ring_buff_read(g_audio_ringbuf, audio_buffer, FRAME_SIZE_BYTES);
                tal_mutex_unlock(g_audio_rb_mutex);
            }

            if (read_len == FRAME_SIZE_BYTES) {
                // Process audio and update power (non-blocking)
                process_audio_power(audio_buffer, read_len);
            }
        } else {
            // Not enough data yet, wait a bit (non-blocking)
            tal_system_sleep(5);
        }
    }

    if (audio_buffer) {
        tal_free(audio_buffer);
    }
}

/**
 * @brief Main rendering task - Separate thread for smooth rendering
 */
static void sphere_rendering_task(void *args)
{
    PR_NOTICE("Sphere rendering task started");

    // Initialize rotation time
    g_last_update_time = tal_time_get_posix();

    while (1) {
#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
        uint32_t frame_start = tal_time_get_posix();

        // Render and display sphere
        sphere_display();

        // Calculate frame time and adjust sleep for consistent ~40-50 FPS (faster updates)
        uint32_t frame_time = tal_time_get_posix() - frame_start;
        uint32_t target_frame_time = 20; // ~50 FPS (20ms per frame) for smoother animation

        if (frame_time < target_frame_time) {
            tal_system_sleep(target_frame_time - frame_time);
        } else {
            // Frame took too long, don't sleep (maintain smoothness)
            tal_system_sleep(1); // Minimal sleep to yield CPU
        }
#else
        tal_system_sleep(33);
#endif
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
    PR_NOTICE("Tuya T5AI Pixel Sphere Effect");
    PR_NOTICE("==========================================");
    PR_NOTICE("3D rotating sphere on 32x32 LED display");
    PR_NOTICE("Audio power controls rotation speed");
    PR_NOTICE("==========================================");

    // Initialize hardware
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }
    PR_NOTICE("Hardware initialized");

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    // Initialize pixel LED
    rt = pixel_led_init();
    if (OPRT_OK != rt) {
        PR_ERR("Pixel LED initialization failed: %d", rt);
        return;
    }
    PR_NOTICE("Pixel LED initialized");
#endif

    // Initialize audio ring buffer
    rt = tuya_ring_buff_create(AUDIO_RINGBUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &g_audio_ringbuf);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create audio ring buffer: %d", rt);
        return;
    }
    PR_NOTICE("Audio ring buffer created");

    // Create mutexes for audio processing
    rt = tal_mutex_create_init(&g_audio_rb_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create audio ring buffer mutex: %d", rt);
        return;
    }
    PR_NOTICE("Audio ring buffer mutex created");

    rt = tal_mutex_create_init(&g_uv_power_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create UV power mutex: %d", rt);
        return;
    }
    PR_NOTICE("UV power mutex created");

    // Wait a bit to ensure audio driver registration is complete
    tal_system_sleep(200);

    // Find audio device
    rt = tdl_audio_find(AUDIO_CODEC_NAME, &g_audio_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to find audio device '%s': %d", AUDIO_CODEC_NAME, rt);
        return;
    }
    PR_NOTICE("Audio device found");

    // Open audio device with callback
    rt = tdl_audio_open(g_audio_handle, audio_frame_callback);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to open audio device: %d", rt);
        return;
    }
    PR_NOTICE("Audio device opened and started");

    // Initialize audio buffers
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));
    g_audio_power = 0.0f;

    // Initialize display buffers
    memset(g_display_buffer, 0, sizeof(g_display_buffer));
    memset(g_hue_buffer, 0, sizeof(g_hue_buffer));

    // Initialize rotation state with smooth speeds
    g_rotation_angle_x = 0.0f;
    g_rotation_angle_y = 0.0f;
    g_rotation_angle_z = 0.0f;
    g_rotation_speed_x = 0.4f; // Smooth default speeds (radians per second)
    g_rotation_speed_y = 0.5f;
    g_rotation_speed_z = 0.35f;
    g_rotation_change_time = 0;

    // Start audio processing task (non-blocking)
    THREAD_CFG_T audio_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_2, .thrdname = "audio_proc"};
    THREAD_HANDLE audio_thrd = NULL;
    rt = tal_thread_create_and_start(&audio_thrd, NULL, NULL, audio_processing_task, NULL, &audio_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start audio processing thread: %d", rt);
        return;
    }
    PR_NOTICE("Audio processing thread started");

    // Start sphere rendering task
    THREAD_CFG_T thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_2, .thrdname = "sphere_render"};
    THREAD_HANDLE sphere_thrd = NULL;
    rt = tal_thread_create_and_start(&sphere_thrd, NULL, NULL, sphere_rendering_task, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start sphere rendering thread: %d", rt);
        return;
    }
    PR_NOTICE("Sphere rendering thread started");

    PR_NOTICE("==========================================");
    PR_NOTICE("Sphere Effect Ready!");
    PR_NOTICE("==========================================");

    // Main loop
    int cnt = 0;
    while (1) {
        if (cnt % 100 == 0) {
            PR_DEBUG("Sphere effect running... (count: %d)", cnt);
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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
