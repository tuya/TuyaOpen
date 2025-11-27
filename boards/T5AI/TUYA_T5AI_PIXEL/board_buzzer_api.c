/**
 * @file board_buzzer_api.c
 * @author Tuya Inc.
 * @brief Implementation of PWM buzzer driver API for TUYA_T5AI_PIXEL board.
 *
 * This module provides APIs to control a PWM buzzer, including initialization,
 * tone generation, and volume control. Supports musical note frequencies and
 * custom frequency settings.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pwm.h"
#include "tkl_pinmux.h"
#include "board_buzzer_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Sequencer thread function
 *
 * This function runs in a separate thread and plays the sequence entries
 * one by one with proper timing.
 *
 * @param[in] arg Pointer to BUZZER_SEQUENCE_T structure
 */
static void __buzzer_sequencer_thread(void *arg);

/***********************************************************
***********************variable define**********************
***********************************************************/

static bool g_buzzer_initialized = false;
static TUYA_PWM_NUM_E g_buzzer_pwm_channel = TUYA_PWM_NUM_MAX;

// Sequencer thread management
static THREAD_HANDLE g_sequencer_thread = NULL;
static volatile bool g_sequencer_running = false;
static volatile bool g_sequencer_stop_requested = false;

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET board_buzzer_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (g_buzzer_initialized) {
        PR_DEBUG("Buzzer already initialized");
        return OPRT_OK;
    }

    // Get PWM channel from GPIO pin using pinmux function
    int32_t pwm_id = tkl_io_pin_to_func(BOARD_BUZZER_PIN, TUYA_IO_TYPE_PWM);
    if (pwm_id < 0) {
        PR_ERR("GPIO pin %d does not support PWM function: %d", BOARD_BUZZER_PIN, pwm_id);
        return OPRT_NOT_SUPPORTED;
    }

    // Extract PWM channel from the returned value
    // The return value format: | rsv | port | channel |
    // Note: TUYA_IO_GET_CHANNEL_ID macro is missing closing paren, so we extract manually
    g_buzzer_pwm_channel = (TUYA_PWM_NUM_E)((pwm_id) & 0xFF);

    PR_DEBUG("GPIO pin %d mapped to PWM channel %d (pwm_id: 0x%x)", BOARD_BUZZER_PIN, g_buzzer_pwm_channel, pwm_id);

    // Configure pinmux for PWM function
    rt = tkl_io_pinmux_config(BOARD_BUZZER_PIN, (TUYA_PIN_FUNC_E)pwm_id);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to configure pinmux for buzzer PWM: %d", rt);
        return rt;
    }

    // Initialize PWM with default settings (silent)
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = 0, // Start with 0% duty (silent)
        .frequency = BOARD_BUZZER_DEFAULT_FREQ,
        .polarity = TUYA_PWM_NEGATIVE,
    };

    rt = tkl_pwm_init(g_buzzer_pwm_channel, &pwm_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to initialize buzzer PWM: %d", rt);
        return rt;
    }

    g_buzzer_initialized = true;
    PR_DEBUG("Buzzer initialized on pin %d (PWM channel %d)", BOARD_BUZZER_PIN, g_buzzer_pwm_channel);

    return rt;
}

OPERATE_RET board_buzzer_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        return OPRT_OK;
    }

    // Stop the buzzer
    rt = board_buzzer_stop();
    if (OPRT_OK != rt) {
        PR_ERR("Failed to stop buzzer: %d", rt);
    }

    // Deinitialize PWM
    rt = tkl_pwm_deinit(g_buzzer_pwm_channel);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to deinitialize buzzer PWM: %d", rt);
        return rt;
    }

    g_buzzer_initialized = false;
    g_buzzer_pwm_channel = TUYA_PWM_NUM_MAX;
    PR_DEBUG("Buzzer deinitialized");

    return rt;
}

