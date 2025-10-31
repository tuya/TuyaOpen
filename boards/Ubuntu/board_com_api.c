/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for Ubuntu platform.
 *
 * This file provides the implementation for initializing and registering hardware
 * components on the Ubuntu/Linux platform, with primary focus on ALSA audio support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
#include "tdd_audio_alsa.h"
#endif

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
#include "keyboard_input.h"
#endif

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Registers ALSA audio device for Ubuntu platform
 *
 * This function initializes and registers the ALSA audio driver for audio
 * capture (microphone) and playback (speaker) functionality. It is only
 * available when ENABLE_AUDIO_ALSA is enabled in the configuration.
 *
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure
 */
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
    #if defined(AUDIO_CODEC_NAME)
        PR_INFO("Registering ALSA audio device: %s", AUDIO_CODEC_NAME);

        // Configure ALSA audio parameters
        TDD_AUDIO_ALSA_CFG_T alsa_cfg = {0};

        // Use default ALSA device names (configurable via Kconfig)
        #if defined(CONFIG_ALSA_DEVICE_CAPTURE)
            strncpy(alsa_cfg.capture_device, CONFIG_ALSA_DEVICE_CAPTURE, sizeof(alsa_cfg.capture_device) - 1);
        #else
            strncpy(alsa_cfg.capture_device, "default", sizeof(alsa_cfg.capture_device) - 1);
        #endif

        #if defined(CONFIG_ALSA_DEVICE_PLAYBACK)
            strncpy(alsa_cfg.playback_device, CONFIG_ALSA_DEVICE_PLAYBACK, sizeof(alsa_cfg.playback_device) - 1);
        #else
            strncpy(alsa_cfg.playback_device, "default", sizeof(alsa_cfg.playback_device) - 1);
        #endif

        // Audio format configuration
        alsa_cfg.sample_rate = TDD_ALSA_SAMPLE_16000;  // 16 kHz for voice
        alsa_cfg.data_bits = TDD_ALSA_DATABITS_16;     // 16-bit PCM
        alsa_cfg.channels = TDD_ALSA_CHANNEL_MONO;     // Mono audio

        // Speaker configuration (same as microphone for simplicity)
        alsa_cfg.spk_sample_rate = TDD_ALSA_SAMPLE_16000;

        // ALSA buffer configuration
        #if defined(CONFIG_ALSA_BUFFER_FRAMES)
            alsa_cfg.buffer_frames = CONFIG_ALSA_BUFFER_FRAMES;
        #else
            alsa_cfg.buffer_frames = 1024;  // Default buffer size
        #endif

        #if defined(CONFIG_ALSA_PERIOD_FRAMES)
            alsa_cfg.period_frames = CONFIG_ALSA_PERIOD_FRAMES;
        #else
            alsa_cfg.period_frames = 256;   // Default period size
        #endif

        // AEC configuration (for future use)
        #if defined(ENABLE_AUDIO_AEC) && (ENABLE_AUDIO_AEC == 1)
            alsa_cfg.aec_enable = 1;
        #else
            alsa_cfg.aec_enable = 0;
        #endif

    // Register the ALSA audio driver
    rt = tdd_audio_alsa_register(AUDIO_CODEC_NAME, alsa_cfg);
    if (OPRT_OK != rt) {
        PR_WARN("Failed to register ALSA audio driver: %d", rt);
        PR_WARN("This is expected on Ubuntu systems without audio hardware");
        PR_WARN("Application will continue without audio functionality");
        return rt;
    }

        PR_INFO("ALSA audio device registered successfully");
        PR_INFO("  Capture device: %s", alsa_cfg.capture_device);
        PR_INFO("  Playback device: %s", alsa_cfg.playback_device);
        PR_INFO("  Sample rate: %d Hz", alsa_cfg.sample_rate);
        PR_INFO("  Channels: %d", alsa_cfg.channels);
        PR_INFO("  Bit depth: %d bits", alsa_cfg.data_bits);

    #else
        PR_WARN("AUDIO_CODEC_NAME not defined, skipping audio registration");
    #endif
#else
    PR_WARN("ALSA audio support is not enabled");
#endif

    return rt;
}

/**
 * @brief Keyboard event callback handler
 *
 * Simple forwarding of keyboard events to the application layer.
 * All business logic is handled in app_chat_bot.c
 */
#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)

// Forward declaration - Application layer callback
extern void app_chat_bot_keyboard_event_handler(KEYBOARD_EVENT_E event);

static void __keyboard_event_callback(KEYBOARD_EVENT_E event, void *arg)
{
    // Simple forwarding to application layer
    // All business logic is in app_chat_bot.c
    app_chat_bot_keyboard_event_handler(event);
}
#endif

/**
 * @brief Registers button hardware for Ubuntu platform
 *
 * On Ubuntu, we use keyboard input instead of physical buttons.
 * Press 'S' key to trigger conversation.
 *
 * @return OPERATE_RET - OPRT_OK on success
 */
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
    PR_INFO("Initializing keyboard input handler");
    rt = keyboard_input_init(__keyboard_event_callback, NULL);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to initialize keyboard input: %d", rt);
        return rt;
    }
    PR_INFO("Keyboard ready: [S]=Start [X]=Stop [V]=Vol+ [D]=Vol- [Q]=Quit");
#else
    PR_DEBUG("Keyboard input not enabled");
#endif

    return rt;
}

/**
 * @brief Registers LED hardware for Ubuntu platform
 *
 * Note: LED support on Ubuntu may require platform-specific implementation
 * or GPIO access. This is a placeholder for future implementation.
 *
 * @return OPERATE_RET - OPRT_OK on success
 */
static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME)
    // TODO: Implement LED support for Ubuntu if needed
    // This may involve software indicators or GPIO access
    PR_DEBUG("LED support not yet implemented for Ubuntu platform");
#endif

    return rt;
}

/**
 * @brief Registers all the hardware peripherals on the Ubuntu platform.
 * 
 * This function initializes and registers hardware components including:
 * - ALSA audio device (if ENABLE_AUDIO_ALSA is enabled)
 * - Button (placeholder)
 * - LED (placeholder)
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_INFO("Registering Ubuntu platform hardware...");

    // Register audio device (ALSA)
    rt = __board_register_audio();
    if (OPRT_OK != rt) {
        PR_ERR("Audio registration failed: %d", rt);
        // Continue with other hardware registration
    }

    // Register button (placeholder)
    rt = __board_register_button();
    if (OPRT_OK != rt) {
        PR_WARN("Button registration failed: %d", rt);
    }

    // Register LED (placeholder)
    rt = __board_register_led();
    if (OPRT_OK != rt) {
        PR_WARN("LED registration failed: %d", rt);
    }

    PR_INFO("Ubuntu platform hardware registration completed");

    return OPRT_OK;
}

