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
#include "board_pixel_api.h"
#include "tdl_audio_manage.h"
#include "tuya_ringbuf.h"
#include "tdl_button_manage.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define LED_PIXELS_TOTAL_NUM 1027
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.1f // 2% brightness
#define MATRIX_WIDTH         32
#define MATRIX_HEIGHT        32

// 3D space configuration
#define SPACE_SIZE      32
#define SPHERE_RADIUS   16.0f
#define SPHERE_CENTER_X 16.0f
#define SPHERE_CENTER_Y 16.0f
#define SPHERE_CENTER_Z 16.0f
// Removed DISPLAY_PLANE_Z - viewing sphere top-down from center
#define BASE_ROTATION_SPEED 5.0f  // Base rotation speed (pixels per second) - smooth and visible
#define MAX_ROTATION_SPEED  15.0f // Maximum rotation speed with audio boost
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

static PIXEL_HANDLE_T g_pixels_handle = NULL;

// Audio processing
static TDL_AUDIO_HANDLE_T g_audio_handle = NULL;
static TUYA_RINGBUFF_T g_audio_ringbuf = NULL;
static MUTEX_HANDLE g_audio_rb_mutex = NULL;
static MUTEX_HANDLE g_uv_power_mutex = NULL;

static int16_t g_audio_buffer[AUDIO_BUFFER_SIZE];
static float g_audio_power = 0.0f; // Audio power (0.0-1.0), protected by mutex

// Double buffering for smooth rendering
// Render thread writes to back buffer, display thread reads from front buffer
#define NUM_BUFFERS 2
static struct {
    uint8_t r[MATRIX_WIDTH][MATRIX_HEIGHT]; // Pre-calculated RGB values
    uint8_t g[MATRIX_WIDTH][MATRIX_HEIGHT];
    uint8_t b[MATRIX_WIDTH][MATRIX_HEIGHT];
    bool ready; // Buffer is ready for display
} g_render_buffers[NUM_BUFFERS];

static int g_front_buffer = 0;             // Currently displayed buffer
static int g_back_buffer = 1;              // Currently being rendered to
static MUTEX_HANDLE g_buffer_mutex = NULL; // Protect buffer swapping

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

// Randomness for infinite variation
static float g_random_phase_x = 0.0f;     // Random phase offset for X patterns
static float g_random_hue_offset = 0.0f;  // Random hue offset
static uint32_t g_last_random_update = 0; // Last time randomness was updated

// Voice interaction state machine
typedef enum {
    VOICE_STATE_IDLE = 0,          // Initial state: 5px white circle at center and corner fading
    VOICE_STATE_START,             // Transition: white circle enlarges to animation size, mic responsive
    VOICE_STATE_PROCESSING,        // Processing: voxel gradient + red running ring
    VOICE_STATE_RESPONDING,        // Responding: voxel gradient without ring, mic responsive
    VOICE_STATE_TRANSITION_TO_IDLE // Transition back to idle: zoom out to white circle
} voice_state_t;

static voice_state_t g_voice_state = VOICE_STATE_IDLE;
static MUTEX_HANDLE g_voice_state_mutex = NULL;
static uint64_t g_state_transition_start_time = 0;      // Time in milliseconds (SYS_TICK_T)
static float g_idle_circle_radius = 2.5f;               // 5px diameter = 2.5px radius
static float g_target_animation_radius = SPHERE_RADIUS; // Target radius for animation
static float g_current_radius = 2.5f;                   // Current radius for transitions
static float g_running_ring_angle = 0.0f;               // Angle for first white running ring
static float g_running_ring_angle2 = 0.0f;              // Angle for second white running ring
static float g_start_state_peak_radius = 2.5f;          // Peak hold radius for START state

// Hot spots configuration (4 colored glowing points on sphere)
#define NUM_HOT_SPOTS 4
static struct {
    float x, y, z;        // Position on sphere (normalized)
    float intensity;      // Current intensity (0-1)
    float phase;          // Animation phase
    float speed;          // Animation speed
    float hue;            // Fixed hue for this hot spot (0-360)
    float base_intensity; // Base intensity (before audio modulation)
} g_hot_spots[NUM_HOT_SPOTS];

/***********************************************************
********************function declaration********************
***********************************************************/

static OPERATE_RET pixel_led_init(void);
// static void sphere_display(void);
static void render_sphere_3d(int buffer_idx);
static void display_sphere_fast(void);
static float calculate_sphere_hue(float x, float y, float z, float audio_power);
// static float calculate_hot_spot_intensity(float x, float y, float z, int *hotspot_idx);
// static void update_rotation_axes(void);
static void update_audio_reactive_effects(float audio_power);
static void initialize_hot_spots(void);
static void sphere_rendering_task(void *args);
static void sphere_display_task(void *args);

// Rendering engines
static void render_voxel_gradient_core(int buffer_idx, float radius, float audio_power, bool mic_responsive,
                                       float white_fade_factor);
static void render_idle_state(int buffer_idx);
static void render_start_state(int buffer_idx);
static void render_processing_state(int buffer_idx);
static void render_responding_state(int buffer_idx);
static void render_transition_to_idle_state(int buffer_idx);
static void render_running_ring(int buffer_idx, float angle1, float angle2);
static void update_voice_state_machine(void);
static void transition_to_state(voice_state_t new_state);

// Audio processing functions
static void audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len);
static void audio_processing_task(void *args);

