/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for TaishanPi_3 board.
 *
 * This file provides the implementation for initializing and registering hardware
 * components on the Linux platform, with primary focus on ALSA audio support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
#include "tdd_audio_alsa.h"
#include <alsa/asoundlib.h>
#endif

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
#include "tdd_button_keyboard.h"
#endif

#if defined(ENABLE_CAMERA_V4L2) && (ENABLE_CAMERA_V4L2 == 1)
#include "tdd_camera_v4l2.h"
#endif

#if defined(ENABLE_SDL_DISPLAY) && (ENABLE_SDL_DISPLAY == 1)
#include "tdd_disp_sdl.h"
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
 * @brief Applies ES8388 codec tuning for TaishanPi_3 board mic input.
 *
 * Sets Left/Right Capture Volume to +24 dB and configures ADC Data Select
 * to Left-Left to prevent phase cancellation when down-mixing to mono.
 * Equivalent to:
 *   amixer -c 0 cset numid=52 8   # Left  Capture Volume +24 dB
 *   amixer -c 0 cset numid=53 8   # Right Capture Volume +24 dB
 *   amixer -c 0 cset numid=61 1   # ADC Data Select: Left-Left
 */
#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
static void __es8388_ctl_set_integer(snd_ctl_t *ctl, unsigned int numid, long value)
{
    snd_ctl_elem_id_t    *id  = NULL;
    snd_ctl_elem_value_t *val = NULL;

    snd_ctl_elem_id_malloc(&id);
    snd_ctl_elem_value_malloc(&val);

    snd_ctl_elem_id_set_numid(id, numid);
    snd_ctl_elem_value_set_id(val, id);
    snd_ctl_elem_value_set_integer(val, 0, value);
    if (snd_ctl_elem_write(ctl, val) < 0) {
        PR_WARN("ES8388: failed to set numid=%u to %ld", numid, value);
    }

    snd_ctl_elem_value_free(val);
    snd_ctl_elem_id_free(id);
}

static void __es8388_ctl_set_enum(snd_ctl_t *ctl, unsigned int numid, unsigned int item)
{
    snd_ctl_elem_id_t    *id  = NULL;
    snd_ctl_elem_value_t *val = NULL;

    snd_ctl_elem_id_malloc(&id);
    snd_ctl_elem_value_malloc(&val);

    snd_ctl_elem_id_set_numid(id, numid);
    snd_ctl_elem_value_set_id(val, id);
    snd_ctl_elem_value_set_enumerated(val, 0, item);
    if (snd_ctl_elem_write(ctl, val) < 0) {
        PR_WARN("ES8388: failed to set numid=%u to item %u", numid, item);
    }

    snd_ctl_elem_value_free(val);
    snd_ctl_elem_id_free(id);
}

static void __board_configure_es8388(void)
{
    snd_ctl_t *ctl = NULL;
    if (snd_ctl_open(&ctl, "hw:0", 0) < 0) {
        PR_WARN("ES8388: cannot open ALSA control hw:0, skipping codec tuning");
        return;
    }

    __es8388_ctl_set_integer(ctl, 52, 8); /* Left  Capture Volume: +24 dB */
    __es8388_ctl_set_integer(ctl, 53, 8); /* Right Capture Volume: +24 dB */
    __es8388_ctl_set_enum   (ctl, 61, 1); /* ADC Data Select: Left-Left   */

    snd_ctl_close(ctl);
    PR_INFO("ES8388 codec tuned: mic gain +24 dB, ADC Left-Left");
}

/* Stop PipeWire so direct ALSA hw access is not blocked by the sound server. */
static void __board_stop_pipewire(void)
{
    PR_INFO("Stopping PipeWire...");
    system("systemctl --user stop pipewire.socket pipewire-pulse.socket "
           "wireplumber pipewire pipewire-pulse >/dev/null 2>&1");
}
#endif

/**
 * @brief Registers ALSA audio device for TaishanPi_3 board
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

        // Use plughw:0,0 — ALSA plug layer converts 16kHz mono to hardware 48kHz stereo.
        // No /etc/asound.conf needed.
        #if defined(ALSA_DEVICE_CAPTURE)
            strncpy(alsa_cfg.capture_device, ALSA_DEVICE_CAPTURE, sizeof(alsa_cfg.capture_device) - 1);
        #else
            strncpy(alsa_cfg.capture_device, "plughw:0,0", sizeof(alsa_cfg.capture_device) - 1);
        #endif

        #if defined(ALSA_DEVICE_PLAYBACK)
            strncpy(alsa_cfg.playback_device, ALSA_DEVICE_PLAYBACK, sizeof(alsa_cfg.playback_device) - 1);
        #else
            strncpy(alsa_cfg.playback_device, "plughw:0,0", sizeof(alsa_cfg.playback_device) - 1);
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

    // Stop PipeWire and configure ES8388 codec before registering the ALSA driver
    __board_stop_pipewire();
    __board_configure_es8388();

    // Register the ALSA audio driver
    rt = tdd_audio_alsa_register(AUDIO_CODEC_NAME, alsa_cfg);
    if (OPRT_OK != rt) {
        PR_WARN("Failed to register ALSA audio driver: %d", rt);
        PR_WARN("This is expected on TaishanPi_3 systems without audio hardware");
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
 * @brief Registers button hardware for TaishanPi_3 board
 *
 * On TaishanPi_3, we use keyboard input instead of physical buttons.
 * Press 'S' key to trigger conversation.
 *
 * @return OPERATE_RET - OPRT_OK on success
 */
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
    PR_INFO("Initializing keyboard input handler");
    #if defined(BUTTON_NAME)
    BUTTON_CFG_T btn_cfg = {0};
    btn_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    btn_cfg.key_char = BUTTON_NAME[0];
    rt = tdd_keyboard_button_register("ai_chat_button", &btn_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to register keyboard button handler: %d", rt);
        return rt;
    }
    #endif // BUTTON_NAME