OPERATE_RET board_buzzer_start(uint32_t frequency, uint8_t duty)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        PR_ERR("Buzzer not initialized");
        return OPRT_INVALID_PARM;
    }

    // Validate frequency range (200-10000 Hz)
    if (frequency < 200 || frequency > 10000) {
        PR_ERR("Invalid frequency: %d Hz (valid range: 200-10000)", frequency);
        return OPRT_INVALID_PARM;
    }

    // Validate duty cycle (0-100)
    if (duty > 100) {
        PR_ERR("Invalid duty cycle: %d%% (valid range: 0-100)", duty);
        return OPRT_INVALID_PARM;
    }

    // Set frequency
    rt = tkl_pwm_frequency_set(g_buzzer_pwm_channel, frequency);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set buzzer frequency: %d", rt);
        return rt;
    }

    // Set duty cycle
    rt = tkl_pwm_duty_set(g_buzzer_pwm_channel, duty);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set buzzer duty: %d", rt);
        return rt;
    }

    // Start PWM
    rt = tkl_pwm_start(g_buzzer_pwm_channel);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start buzzer: %d", rt);
        return rt;
    }

    PR_DEBUG("Buzzer started: %d Hz, %d%% duty", frequency, duty);

    return rt;
}

OPERATE_RET board_buzzer_play_note(uint32_t note_frequency)
{
    return board_buzzer_start(note_frequency, BOARD_BUZZER_DEFAULT_DUTY);
}

OPERATE_RET board_buzzer_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        return OPRT_OK;
    }

    // Set duty cycle to 0 (silent)
    rt = tkl_pwm_duty_set(g_buzzer_pwm_channel, 0);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set buzzer duty to 0: %d", rt);
        return rt;
    }

    // Stop PWM
    rt = tkl_pwm_stop(g_buzzer_pwm_channel);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to stop buzzer: %d", rt);
        return rt;
    }

    PR_DEBUG("Buzzer stopped");

    return rt;
}

OPERATE_RET board_buzzer_set_frequency(uint32_t frequency)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        PR_ERR("Buzzer not initialized");
        return OPRT_INVALID_PARM;
    }

    // Validate frequency range
    if (frequency < 200 || frequency > 10000) {
        PR_ERR("Invalid frequency: %d Hz (valid range: 200-10000)", frequency);
        return OPRT_INVALID_PARM;
    }

    rt = tkl_pwm_frequency_set(g_buzzer_pwm_channel, frequency);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set buzzer frequency: %d", rt);
        return rt;
    }

    PR_DEBUG("Buzzer frequency set to: %d Hz", frequency);

    return rt;
}

OPERATE_RET board_buzzer_set_duty(uint8_t duty)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        PR_ERR("Buzzer not initialized");
        return OPRT_INVALID_PARM;
    }

    // Validate duty cycle
    if (duty > 100) {
        PR_ERR("Invalid duty cycle: %d%% (valid range: 0-100)", duty);
        return OPRT_INVALID_PARM;
    }

    rt = tkl_pwm_duty_set(g_buzzer_pwm_channel, duty);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set buzzer duty: %d", rt);
        return rt;
    }

    PR_DEBUG("Buzzer duty cycle set to: %d%%", duty);

    return rt;
}