// Button handling
static TDL_BUTTON_HANDLE g_button_ok_handle = NULL;
static void button_ok_callback(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void process_audio_power(uint8_t *audio_data, uint32_t data_len);

/***********************************************************
***********************function implementation**************
***********************************************************/

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
 * @brief Initialize hot spots on the sphere with specific colors
 * Colors: Blue, Green, Magenta, Light Purple
 */
static void initialize_hot_spots(void)
{
    // Create 4 hot spots at different positions on the sphere
    // Positions are normalized vectors (on unit sphere) - evenly distributed
    g_hot_spots[0].x = 0.707f;
    g_hot_spots[0].y = 0.707f;
    g_hot_spots[0].z = 0.0f; // Blue
    g_hot_spots[1].x = -0.707f;
    g_hot_spots[1].y = 0.0f;
    g_hot_spots[1].z = 0.707f; // Green
    g_hot_spots[2].x = 0.0f;
    g_hot_spots[2].y = -0.707f;
    g_hot_spots[2].z = 0.707f; // Magenta
    g_hot_spots[3].x = 0.577f;
    g_hot_spots[3].y = 0.577f;
    g_hot_spots[3].z = 0.577f; // Light Purple

    // Set specific colors for each hot spot
    g_hot_spots[0].hue = 240.0f; // Blue
    g_hot_spots[1].hue = 120.0f; // Green
    g_hot_spots[2].hue = 300.0f; // Magenta
    g_hot_spots[3].hue = 270.0f; // Light Purple (between magenta and blue)

    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        g_hot_spots[i].intensity = 0.0f;
        g_hot_spots[i].phase = (float)i * 2.0f * M_PI / NUM_HOT_SPOTS;
        g_hot_spots[i].speed = 0.3f + (float)i * 0.1f; // Slower speeds
        g_hot_spots[i].base_intensity = 0.4f;          // Base intensity
    }
}

/**
 * @brief Update audio-reactive effects (breathing, hue shift, hot spots)
 * Also updates random offsets for infinite variation
 */
static void update_audio_reactive_effects(float audio_power)
{
    // Smooth audio power with exponential moving average
    float alpha = 0.1f; // Smoothing factor
    g_audio_power_smoothed = alpha * audio_power + (1.0f - alpha) * g_audio_power_smoothed;

    // Sphere breathing effect - pulse radius with audio (audio power as intensity boost)
    static float breath_target = 0.0f;
    breath_target = g_audio_power_smoothed * 0.4f; // Max 40% size change (boosted)
    float breath_alpha = 0.05f;
    g_sphere_breath = breath_alpha * breath_target + (1.0f - breath_alpha) * g_sphere_breath;

    // Dynamic hue shift - colors shift based on audio power (faster with audio)
    g_hue_shift += 0.8f + g_audio_power_smoothed * 3.0f; // Faster shift with more audio
    if (g_hue_shift >= 360.0f) {
        g_hue_shift -= 360.0f;
    }

    // Update random hue offset for color transition and morphing (prolonged, smooth changes)
    uint32_t current_time = tal_time_get_posix();
    // Update hue offset less frequently for prolonged color transitions (every 5-8 seconds)
    if (g_last_random_update == 0 || (current_time - g_last_random_update) > (5 + (current_time % 3))) {
        // Random hue offset for color morphing - full spectrum (0-360 degrees)
        g_random_hue_offset = ((float)((current_time * 11) % 1000) / 1000.0f) * 360.0f; // 0 to 360 degrees

        g_last_random_update = current_time;
    }

    // Update hot spots - pulse with audio (audio power as intensity boost)
    for (int i = 0; i < NUM_HOT_SPOTS; i++) {
        // Add randomness to phase speed
        float random_speed_mod = 0.8f + ((float)((current_time * (i + 1) * 17) % 400) / 1000.0f);
        g_hot_spots[i].phase += g_hot_spots[i].speed * 0.01f * random_speed_mod;
        if (g_hot_spots[i].phase >= 2.0f * M_PI) {
            g_hot_spots[i].phase -= 2.0f * M_PI;
        }

        // Hot spot intensity pulses with audio (audio power as intensity boost)
        float base_intensity = g_hot_spots[i].base_intensity + g_audio_power_smoothed * 0.8f; // Boosted
        float pulse = 0.5f * (1.0f + sinf(g_hot_spots[i].phase + g_random_phase_x * 0.3f));   // Add randomness
        g_hot_spots[i].intensity =
            base_intensity * (0.5f + 0.5f * pulse * (1.0f + g_audio_power_smoothed)); // Audio boosts pulse
    }
}

// /**
//  * @brief Calculate hot spot intensity and return which hot spot (if any)
//  * @return Intensity (0-1), hotspot_idx set to -1 if no hot spot, or index of closest hot spot
//  */
// static float calculate_hot_spot_intensity(float x, float y, float z, int *hotspot_idx)
// {
//     *hotspot_idx = -1;
//     float max_intensity = 0.0f;
//     int best_hotspot = -1;

//     // Calculate position relative to sphere center
//     float dx = x - SPHERE_CENTER_X;
//     float dy = y - SPHERE_CENTER_Y;
//     float dz = z - SPHERE_CENTER_Z;

//     // Normalize to unit sphere
//     float dist_sq = dx * dx + dy * dy + dz * dz;
//     if (dist_sq < 0.0001f)
//         return 0.0f;
//     float dist = sqrtf(dist_sq);
//     float nx = dx / dist;
//     float ny = dy / dist;
//     float nz = dz / dist;

//     // Check distance to each hot spot (optimized)
//     float hotspot_radius_cos = cosf(M_PI / 4.0f); // 45 degrees - larger hot spots
//     for (int i = 0; i < NUM_HOT_SPOTS; i++) {
//         // Dot product gives cosine of angle
//         float dot = nx * g_hot_spots[i].x + ny * g_hot_spots[i].y + nz * g_hot_spots[i].z;

//         // Use cosine comparison (faster - no acos needed)
//         if (dot > hotspot_radius_cos) {
//             // Smooth falloff (map dot from hotspot_radius_cos to 1.0)
//             float normalized = (dot - hotspot_radius_cos) / (1.0f - hotspot_radius_cos);
//             float falloff = normalized * normalized; // Quadratic falloff for smoother edges
//             float intensity = g_hot_spots[i].intensity * falloff;

//             if (intensity > max_intensity) {
//                 max_intensity = intensity;
//                 best_hotspot = i;
//             }
//         }
//     }

//     *hotspot_idx = best_hotspot;

//     // Clamp intensity
//     if (max_intensity > 1.0f) {
//         max_intensity = 1.0f;
//     }

//     return max_intensity;
// }

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

    // Full color spectrum (0-360 degrees) - no limits
    float base_hue = (azimuth / (2.0f * M_PI)) * 360.0f;
    float hue = base_hue + (elevation_normalized - 0.5f) * 180.0f;

    // Audio-reactive hue shift - full spectrum
    hue += g_hue_shift * 0.5f; // Smooth hue shift for color morphing

    // Keep hue in 0-360 range
    while (hue >= 360.0f)
        hue -= 360.0f;
    while (hue < 0.0f)
        hue += 360.0f;

    return hue;
}