#else
    PR_DEBUG("Keyboard input not enabled");
#endif

    return rt;
}

/**
 * @brief Registers LED hardware for TaishanPi_3 board
 *
 * Note: LED support on TaishanPi_3 may require platform-specific implementation
 * or GPIO access. This is a placeholder for future implementation.
 *
 * @return OPERATE_RET - OPRT_OK on success
 */
static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

/**
 * @brief Registers V4L2 USB camera for TaishanPi_3 board
 */
static OPERATE_RET __board_register_camera(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_CAMERA_V4L2) && (ENABLE_CAMERA_V4L2 == 1)
    #if defined(CAMERA_NAME)
        #if defined(CAMERA_V4L2_DEVNODE)
            PR_INFO("Registering V4L2 camera: %s (%s)", CAMERA_NAME, CAMERA_V4L2_DEVNODE);
            rt = tdd_camera_v4l2_register(CAMERA_NAME, CAMERA_V4L2_DEVNODE);
        #else
            PR_INFO("Registering V4L2 camera: %s (/dev/video0)", CAMERA_NAME);
            rt = tdd_camera_v4l2_register(CAMERA_NAME, "/dev/video0");
        #endif
        if (OPRT_OK != rt) {
            PR_WARN("Failed to register V4L2 camera: %d", rt);
            return rt;
        }
    #else
        PR_WARN("CAMERA_NAME not defined, skipping camera registration");
    #endif
#endif

    return rt;
}

/**
 * @brief Registers SDL2 window display for TaishanPi_3 board.
 */
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SDL_DISPLAY) && (ENABLE_SDL_DISPLAY == 1)
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#if defined(DISPLAY_NAME)
    uint16_t width = 480;
    uint16_t height = 480;

#if defined(CONFIG_SDL_DISPLAY_WIDTH)
    if (CONFIG_SDL_DISPLAY_WIDTH > 0) {
        width = (uint16_t)CONFIG_SDL_DISPLAY_WIDTH;
    }
#endif
#if defined(SDL_DISPLAY_WIDTH)
    if (SDL_DISPLAY_WIDTH > 0) {
        width = (uint16_t)SDL_DISPLAY_WIDTH;
    }
#endif
#if defined(CONFIG_SDL_DISPLAY_HEIGHT)
    if (CONFIG_SDL_DISPLAY_HEIGHT > 0) {
        height = (uint16_t)CONFIG_SDL_DISPLAY_HEIGHT;
    }
#endif
#if defined(SDL_DISPLAY_HEIGHT)
    if (SDL_DISPLAY_HEIGHT > 0) {
        height = (uint16_t)SDL_DISPLAY_HEIGHT;
    }
#endif

    PR_INFO("Registering SDL display device '%s' (%ux%u)", DISPLAY_NAME, width, height);
    rt = tdd_disp_sdl_register(DISPLAY_NAME, width, height);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to register SDL display: %d", rt);
        return rt;
    }

#if defined(ENABLE_SDL_TP) && (ENABLE_SDL_TP == 1)
    PR_INFO("Registering SDL touchpad device '%s' (%ux%u)", DISPLAY_NAME, width, height);
    rt = tdd_tp_sdl_register(DISPLAY_NAME, width, height);
    if (rt != OPRT_OK) {
        PR_WARN("Failed to register SDL touchpad: %d", rt);
        /* Touch is optional on desktop; don't fail the whole board init. */
        rt = OPRT_OK;
    }
#endif
#else
    PR_WARN("DISPLAY_NAME is not defined, skipping SDL display registration");
#endif
#endif
#endif

    return rt;
}

/**
 * @brief Registers all the hardware peripherals on the TaishanPi_3 board.
 * 
 * This function initializes and registers hardware components including:
 * - ALSA audio device (if ENABLE_AUDIO_ALSA is enabled)
 * - Button
 * - LED
 * - Camera (if ENABLE_CAMERA_V4L2 is enabled)
 * - Display (if ENABLE_SDL_DISPLAY is enabled)
 * - Touchpad (if ENABLE_SDL_TP is enabled)
 * 
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_INFO("Registering TaishanPi_3 board hardware...");

    // Register audio device (ALSA)
    rt = __board_register_audio();
    if (OPRT_OK != rt) {
        PR_ERR("Audio registration failed: %d", rt);
        // Continue with other hardware registration
    }

    // Register button
    rt = __board_register_button();
    if (OPRT_OK != rt) {
        PR_WARN("Button registration failed: %d", rt);
    }

    // Register LED
    rt = __board_register_led();
    if (OPRT_OK != rt) {
        PR_WARN("LED registration failed: %d", rt);
    }

    // Register camera (V4L2)
    rt = __board_register_camera();
    if (OPRT_OK != rt) {
        PR_WARN("Camera registration failed: %d", rt);
    }

    // Register display (SDL2 window)
    rt = __board_register_display();
    if (OPRT_OK != rt) {
        PR_WARN("Display registration failed: %d", rt);
    }

    PR_INFO("TaishanPi_3 board hardware registration completed");

    return OPRT_OK;
}

