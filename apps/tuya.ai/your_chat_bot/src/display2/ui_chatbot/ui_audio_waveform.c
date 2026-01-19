// Audio Waveform Display Module Implementation

#include "ui_audio_waveform.h"
#include "lvgl/lvgl.h"

// Waveform configuration constants
#define WAVEFORM_BAR_COUNT       5
#define WAVEFORM_BAR_WIDTH       12
#define WAVEFORM_BAR_MIN_HEIGHT  12
#define WAVEFORM_BAR_MAX_HEIGHT  50
#define WAVEFORM_BAR_SPACING     8
#define WAVEFORM_CONTAINER_WIDTH 92 // 5*12 + 4*8 = 92
// #define WAVEFORM_ANIM_DURATION 250  // milliseconds
#define WAVEFORM_ANIM_DURATION 30      // milliseconds
#define WAVEFORM_BAR_COLOR     0x1CB5FC // Cyan blue color

// Private state variables
static lv_obj_t                    *waveform_container                = NULL;
static lv_obj_t                    *waveform_bars[WAVEFORM_BAR_COUNT] = {NULL};
static ui_audio_waveform_power_cb_t power_callback                    = NULL;
static bool                         is_running                        = false;

// Height ratios for symmetric wave pattern (center highest, edges lowest)
static const float height_ratios[WAVEFORM_BAR_COUNT] = {0.3f, 0.7f, 1.0f, 0.7f, 0.3f};

// Forward declaration
static void waveform_update_internal(void);

// Animation callback for waveform bar height changes
static void waveform_bar_anim_cb(void *var, int32_t value)
{
    lv_obj_t *bar = (lv_obj_t *)var;
    if (bar == NULL) {
        return;
    }

    // Update height
    lv_obj_set_height(bar, value);

    // Keep bar aligned to center of container
    int32_t y_pos = (WAVEFORM_BAR_MAX_HEIGHT - value) / 2;
    lv_obj_set_y(bar, y_pos);
}

// Animation ready callback - triggered when animation cycle completes
static void waveform_anim_ready_cb(lv_anim_t *anim)
{
    (void)anim; // Unused parameter

    // If still running, get next power value and start next animation cycle
    if (is_running && power_callback != NULL) {
        waveform_update_internal();
    }
}