/**
 * @brief Transition to a new voice state
 */
static void transition_to_state(voice_state_t new_state)
{
    if (g_voice_state_mutex != NULL) {
        if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
            g_voice_state = new_state;
            // Store time in milliseconds for accurate timing
            g_state_transition_start_time = tal_time_get_posix_ms();
            tal_mutex_unlock(g_voice_state_mutex);
        }
    }
}

/**
 * @brief Update voice state machine - handles automatic transitions
 */
static void update_voice_state_machine(void)
{
    // voice_state_t current_state;
    if (g_voice_state_mutex != NULL) {
        if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
            // current_state = g_voice_state;
            tal_mutex_unlock(g_voice_state_mutex);
        } else {
            return;
        }
    } else {
        return;
    }

    // State transitions are now button-controlled, not timer-based
    // All transitions happen via button_ok_callback()
}

/**
 * @brief Render 3D sphere - routes to appropriate engine based on state
 */
static void render_sphere_3d(int buffer_idx)
{
    voice_state_t current_state;
    if (g_voice_state_mutex != NULL) {
        if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
            current_state = g_voice_state;
            tal_mutex_unlock(g_voice_state_mutex);
        } else {
            current_state = VOICE_STATE_IDLE;
        }
    } else {
        current_state = VOICE_STATE_IDLE;
    }

    switch (current_state) {
    case VOICE_STATE_IDLE:
        render_idle_state(buffer_idx);
        break;
    case VOICE_STATE_START:
        render_start_state(buffer_idx);
        break;
    case VOICE_STATE_PROCESSING:
        render_processing_state(buffer_idx);
        break;
    case VOICE_STATE_RESPONDING:
        render_responding_state(buffer_idx);
        break;
    case VOICE_STATE_TRANSITION_TO_IDLE:
        render_transition_to_idle_state(buffer_idx);
        break;
    default:
        render_idle_state(buffer_idx);
        break;
    }
}

/**
 * @brief Helper function: Render voxel gradient sphere with configurable parameters
 * @param buffer_idx Buffer index to render to
 * @param radius Sphere radius
 * @param audio_power Audio power for reactivity
 * @param mic_responsive Whether brightness should react to mic RMS
 * @param white_fade_factor Factor to fade to white (0.0 = no fade, 1.0 = fully white)
 */
