/**
 * @file board_buzzer_api.h
 * @author Tuya Inc.
 * @brief Header file for PWM buzzer driver API for TUYA_T5AI_PIXEL board.
 *
 * This module provides APIs to control a PWM buzzer, including initialization,
 * tone generation, and volume control. Supports musical note frequencies and
 * custom frequency settings.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_BUZZER_API_H__
#define __BOARD_BUZZER_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief GPIO pin used for buzzer PWM output
 */
#define BOARD_BUZZER_PIN TUYA_GPIO_NUM_33

/**
 * @brief Default buzzer frequency (Hz)
 */
#define BOARD_BUZZER_DEFAULT_FREQ 2000

/**
 * @brief Default buzzer duty cycle (0-100)
 */
#define BOARD_BUZZER_DEFAULT_DUTY 50

/**
 * @brief Musical note frequencies (Hz) - 4th octave
 */
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494

/**
 * @brief Musical note frequencies (Hz) - 5th octave
 */
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988

/**
 * @brief Musical note frequencies (Hz) - 6th octave
 */
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Sequencer entry structure for tone sequences
 */
typedef struct {
    uint32_t frequency;   /**< Frequency in Hz (0 = rest/silence) */
    uint32_t duration_ms; /**< Duration in milliseconds */
    uint8_t duty;         /**< Duty cycle (0-100), 0 = use default */
} BUZZER_SEQ_ENTRY_T;

/**
 * @brief Sequencer structure
 */
typedef struct {
    const BUZZER_SEQ_ENTRY_T *entries; /**< Array of sequence entries */
    uint32_t count;                    /**< Number of entries */
    bool loop;                         /**< Whether to loop the sequence */
} BUZZER_SEQUENCE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the buzzer PWM driver
 *
 * This function initializes the PWM channel for the buzzer and sets it to
 * a default silent state (0% duty cycle).
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_init(void);

/**
 * @brief Deinitialize the buzzer PWM driver
 *
 * This function stops the buzzer and deinitializes the PWM channel.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_deinit(void);

/**
 * @brief Start the buzzer with specified frequency and duty cycle
 *
 * This function starts the buzzer output with the specified frequency and
 * duty cycle (volume).
 *
 * @param[in] frequency Frequency in Hz (typically 200-5000 Hz)
 * @param[in] duty Duty cycle in percentage (0-100, where 50 is typical)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_start(uint32_t frequency, uint8_t duty);

/**
 * @brief Start the buzzer with a musical note
 *
 * This function starts the buzzer output with a musical note frequency.
 * Uses default duty cycle (50%).
 *
 * @param[in] note_frequency Musical note frequency (use NOTE_* macros)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_play_note(uint32_t note_frequency);

/**
 * @brief Stop the buzzer
 *
 * This function stops the buzzer output by setting duty cycle to 0.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_stop(void);

/**
 * @brief Set the buzzer frequency
 *
 * This function changes the frequency of the buzzer while it's running.
 *
 * @param[in] frequency Frequency in Hz (typically 200-5000 Hz)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_set_frequency(uint32_t frequency);

/**
 * @brief Set the buzzer duty cycle (volume)
 *
 * This function changes the duty cycle (volume) of the buzzer while it's running.
 *
 * @param[in] duty Duty cycle in percentage (0-100, where 0 is silent and 100 is maximum)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_set_duty(uint8_t duty);

/**
 * @brief Play a tone for a specified duration
 *
 * This function plays a tone at the specified frequency for the given duration,
 * then automatically stops.
 *
 * @param[in] frequency Frequency in Hz
 * @param[in] duty Duty cycle in percentage (0-100)
 * @param[in] duration_ms Duration in milliseconds
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_play_tone(uint32_t frequency, uint8_t duty, uint32_t duration_ms);

/**
 * @brief Play a musical note for a specified duration
 *
 * This function plays a musical note for the given duration, then automatically stops.
 *
 * @param[in] note_frequency Musical note frequency (use NOTE_* macros)
 * @param[in] duration_ms Duration in milliseconds
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_play_note_duration(uint32_t note_frequency, uint32_t duration_ms);

/**
 * @brief Play a tone sequence asynchronously in a separate thread
 *
 * This function plays a sequence of tones asynchronously without blocking the caller.
 * The sequence is played in a background thread.
 *
 * @param[in] sequence Pointer to the sequence structure
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_play_sequence_async(const BUZZER_SEQUENCE_T *sequence);

/**
 * @brief Stop the currently playing sequence
 *
 * This function stops the sequence playback thread if one is running.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_stop_sequence(void);

/**
 * @brief Check if a sequence is currently playing
 *
 * @return Returns true if a sequence is playing, false otherwise.
 */
bool board_buzzer_is_sequence_playing(void);

/**
 * @brief Play "Twinkle Twinkle Little Star" melody
 *
 * This function plays the classic "Twinkle Twinkle Little Star" melody
 * asynchronously in a background thread.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_buzzer_play_twinkle_twinkle_little_star(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BUZZER_API_H__ */
