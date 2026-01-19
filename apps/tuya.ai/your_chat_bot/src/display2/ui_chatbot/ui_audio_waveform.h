// Audio Waveform Display Module
// Provides visual feedback for audio input power levels

#ifndef UI_AUDIO_WAVEFORM_H
#define UI_AUDIO_WAVEFORM_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for getting audio power level
 * @return Audio power level (0.0 = minimum, 1.0 = maximum)
 */
typedef float (*ui_audio_waveform_power_cb_t)(void);

/**
 * @brief Initialize the audio waveform display
 * @param parent_container The parent container object to attach the waveform to
 * @note This function creates 5 vertical bars that animate based on audio power
 */
void ui_audio_waveform_init(lv_obj_t *parent_container);

/**
 * @brief Destroy the audio waveform display and free resources
 * @note This will automatically stop any running animation
 */
void ui_audio_waveform_destroy(void);

/**
 * @brief Start the waveform animation with automatic updates
 * @param power_callback Callback function to get the current power level
 * @note The callback will be invoked after each animation cycle completes
 *       to get the next power value, creating a continuous animation loop
 */
void ui_audio_waveform_start(ui_audio_waveform_power_cb_t power_callback);

/**
 * @brief Stop the waveform animation
 * @note The waveform bars will remain at their current state
 */
void ui_audio_waveform_stop(void);

/**
 * @brief Check if the waveform animation is currently running
 * @return true if animation is running, false otherwise
 */
bool ui_audio_waveform_is_running(void);

/**
 * @brief Get the waveform container object
 * @return Pointer to the waveform container object, or NULL if not initialized
 */
lv_obj_t *ui_audio_waveform_get_container(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // UI_AUDIO_WAVEFORM_H