static void render_voxel_gradient_core(int buffer_idx, float radius, float audio_power, bool mic_responsive,
                                       float white_fade_factor)
{
    // audio_power is passed as parameter, use it directly

    // Apply audio-reactive breathing to radius if mic responsive
    float base_radius = radius;
    if (mic_responsive) {
        base_radius = radius * (1.0f + g_sphere_breath * 0.5f);
    } else {
        base_radius = radius * (1.0f + g_sphere_breath);
    }
    float current_radius = base_radius;

    // Small constant Z-axis rotation for effect 2
    static float effect2_rotation_z = 0.0f;
    static uint32_t effect2_last_time = 0;
    uint32_t current_time = tal_time_get_posix();
    if (effect2_last_time > 0) {
        float elapsed_sec = (float)(current_time - effect2_last_time);
        if (elapsed_sec > 0.1f) {
            elapsed_sec = 0.1f; // Cap at 100ms to prevent large jumps
        }
        // Small constant rotation speed: 0.4 radians per second
        float rotation_speed_z = 0.5f;
        effect2_rotation_z += rotation_speed_z * elapsed_sec;
        // Keep angle in 0-2π range
        while (effect2_rotation_z >= 2.0f * M_PI)
            effect2_rotation_z -= 2.0f * M_PI;
        while (effect2_rotation_z < 0.0f)
            effect2_rotation_z += 2.0f * M_PI;
    }
    effect2_last_time = current_time;

    // Pre-calculate rotation matrices - only Z axis rotation for effect 2

    float cos_z = cosf(effect2_rotation_z);
    float sin_z = sinf(effect2_rotation_z);

    float radius_sq = current_radius * current_radius;

    // Temporary buffers
    static float temp_brightness[MATRIX_WIDTH][MATRIX_HEIGHT];
    static float temp_hue[MATRIX_WIDTH][MATRIX_HEIGHT];
    memset(temp_brightness, 0, sizeof(temp_brightness));
    memset(temp_hue, 0, sizeof(temp_hue));

    // Iterate through Z layers (voxel approach)
    for (int z = 0; z < SPACE_SIZE; z++) {
        float z_world = (float)z;
        float dz = z_world - SPHERE_CENTER_Z;

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                float x_world = (float)x;
                float y_world = (float)y;
                float dx = x_world - SPHERE_CENTER_X;
                float dy = y_world - SPHERE_CENTER_Y;

                // Rotate point around Z axis only (simplified rotation for effect 2)
                // Z-axis rotation: z stays same, x and y rotate around z
                float rx = dx * cos_z - dy * sin_z;
                float ry = dx * sin_z + dy * cos_z;
                float rz = dz;

                // Check if inside sphere
                float dist_sq = rx * rx + ry * ry + rz * rz;
                if (dist_sq <= radius_sq) {
                    // float dist = sqrtf(dist_sq);
                    // float nx = rx / current_radius;
                    // float ny = ry / current_radius;
                    float nz = rz / current_radius;

                    // Top-down view - brightness based on normal Z component (facing up)
                    // No depth factor - all rotations based on sphere center
                    float brightness = fmaxf(0.0f, nz * 0.5f + 0.5f);

                    // Audio power as intensity boost (multiplicative, not just additive)
                    float audio_intensity_boost = 0.4f + audio_power * 1.6f; // 0.4x to 2.0x boost
                    brightness *= audio_intensity_boost;

                    // Hue based on position with gradient - smooth color transition and morphing
                    // Full color spectrum (0-360 degrees) - no limits
                    float hue = calculate_sphere_hue(rx + SPHERE_CENTER_X, ry + SPHERE_CENTER_Y, rz + SPHERE_CENTER_Z,
                                                     audio_power);
                    // Smooth color morphing - use hue shift for prolonged transitions
                    hue += g_hue_shift * 0.5f;         // Smooth hue shift for color morphing
                    hue += g_random_hue_offset * 0.2f; // Random hue offset for color variation
                    // No time-based pixel-level randomness - only smooth global transitions

                    // Wrap to 0-360 range
                    while (hue >= 360.0f)
                        hue -= 360.0f;
                    while (hue < 0.0f)
                        hue += 360.0f;

                    // Accumulate (summation)
                    temp_brightness[x][y] += brightness;
                    // Weighted average hue
                    if (temp_brightness[x][y] > 0.001f) {
                        float weight = brightness / (temp_brightness[x][y] + 0.001f);
                        temp_hue[x][y] = temp_hue[x][y] * (1.0f - weight) + hue * weight;
                    }
                }
            }
        }
    }

    // Normalize and render
    float max_brightness = 0.0f;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (temp_brightness[x][y] > max_brightness) {
                max_brightness = temp_brightness[x][y];
            }
        }
    }

    if (max_brightness > 0.001f) {
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                float brightness = temp_brightness[x][y] / max_brightness;
                if (brightness > 1.0f)
                    brightness = 1.0f;
                float hue = temp_hue[x][y];
                while (hue >= 360.0f)
                    hue -= 360.0f;
                while (hue < 0.0f)
                    hue += 360.0f;

                uint32_t r, g, b;
                // Dynamic saturation for white/pink effects - lower saturation creates white/pastel tones
                // Higher brightness areas can have lower saturation for white effect
                float base_saturation = 0.6f + audio_power * 0.4f; // 0.6 to 1.0 base saturation
                // Reduce saturation in brighter areas to create white/pink tones
                float white_factor =
                    (brightness > 0.7f) ? (brightness - 0.7f) * 0.5f : 0.0f; // Reduce sat in bright areas
                float saturation = base_saturation * (1.0f - white_factor);  // Lower sat = more white
                if (saturation < 0.2f)
                    saturation = 0.2f; // Minimum saturation to avoid pure gray

                // Brightness handling - mic responsive or fixed
                float final_brightness;
                if (mic_responsive) {
                    // Responsive to mic RMS - brightness reacts to audio
                    final_brightness = brightness * (0.5f + audio_power * 0.5f); // 0.5x to 1.0x brightness
                } else {
                    // Less reactive - fixed brightness
                    final_brightness = brightness * 0.7f;
                }

                // Apply white fade if needed
                if (white_fade_factor > 0.001f) {
                    board_pixel_hsv_to_rgb(hue, saturation, final_brightness, &r, &g, &b);
                    // Blend between colored and white based on white_fade_factor
                    float white_r = 255 * final_brightness;
                    float white_g = 255 * final_brightness;
                    float white_b = 255 * final_brightness;
                    g_render_buffers[buffer_idx].r[x][y] =
                        (uint8_t)(r * (1.0f - white_fade_factor) + white_r * white_fade_factor);
                    g_render_buffers[buffer_idx].g[x][y] =
                        (uint8_t)(g * (1.0f - white_fade_factor) + white_g * white_fade_factor);
                    g_render_buffers[buffer_idx].b[x][y] =
                        (uint8_t)(b * (1.0f - white_fade_factor) + white_b * white_fade_factor);
                } else {
                    board_pixel_hsv_to_rgb(hue, saturation, final_brightness, &r, &g, &b);
                    g_render_buffers[buffer_idx].r[x][y] = (uint8_t)r;
                    g_render_buffers[buffer_idx].g[x][y] = (uint8_t)g;
                    g_render_buffers[buffer_idx].b[x][y] = (uint8_t)b;
                }
            }
        }
    } else {
        memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
        memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
        memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));
    }

    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render PROCESSING state: voxel gradient + red running ring
 */
static void render_processing_state(int buffer_idx)
{
    // Processing state should not react to sound intensity
    // Pass 0.0f for audio_power to disable all audio-reactive effects
    float audio_power = 0.0f;

    // Render voxel gradient (not mic responsive, no white fade, no audio reaction)
    render_voxel_gradient_core(buffer_idx, SPHERE_RADIUS, audio_power, false, 0.0f);

    // Add two white running rings
    float ring_speed = 0.3f; // radians per frame (increased from 0.1f for faster rotation)
    g_running_ring_angle += ring_speed;
    if (g_running_ring_angle >= 2.0f * M_PI) {
        g_running_ring_angle -= 2.0f * M_PI;
    }

    // Second ring rotates in opposite direction for visual variety
    g_running_ring_angle2 -= ring_speed * 0.8f; // Slightly slower and opposite direction
    if (g_running_ring_angle2 < 0.0f) {
        g_running_ring_angle2 += 2.0f * M_PI;
    }

    render_running_ring(buffer_idx, g_running_ring_angle, g_running_ring_angle2);
}

/**
 * @brief Render RESPONDING state: voxel gradient without ring, mic responsive
 */
static void render_responding_state(int buffer_idx)
{
    // Get audio power
    float audio_power = 0.0f;
    if (g_uv_power_mutex != NULL) {
        if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
            audio_power = g_audio_power;
            tal_mutex_unlock(g_uv_power_mutex);
        }
    }

    // Render voxel gradient (mic responsive, no white fade, no ring)
    render_voxel_gradient_core(buffer_idx, SPHERE_RADIUS, audio_power, true, 0.0f);

    // Add white hotspot that reacts to speech intensity
    // render_white_hotspot(buffer_idx, audio_power);
}

/**
 * @brief Render TRANSITION_TO_IDLE state: voxel gradient fading to white, zooming out
 */