OPERATE_RET board_buzzer_play_tone(uint32_t frequency, uint8_t duty, uint32_t duration_ms)
{
    OPERATE_RET rt = OPRT_OK;

    // Start the buzzer
    rt = board_buzzer_start(frequency, duty);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Wait for the specified duration
    tal_system_sleep(duration_ms);

    // Stop the buzzer
    rt = board_buzzer_stop();
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

OPERATE_RET board_buzzer_play_note_duration(uint32_t note_frequency, uint32_t duration_ms)
{
    return board_buzzer_play_tone(note_frequency, BOARD_BUZZER_DEFAULT_DUTY, duration_ms);
}

/**
 * @brief Sequencer thread function
 */
static void __buzzer_sequencer_thread(void *arg)
{
    const BUZZER_SEQUENCE_T *sequence = (const BUZZER_SEQUENCE_T *)arg;

    PR_NOTICE("Sequencer thread function called");

    if (!sequence || !sequence->entries || sequence->count == 0) {
        PR_ERR("Invalid sequence in sequencer thread: sequence=%p, entries=%p, count=%d", sequence,
               sequence ? sequence->entries : NULL, sequence ? sequence->count : 0);
        g_sequencer_running = false;
        g_sequencer_thread = NULL;
        return;
    }

    PR_NOTICE("Sequencer thread started, playing %d entries", sequence->count);

    do {
        for (uint32_t i = 0; i < sequence->count; i++) {
            // Check if stop was requested
            if (g_sequencer_stop_requested) {
                PR_DEBUG("Sequencer stop requested");
                break;
            }

            const BUZZER_SEQ_ENTRY_T *entry = &sequence->entries[i];

            // Determine duty cycle (use default if 0)
            uint8_t duty = (entry->duty == 0) ? BOARD_BUZZER_DEFAULT_DUTY : entry->duty;

            if (entry->frequency == 0) {
                // Rest/silence - just wait
                board_buzzer_stop();
                tal_system_sleep(entry->duration_ms);
            } else {
                // Ensure buzzer is stopped before starting new tone (prevents sticking)
                board_buzzer_stop();
                tal_system_sleep(10); // Small gap between tones (10ms)

                // Play the tone
                PR_DEBUG("Playing entry %d: freq=%d Hz, duration=%d ms, duty=%d%%", i, entry->frequency,
                         entry->duration_ms, duty);
                OPERATE_RET rt = board_buzzer_start(entry->frequency, duty);
                if (OPRT_OK == rt) {
                    tal_system_sleep(entry->duration_ms);
                    board_buzzer_stop();
                } else {
                    PR_ERR("Failed to play tone at entry %d (freq=%d, duty=%d): %d", i, entry->frequency, duty, rt);
                }
            }
        }
    } while (sequence->loop && !g_sequencer_stop_requested);

    // Stop the buzzer when done
    board_buzzer_stop();

    PR_DEBUG("Sequencer thread finished");
    g_sequencer_running = false;
    g_sequencer_thread = NULL;
}

OPERATE_RET board_buzzer_play_sequence_async(const BUZZER_SEQUENCE_T *sequence)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_buzzer_initialized) {
        PR_ERR("Buzzer not initialized");
        return OPRT_INVALID_PARM;
    }

    if (!sequence || !sequence->entries || sequence->count == 0) {
        PR_ERR("Invalid sequence");
        return OPRT_INVALID_PARM;
    }

    // Stop any currently playing sequence
    if (g_sequencer_running) {
        PR_DEBUG("Stopping current sequence before starting new one");
        board_buzzer_stop_sequence();
        // Wait a bit for the thread to stop
        tal_system_sleep(100);
    }

    // Reset stop flag
    g_sequencer_stop_requested = false;
    g_sequencer_running = true;

    // Configure thread parameters
    THREAD_CFG_T thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_2, .thrdname = "buzzer_seq"};

    PR_NOTICE("Creating sequencer thread: sequence=%p, entries=%p, count=%d", sequence, sequence->entries,
              sequence->count);

    // Create and start the sequencer thread
    rt = tal_thread_create_and_start(&g_sequencer_thread, NULL, NULL, __buzzer_sequencer_thread, (void *)sequence,
                                     &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create sequencer thread: %d", rt);
        g_sequencer_running = false;
        return rt;
    }

    PR_NOTICE("Sequencer thread created and started successfully, handle=%p", g_sequencer_thread);

    // Give the thread a moment to start
    tal_system_sleep(10);

    return rt;
}