void ui_audio_waveform_init(lv_obj_t *parent_container)
{
    if (parent_container == NULL) {
        LV_LOG_WARN("ui_audio_waveform_init: parent_container is NULL");
        return;
    }

    // Destroy existing waveform if any
    ui_audio_waveform_destroy();

    // Create waveform container
    waveform_container = lv_obj_create(parent_container);
    lv_obj_remove_style_all(waveform_container);
    lv_obj_set_width(waveform_container, WAVEFORM_CONTAINER_WIDTH);
    lv_obj_set_height(waveform_container, WAVEFORM_BAR_MAX_HEIGHT);
    lv_obj_set_align(waveform_container, LV_ALIGN_CENTER);
    lv_obj_remove_flag(waveform_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // Create waveform bars
    for (int i = 0; i < WAVEFORM_BAR_COUNT; i++) {
        waveform_bars[i] = lv_obj_create(waveform_container);
        lv_obj_remove_style_all(waveform_bars[i]);

        // Set bar size
        lv_obj_set_width(waveform_bars[i], WAVEFORM_BAR_WIDTH);
        lv_obj_set_height(waveform_bars[i], WAVEFORM_BAR_MIN_HEIGHT);

        // Set bar position (horizontal layout with spacing, center aligned)
        int x_pos = i * (WAVEFORM_BAR_WIDTH + WAVEFORM_BAR_SPACING);
        int y_pos = (WAVEFORM_BAR_MAX_HEIGHT - WAVEFORM_BAR_MIN_HEIGHT) / 2;
        lv_obj_set_pos(waveform_bars[i], x_pos, y_pos);

        // Set bar style
        lv_obj_set_style_bg_color(waveform_bars[i], lv_color_hex(WAVEFORM_BAR_COLOR), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(waveform_bars[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(waveform_bars[i], WAVEFORM_BAR_WIDTH / 2, LV_PART_MAIN);

        lv_obj_remove_flag(waveform_bars[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    LV_LOG_USER("ui_audio_waveform_init: initialized with %d bars", WAVEFORM_BAR_COUNT);
}

void ui_audio_waveform_destroy(void)
{
    // Stop animation first
    ui_audio_waveform_stop();

    // Delete container (this will also delete all child bars)
    if (waveform_container != NULL) {
        lv_obj_del(waveform_container);
        waveform_container = NULL;
    }

    // Clear bar pointers
    for (int i = 0; i < WAVEFORM_BAR_COUNT; i++) {
        waveform_bars[i] = NULL;
    }

    LV_LOG_USER("ui_audio_waveform_destroy: waveform destroyed");
}

// Internal function to update waveform based on callback
static void waveform_update_internal(void)
{
    if (power_callback == NULL) {
        LV_LOG_WARN("ui_audio_waveform: power_callback is NULL");
        return;
    }

    // Get power value from callback
    float power = power_callback();

    // Validate power range
    if (power < 0.0f) {
        power = 0.0f;
    } else if (power > 1.0f) {
        power = 1.0f;
    }

    LV_LOG_USER("ui_audio_waveform_update: power = %.2f", power);

    // Update each bar with animation
    for (int i = 0; i < WAVEFORM_BAR_COUNT; i++) {
        if (waveform_bars[i] == NULL) {
            continue;
        }

        // Calculate target height based on power and height ratio
        // height = MIN_HEIGHT + (ratio * power * (MAX_HEIGHT - MIN_HEIGHT))
        float   height_range  = WAVEFORM_BAR_MAX_HEIGHT - WAVEFORM_BAR_MIN_HEIGHT;
        int32_t target_height = WAVEFORM_BAR_MIN_HEIGHT + (int32_t)(height_ratios[i] * power * height_range);

        // Get current height
        int32_t current_height = lv_obj_get_height(waveform_bars[i]);

        // Create animation
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, waveform_bars[i]);
        lv_anim_set_exec_cb(&anim, waveform_bar_anim_cb);
        lv_anim_set_values(&anim, current_height, target_height);
        lv_anim_set_duration(&anim, WAVEFORM_ANIM_DURATION);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);

        // Set ready callback only for the last bar to trigger next cycle
        if (i == WAVEFORM_BAR_COUNT - 1) {
            lv_anim_set_ready_cb(&anim, waveform_anim_ready_cb);
        }

        lv_anim_start(&anim);
    }
}

void ui_audio_waveform_start(ui_audio_waveform_power_cb_t callback)
{
    if (callback == NULL) {
        LV_LOG_WARN("ui_audio_waveform_start: callback is NULL");
        return;
    }

    if (waveform_container == NULL) {
        LV_LOG_WARN("ui_audio_waveform_start: waveform not initialized");
        return;
    }

    // Stop any existing animation
    ui_audio_waveform_stop();

    // Set callback and start
    power_callback = callback;
    is_running     = true;

    LV_LOG_USER("ui_audio_waveform_start: animation started");

    // Trigger first update
    waveform_update_internal();
}

void ui_audio_waveform_stop(void)
{
    if (!is_running) {
        return;
    }

    is_running     = false;
    power_callback = NULL;

    // Stop all animations
    for (int i = 0; i < WAVEFORM_BAR_COUNT; i++) {
        if (waveform_bars[i] != NULL) {
            lv_anim_delete(waveform_bars[i], waveform_bar_anim_cb);
        }
    }

    LV_LOG_USER("ui_audio_waveform_stop: animation stopped");
}

bool ui_audio_waveform_is_running(void)
{
    return is_running;
}

lv_obj_t *ui_audio_waveform_get_container(void)
{
    return waveform_container;
}