static void render_transition_to_idle_state(int buffer_idx)
{
    // Get audio power
    float audio_power = 0.0f;
    if (g_uv_power_mutex != NULL) {
        if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
            audio_power = g_audio_power;
            tal_mutex_unlock(g_uv_power_mutex);
        }
    }

    // Calculate transition progress (in milliseconds)
    uint64_t state_time_ms = tal_time_get_posix_ms();
    uint64_t transition_time_ms = 0;
    if (g_state_transition_start_time > 0) {
        transition_time_ms = state_time_ms - g_state_transition_start_time;
    }

    float target_radius = SPHERE_RADIUS;
    float transition_duration = 500.0f; // 500ms transition
    float white_fade_factor = 0.0f;
    float current_radius = target_radius;

    // Transition back to idle circle
    if (transition_time_ms < transition_duration) {
        float t = (float)transition_time_ms / transition_duration;
        // Smooth ease-in transition
        t = t * t;
        current_radius = target_radius - (target_radius - g_idle_circle_radius) * t;
        // Fade to white as we transition
        white_fade_factor = 1.0f - t;
    } else {
        current_radius = g_idle_circle_radius;
        white_fade_factor = 1.0f; // Fully white when transition completes
    }

    // Update global radius
    g_current_radius = current_radius;

    // Render voxel gradient with white fade (not mic responsive during transition)
    render_voxel_gradient_core(buffer_idx, current_radius, audio_power, false, white_fade_factor);
}

/**
 * @brief Render idle state: breathing white circle at center only
 */
static void render_idle_state(int buffer_idx)
{
    // Clear buffer (all black)
    memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
    memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
    memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));

    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;

    // Breathing animation - radius pulses between min and max
    uint32_t current_time = tal_time_get_posix();
    float pulse = 0.5f + 0.5f * sinf((float)current_time * 0.001f); // Slow breathing pulse

    // Base radius with breathing effect (pulses between 2px and 3px radius)
    float min_radius = 2.0f;
    float max_radius = 3.0f;
    float radius = min_radius + (max_radius - min_radius) * pulse;
    float radius_sq = radius * radius;

    // Brightness also pulses with breathing
    float brightness = 0.6f + 0.4f * pulse; // Pulse between 0.6 and 1.0 brightness

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist_sq = dx * dx + dy * dy;

            // Only render circle at center - no corner fading
            if (dist_sq <= radius_sq) {
                float dist = sqrtf(dist_sq);
                // Smooth falloff from center to edge
                float normalized_dist = dist / radius;
                float edge_falloff = 1.0f - normalized_dist * 0.5f; // Smooth edge
                if (edge_falloff < 0.0f)
                    edge_falloff = 0.0f;

                float final_brightness = brightness * edge_falloff;
                g_render_buffers[buffer_idx].r[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].g[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].b[x][y] = (uint8_t)(255 * final_brightness);
            }
            // Everything else stays black (already cleared above)
        }
    }

    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render START state: red circle that grows with audio RMS (5px to 32x32)
 */
static void render_start_state(int buffer_idx)
{
    // Get audio power (RMS) for responsiveness
    float audio_power = 0.0f;
    if (g_uv_power_mutex != NULL) {
        if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
            audio_power = g_audio_power;
            tal_mutex_unlock(g_uv_power_mutex);
        }
    }

    // Clear buffer (all black)
    memset(g_render_buffers[buffer_idx].r, 0, sizeof(g_render_buffers[buffer_idx].r));
    memset(g_render_buffers[buffer_idx].g, 0, sizeof(g_render_buffers[buffer_idx].g));
    memset(g_render_buffers[buffer_idx].b, 0, sizeof(g_render_buffers[buffer_idx].b));

    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;

    // Circle diameter normalized with audio RMS:
    // - Minimum: 5px diameter (2.5px radius) when silent
    // - Maximum: Full screen width (32px diameter = 16px radius) when loud
    float min_radius = 2.5f;  // 5px diameter / 2
    float max_radius = 16.0f; // Full screen width (32px diameter)

    // Focus on small voice signals - use aggressive scaling for low-level audio
    // Strategy: Amplify small signals and compress large ones
    float responsive_power = 0.0f;

    if (audio_power > 0.001f) { // Only process if there's actual audio
        // Use square root curve to emphasize low values (inverse of square)
        // This makes small signals map to larger radius values
        responsive_power = sqrtf(audio_power); // sqrt amplifies low values

        // Further amplify small signals with gain
        // Map: 0.0-0.3 audio_power -> 0.0-0.7 responsive_power (amplified)
        //      0.3-1.0 audio_power -> 0.7-1.0 responsive_power (compressed)
        if (audio_power < 0.3f) {
            // Small signals: amplify by 2.3x (0.3 -> 0.7)
            responsive_power = responsive_power * 2.3f;
            if (responsive_power > 0.7f)
                responsive_power = 0.7f;
        } else {
            // Larger signals: compress remaining range (0.3-1.0 -> 0.7-1.0)
            float compressed = (audio_power - 0.3f) / 0.7f; // Normalize 0.3-1.0 to 0-1
            responsive_power = 0.7f + compressed * 0.3f;    // Map to 0.7-1.0
        }

        // Ensure we stay in 0-1 range
        if (responsive_power > 1.0f)
            responsive_power = 1.0f;
        if (responsive_power < 0.0f)
            responsive_power = 0.0f;
    }

    // Normalize audio power to control radius (0.0 = min, 1.0 = max)
    // Map directly from 5px to full screen width
    float current_radius = min_radius + (max_radius - min_radius) * responsive_power;

    // Peak hold and decay (like spectrum meter bars)
    // Update peak if current radius is larger
    if (current_radius > g_start_state_peak_radius) {
        g_start_state_peak_radius = current_radius;
    }

    // Decay peak radius (decay by 0.3 pixels per frame for smooth decay)
    if (g_start_state_peak_radius > min_radius) {
        g_start_state_peak_radius -= 0.3f;
        if (g_start_state_peak_radius < min_radius) {
            g_start_state_peak_radius = min_radius;
        }
    }

    // Use peak radius for display (shows peak hold effect)
    float display_radius = g_start_state_peak_radius;
    float radius_sq = display_radius * display_radius;

    // Red color - brightness responds to audio
    float brightness = 0.7f + responsive_power * 0.3f; // 0.7 to 1.0 brightness

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= radius_sq) {
                // Inside circle - render red with smooth falloff
                float dist = sqrtf(dist_sq);
                float normalized_dist = dist / display_radius;
                // Smooth falloff from center to edge
                float edge_falloff = 1.0f - normalized_dist * 0.3f;
                if (edge_falloff < 0.0f)
                    edge_falloff = 0.0f;

                float final_brightness = brightness * edge_falloff;

                // Red LED only
                g_render_buffers[buffer_idx].r[x][y] = (uint8_t)(255 * final_brightness);
                g_render_buffers[buffer_idx].g[x][y] = 0;
                g_render_buffers[buffer_idx].b[x][y] = 0;
            }
            // Everything else stays black
        }
    }

    // Update global radius for smooth transitions to next state
    g_current_radius = current_radius;

    g_render_buffers[buffer_idx].ready = true;
}