OPERATE_RET board_buzzer_stop_sequence(void)
{
    if (!g_sequencer_running) {
        return OPRT_OK;
    }

    PR_DEBUG("Requesting sequencer stop");
    g_sequencer_stop_requested = true;

    // Wait for thread to finish (with timeout)
    uint32_t timeout_ms = 5000; // 5 second timeout
    uint32_t waited_ms = 0;
    while (g_sequencer_running && waited_ms < timeout_ms) {
        tal_system_sleep(50);
        waited_ms += 50;
    }

    if (g_sequencer_running) {
        PR_ERR("Sequencer thread did not stop within timeout");
        // Thread will clean itself up eventually
    } else {
        PR_DEBUG("Sequencer thread stopped");
    }

    // Stop the buzzer
    board_buzzer_stop();

    return OPRT_OK;
}

bool board_buzzer_is_sequence_playing(void)
{
    return g_sequencer_running;
}

/**
 * @brief "Twinkle Twinkle Little Star" melody sequence
 *
 * Notes: C C G G A A G - F F E E D D C
 * Tempo: Quarter note = 300ms (faster tempo)
 */
static const BUZZER_SEQ_ENTRY_T twinkle_twinkle_sequence[] = {
    // Twinkle, twinkle, little star
    {NOTE_C4, 300, 0}, // C
    {NOTE_C4, 300, 0}, // C
    {NOTE_G4, 300, 0}, // G
    {NOTE_G4, 300, 0}, // G
    {NOTE_A4, 300, 0}, // A
    {NOTE_A4, 300, 0}, // A
    {NOTE_G4, 600, 0}, // G (half note)

    // How I wonder what you are
    {NOTE_F4, 300, 0}, // F
    {NOTE_F4, 300, 0}, // F
    {NOTE_E4, 300, 0}, // E
    {NOTE_E4, 300, 0}, // E
    {NOTE_D4, 300, 0}, // D
    {NOTE_D4, 300, 0}, // D
    {NOTE_C4, 600, 0}, // C (half note)

    // Up above the world so high
    {NOTE_G4, 300, 0}, // G
    {NOTE_G4, 300, 0}, // G
    {NOTE_F4, 300, 0}, // F
    {NOTE_F4, 300, 0}, // F
    {NOTE_E4, 300, 0}, // E
    {NOTE_E4, 300, 0}, // E
    {NOTE_D4, 600, 0}, // D (half note)

    // Like a diamond in the sky
    {NOTE_G4, 300, 0}, // G
    {NOTE_G4, 300, 0}, // G
    {NOTE_F4, 300, 0}, // F
    {NOTE_F4, 300, 0}, // F
    {NOTE_E4, 300, 0}, // E
    {NOTE_E4, 300, 0}, // E
    {NOTE_D4, 600, 0}, // D (half note)

    // Twinkle, twinkle, little star
    {NOTE_C4, 300, 0}, // C
    {NOTE_C4, 300, 0}, // C
    {NOTE_G4, 300, 0}, // G
    {NOTE_G4, 300, 0}, // G
    {NOTE_A4, 300, 0}, // A
    {NOTE_A4, 300, 0}, // A
    {NOTE_G4, 600, 0}, // G (half note)

    // How I wonder what you are
    {NOTE_F4, 300, 0},  // F
    {NOTE_F4, 300, 0},  // F
    {NOTE_E4, 300, 0},  // E
    {NOTE_E4, 300, 0},  // E
    {NOTE_D4, 300, 0},  // D
    {NOTE_D4, 300, 0},  // D
    {NOTE_C4, 1200, 0}, // C (whole note - final)
};

OPERATE_RET board_buzzer_play_twinkle_twinkle_little_star(void)
{
    // Use static to ensure the sequence structure persists for the thread
    static BUZZER_SEQUENCE_T sequence = {.entries = twinkle_twinkle_sequence,
                                         .count =
                                             sizeof(twinkle_twinkle_sequence) / sizeof(twinkle_twinkle_sequence[0]),
                                         .loop = false};

    PR_NOTICE("Playing Twinkle Twinkle Little Star: %d entries", sequence.count);
    return board_buzzer_play_sequence_async(&sequence);
}