/**
 * @brief Render red running ring at 32x32 1px circle
 */
static void render_running_ring(int buffer_idx, float angle1, float angle2)
{
    float center_x = SPHERE_CENTER_X;
    float center_y = SPHERE_CENTER_Y;
    float ring_radius = 15.5f; // Approximate radius for 32x32 circle (1px wide)

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float dist = sqrtf(dx * dx + dy * dy);

            float brightness = 0.0f;

            // Check if pixel is on the ring (1px wide)
            if (fabsf(dist - ring_radius) < 0.7f) {
                // Calculate angle of this pixel
                float pixel_angle = atan2f(dy, dx);
                if (pixel_angle < 0.0f)
                    pixel_angle += 2.0f * M_PI;

                // First ring - running effect with bright spot
                float angle_diff1 = fabsf(pixel_angle - angle1);
                if (angle_diff1 > M_PI)
                    angle_diff1 = 2.0f * M_PI - angle_diff1;

                if (angle_diff1 < 0.6f) { // ~34 degree bright spot (half length)
                    float ring_brightness = 1.0f - (angle_diff1 / 0.6f);
                    if (ring_brightness > brightness) {
                        brightness = ring_brightness;
                    }
                }

                // Second ring - running effect with bright spot
                float angle_diff2 = fabsf(pixel_angle - angle2);
                if (angle_diff2 > M_PI)
                    angle_diff2 = 2.0f * M_PI - angle_diff2;

                if (angle_diff2 < 0.6f) { // ~34 degree bright spot (half length)
                    float ring_brightness = 1.0f - (angle_diff2 / 0.6f);
                    if (ring_brightness > brightness) {
                        brightness = ring_brightness;
                    }
                }
            }

            // Render if brightness > 0
            if (brightness > 0.0f) {
                // Overlay white rings on existing buffer (additive blending)
                uint8_t existing_r = g_render_buffers[buffer_idx].r[x][y];
                uint8_t existing_g = g_render_buffers[buffer_idx].g[x][y];
                uint8_t existing_b = g_render_buffers[buffer_idx].b[x][y];

                // Add white rings with brightness (equal RGB for white)
                uint8_t ring_brightness = (uint8_t)(255 * brightness);
                g_render_buffers[buffer_idx].r[x][y] =
                    (existing_r + ring_brightness > 255) ? 255 : (existing_r + ring_brightness);
                g_render_buffers[buffer_idx].g[x][y] =
                    (existing_g + ring_brightness > 255) ? 255 : (existing_g + ring_brightness);
                g_render_buffers[buffer_idx].b[x][y] =
                    (existing_b + ring_brightness > 255) ? 255 : (existing_b + ring_brightness);
            }
        }
    }
}

// /**
//  * @brief Render white hotspot that reacts to speech intensity
//  */
// static void render_white_hotspot(int buffer_idx, float audio_power)
// {
//     float center_x = SPHERE_CENTER_X;
//     float center_y = SPHERE_CENTER_Y;

//     // Hotspot radius scales with audio power: 1px (silent) to 8px (loud)
//     float min_radius = 1.0f;
//     float max_radius = 8.0f;
//     float hotspot_radius = min_radius + (max_radius - min_radius) * audio_power;
//     if (hotspot_radius > max_radius)
//         hotspot_radius = max_radius;
//     if (hotspot_radius < min_radius)
//         hotspot_radius = min_radius;

//     // Brightness also scales with audio power: 0.3 (silent) to 1.0 (loud)
//     float min_brightness = 0.3f;
//     float max_brightness = 1.0f;
//     float hotspot_brightness = min_brightness + (max_brightness - min_brightness) * audio_power;
//     if (hotspot_brightness > max_brightness)
//         hotspot_brightness = max_brightness;
//     if (hotspot_brightness < min_brightness)
//         hotspot_brightness = min_brightness;

//     float radius_sq = hotspot_radius * hotspot_radius;

//     for (int y = 0; y < MATRIX_HEIGHT; y++) {
//         for (int x = 0; x < MATRIX_WIDTH; x++) {
//             float dx = (float)x - center_x;
//             float dy = (float)y - center_y;
//             float dist_sq = dx * dx + dy * dy;

//             // Check if pixel is within hotspot radius
//             if (dist_sq <= radius_sq) {
//                 float dist = sqrtf(dist_sq);
//                 float normalized_dist = dist / hotspot_radius;

//                 // Smooth falloff from center to edge
//                 float edge_falloff = 1.0f - normalized_dist * 0.5f;
//                 if (edge_falloff < 0.0f)
//                     edge_falloff = 0.0f;

//                 // Final brightness combines hotspot brightness and edge falloff
//                 float final_brightness = hotspot_brightness * edge_falloff;

//                 // Overlay white hotspot on existing buffer (additive blending)
//                 uint8_t existing_r = g_render_buffers[buffer_idx].r[x][y];
//                 uint8_t existing_g = g_render_buffers[buffer_idx].g[x][y];
//                 uint8_t existing_b = g_render_buffers[buffer_idx].b[x][y];

//                 // Add white hotspot (equal RGB for white)
//                 uint8_t hotspot_value = (uint8_t)(255 * final_brightness);
//                 g_render_buffers[buffer_idx].r[x][y] =
//                     (existing_r + hotspot_value > 255) ? 255 : (existing_r + hotspot_value);
//                 g_render_buffers[buffer_idx].g[x][y] =
//                     (existing_g + hotspot_value > 255) ? 255 : (existing_g + hotspot_value);
//                 g_render_buffers[buffer_idx].b[x][y] =
//                     (existing_b + hotspot_value > 255) ? 255 : (existing_b + hotspot_value);
//             }
//         }
//     }
// }

/**
 * @brief Button OK callback - triggers voice interaction state machine transitions
 */
static void button_ok_callback(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        voice_state_t current_state;
        if (g_voice_state_mutex != NULL) {
            if (tal_mutex_lock(g_voice_state_mutex) == OPRT_OK) {
                current_state = g_voice_state;
                tal_mutex_unlock(g_voice_state_mutex);
            } else {
                return;
            }
        } else {
            return;
        }

        // Handle state transitions based on current state
        switch (current_state) {
        case VOICE_STATE_IDLE:
            // IDLE -> START: Begin voice interaction
            PR_NOTICE("OK Button: IDLE -> START");
            transition_to_state(VOICE_STATE_START);
            break;
        case VOICE_STATE_START:
            // START -> PROCESSING: Start processing voice
            PR_NOTICE("OK Button: START -> PROCESSING");
            transition_to_state(VOICE_STATE_PROCESSING);
            break;
        case VOICE_STATE_PROCESSING:
            // PROCESSING -> RESPONDING: Processing complete, start responding
            PR_NOTICE("OK Button: PROCESSING -> RESPONDING");
            transition_to_state(VOICE_STATE_RESPONDING);
            break;
        case VOICE_STATE_RESPONDING:
            // RESPONDING -> TRANSITION_TO_IDLE: End conversation
            PR_NOTICE("OK Button: RESPONDING -> TRANSITION_TO_IDLE");
            transition_to_state(VOICE_STATE_TRANSITION_TO_IDLE);
            break;
        case VOICE_STATE_TRANSITION_TO_IDLE:
            // TRANSITION_TO_IDLE -> IDLE: Skip transition, go directly to idle
            PR_NOTICE("OK Button: TRANSITION_TO_IDLE -> IDLE");
            transition_to_state(VOICE_STATE_IDLE);
            break;
        default:
            PR_NOTICE("OK Button pressed - unknown state: %d", current_state);
            break;
        }
    }
}

/**
 * @brief Fast display update - just copies pre-rendered RGB buffer to LEDs
 * This runs in a separate high-priority thread for smooth animation
 */
static void display_sphere_fast(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    // Lock buffer mutex to safely swap buffers
    if (g_buffer_mutex != NULL && tal_mutex_lock(g_buffer_mutex) == OPRT_OK) {
        // Check if back buffer is ready, swap if so
        if (g_render_buffers[g_back_buffer].ready) {
            // Swap buffers
            int temp = g_front_buffer;
            g_front_buffer = g_back_buffer;
            g_back_buffer = temp;
            g_render_buffers[g_back_buffer].ready = false; // Mark as being rendered
        }
        tal_mutex_unlock(g_buffer_mutex);
    }

    // Clear all pixels
    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    // Fast display - just copy pre-calculated RGB values
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t r = g_render_buffers[g_front_buffer].r[x][y];
            uint8_t g_val = g_render_buffers[g_front_buffer].g[x][y];
            uint8_t b = g_render_buffers[g_front_buffer].b[x][y];

            // Apply brightness scaling
            // Note: If LEDs show GRB order instead of RGB, swap channels here
            // For now, keeping standard RGB mapping - if red shows as green, swap r and g_val
            PIXEL_COLOR_T pixel_color = {.red = (uint32_t)(g_val * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .green = (uint32_t)(r * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .blue = (uint32_t)(b * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                         .warm = 0,
                                         .cold = 0};

            uint32_t led_index = board_pixel_matrix_coord_to_led_index((uint32_t)x, (uint32_t)y);
            if (led_index < LED_PIXELS_TOTAL_NUM) {
                tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &pixel_color);
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

// /**
//  * @brief Update rotation axes - smooth constant rotation (no randomness)
//  */
// static void update_rotation_axes(void)
// {
//     // Constant smooth rotation speeds - balanced to maintain circular appearance
//     // Initialize once if not set
//     // Use same speed for all axes to ensure sphere remains circular on 2D plane
//     if (g_rotation_speed_x == 0.0f && g_rotation_speed_y == 0.0f && g_rotation_speed_z == 0.0f) {
//         float base_speed = 0.15f; // Same base speed for all axes
//         g_rotation_speed_x = base_speed;
//         g_rotation_speed_y = base_speed;
//         g_rotation_speed_z = base_speed;
//     }
//     // Keep speeds constant and balanced - no random changes
// }

/**
 * @brief Sphere rendering task - SLOW calculations in separate thread
 * This thread does all the heavy 3D math and pre-calculates RGB values
 */
static void sphere_rendering_task(void *args)
{
    PR_NOTICE("Sphere rendering task started");

    g_last_update_time = tal_time_get_posix();

    while (1) {
        // Update rotation angles based on time and audio power
        uint32_t current_time = tal_time_get_posix();
        if (g_last_update_time > 0) {
            float elapsed_sec = (float)(current_time - g_last_update_time);
            if (elapsed_sec > 0.1f) {
                elapsed_sec = 0.1f;
            }
            if (elapsed_sec < 0.0f) {
                elapsed_sec = 0.0f;
            }

            // Get audio power (non-blocking)
            float audio_power = 0.0f;
            if (g_uv_power_mutex != NULL) {
                if (tal_mutex_lock(g_uv_power_mutex) == OPRT_OK) {
                    audio_power = g_audio_power;
                    tal_mutex_unlock(g_uv_power_mutex);
                }
            }

            // Update audio-reactive effects
            update_audio_reactive_effects(audio_power);

            // Update voice state machine
            update_voice_state_machine();

            // Update rotation axes
            // update_rotation_axes();

            // Calculate rotation speed multiplier - audio power as intensity boost
            // Smooth constant rotation with audio-reactive speed
            // float speed_multiplier = 1.0f + (UV_SPEED_MULTIPLIER * 1.5f - 1.0f) * audio_power;
            // if (speed_multiplier > 3.0f) {
            //     speed_multiplier = 3.0f;
            // }
            // int speed_multiplier = 1;
            // No random variation - keep rotation smooth and constant

            // // Update rotation angles
            // g_rotation_angle_x += g_rotation_speed_x * speed_multiplier * elapsed_sec;
            // g_rotation_angle_y += g_rotation_speed_y * speed_multiplier * elapsed_sec;
            // g_rotation_angle_z += g_rotation_speed_z * speed_multiplier * elapsed_sec;

            // // Keep angles in 0-2π range
            // while (g_rotation_angle_x >= 2.0f * M_PI)
            //     g_rotation_angle_x -= 2.0f * M_PI;
            // while (g_rotation_angle_x < 0.0f)
            //     g_rotation_angle_x += 2.0f * M_PI;
            // while (g_rotation_angle_y >= 2.0f * M_PI)
            //     g_rotation_angle_y -= 2.0f * M_PI;
            // while (g_rotation_angle_y < 0.0f)
            //     g_rotation_angle_y += 2.0f * M_PI;
            // while (g_rotation_angle_z >= 2.0f * M_PI)
            //     g_rotation_angle_z -= 2.0f * M_PI;
            // while (g_rotation_angle_z < 0.0f)
            //     g_rotation_angle_z += 2.0f * M_PI;
        }
        g_last_update_time = current_time;

        // Render 3D sphere to back buffer (SLOW - can take time)
        // Only render if back buffer is not ready (not being displayed)
        if (!g_render_buffers[g_back_buffer].ready) {
            render_sphere_3d(g_back_buffer);
        }

        // Small sleep to yield CPU
        tal_system_sleep(1);
    }
}

/**
 * @brief Sphere display task - FAST updates in separate high-priority thread
 * This thread just copies pre-rendered buffers to LEDs for smooth animation
 */
static void sphere_display_task(void *args)
{
    PR_NOTICE("Sphere display task started");

    while (1) {
        uint32_t frame_start = tal_time_get_posix();

        // Fast display - just copy pre-calculated RGB buffer
        display_sphere_fast();

        // Maintain ~50 FPS for smooth animation
        uint32_t frame_time = tal_time_get_posix() - frame_start;
        uint32_t target_frame_time = 20; // ~50 FPS (20ms per frame)

        if (frame_time < target_frame_time) {
            tal_system_sleep(target_frame_time - frame_time);
        } else {
            tal_system_sleep(1); // Minimal sleep if frame took too long
        }
    }
}

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

    // Initialize pixel LED
    rt = pixel_led_init();
    if (OPRT_OK != rt) {
        PR_ERR("Pixel LED initialization failed: %d", rt);
        return;
    }
    PR_NOTICE("Pixel LED initialized");

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

    // Create mutex for buffer swapping
    rt = tal_mutex_create_init(&g_buffer_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create buffer mutex: %d", rt);
        return;
    }
    PR_NOTICE("Buffer mutex created");

    // Create mutex for voice state machine
    rt = tal_mutex_create_init(&g_voice_state_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create voice state mutex: %d", rt);
        return;
    }
    PR_NOTICE("Voice state mutex created");

    // Initialize double buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        memset(g_render_buffers[i].r, 0, sizeof(g_render_buffers[i].r));
        memset(g_render_buffers[i].g, 0, sizeof(g_render_buffers[i].g));
        memset(g_render_buffers[i].b, 0, sizeof(g_render_buffers[i].b));
        g_render_buffers[i].ready = false;
    }
    g_front_buffer = 0;
    g_back_buffer = 1;
    PR_NOTICE("Double buffers initialized");

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

    // Initialize rotation state with slower speeds
    g_rotation_angle_x = 0.0f;
    g_rotation_angle_y = 0.0f;
    g_rotation_angle_z = 0.0f;
    // Balanced rotation speeds - same for all axes to maintain circular appearance
    float base_speed = 0.15f; // Same base speed for all axes
    g_rotation_speed_x = base_speed;
    g_rotation_speed_y = base_speed;
    g_rotation_speed_z = base_speed;
    g_rotation_change_time = 0;

    // Initialize audio-reactive effects
    g_audio_power_smoothed = 0.0f;
    g_sphere_breath = 0.0f;
    g_hue_shift = 0.0f;
    initialize_hot_spots();

    // Initialize voice state machine
    g_voice_state = VOICE_STATE_IDLE;
    g_state_transition_start_time = 0;
    g_idle_circle_radius = 2.5f;
    g_target_animation_radius = SPHERE_RADIUS;
    g_current_radius = 2.5f;
    g_running_ring_angle = 0.0f;
    g_running_ring_angle2 = 0.0f;
    g_start_state_peak_radius = 2.5f; // Initialize peak radius
    PR_NOTICE("Voice state machine initialized to IDLE");

    // Initialize OK button for engine switching (following demo project pattern)
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 2000, // 2 seconds for long press
                                   .long_keep_timer = 500,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    rt = tdl_button_create(BUTTON_NAME, &button_cfg, &g_button_ok_handle);
    if (OPRT_OK == rt) {
        tdl_button_event_register(g_button_ok_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_ok_callback);
    } else {
        PR_ERR("Failed to create OK button '%s': %d (button may not be available)", BUTTON_NAME, rt);
    }

    // Start audio processing task (non-blocking)
    THREAD_CFG_T audio_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_2, .thrdname = "audio_proc"};
    THREAD_HANDLE audio_thrd = NULL;
    rt = tal_thread_create_and_start(&audio_thrd, NULL, NULL, audio_processing_task, NULL, &audio_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start audio processing thread: %d", rt);
        return;
    }
    PR_NOTICE("Audio processing thread started");

    // Start sphere rendering task (SLOW - does 3D calculations)
    THREAD_CFG_T render_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_3, .thrdname = "sphere_render"};
    THREAD_HANDLE render_thrd = NULL;
    rt = tal_thread_create_and_start(&render_thrd, NULL, NULL, sphere_rendering_task, NULL, &render_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start sphere rendering thread: %d", rt);
        return;
    }
    PR_NOTICE("Sphere rendering thread started");

    // Start sphere display task (FAST - high priority for smooth animation)
    THREAD_CFG_T display_thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_1, .thrdname = "sphere_display"};
    THREAD_HANDLE display_thrd = NULL;
    rt = tal_thread_create_and_start(&display_thrd, NULL, NULL, sphere_display_task, NULL, &display_thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start sphere display thread: %d", rt);
        return;
    }
    PR_NOTICE("Sphere display thread started (high priority)");

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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
