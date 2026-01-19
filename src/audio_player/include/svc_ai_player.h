/**
 * @file svc_ai_player.h
 * @brief 
 * @version 0.1
 * @date 2025-09-19
 * 
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 * 
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying 
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 */

#ifndef __SVC_AI_PLAYER_H__
#define __SVC_AI_PLAYER_H__

#include "tuya_cloud_types.h"
#include "tkl_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RAM usage */
#ifndef AI_PLAYER_STACK_SIZE
#if defined(AI_PLAYER_DECODER_OPUS_ENABLE) && (AI_PLAYER_DECODER_OPUS_ENABLE==1) || \
    defined(AI_PLAYER_DECODER_OGGOPUS_ENABLE) && (AI_PLAYER_DECODER_OGGOPUS_ENABLE==1)
#define AI_PLAYER_STACK_SIZE (12 * 1024)
#else
#define AI_PLAYER_STACK_SIZE (6 * 1024)
#endif
#endif

#ifndef AI_PLAYER_FRAMEBUF_SIZE
#define AI_PLAYER_FRAMEBUF_SIZE (4 * 1024)
#endif

#ifndef AI_PLAYER_DECODEBUF_SIZE
#define AI_PLAYER_DECODEBUF_SIZE (8 * 1024)
#endif

#ifndef AI_PLAYER_RINGBUF_SIZE
#define AI_PLAYER_RINGBUF_SIZE (16 * 1024)
#endif

#ifndef AI_PLAYER_HTTP_YIELD_MS
#define AI_PLAYER_HTTP_YIELD_MS (10)
#endif

#ifndef AI_PLAYER_HTTP_TIMEOUT_S
#define AI_PLAYER_HTTP_TIMEOUT_S (180)
#endif

/** Feature definition (ROM usage) */
#define AI_PLAYER_DATASINK_URL  (0x1)
#define AI_PLAYER_DATASINK_FILE (0x2)
#define AI_PLAYER_DATASINK_MEM  (0x4)
#ifndef AI_PLAYER_DATASINK
#define AI_PLAYER_DATASINK (AI_PLAYER_DATASINK_MEM | AI_PLAYER_DATASINK_FILE | AI_PLAYER_DATASINK_URL)
#endif

#define AI_PLAYER_DECODER_MP3   (0x1)
#define AI_PLAYER_DECODER_WAV   (0x2)
#define AI_PLAYER_DECODER_SPEEX (0x4)
#define AI_PLAYER_DECODER_OPUS  (0x8)
#define AI_PLAYER_DECODER_OGGOPUS (0x10)
#ifndef AI_PLAYER_DECODER
#if defined(ENABLE_AI_PLAYER_DECODER_OPUS) && (ENABLE_AI_PLAYER_DECODER_OPUS==1)
#define _AI_PLAYER_DECODER_OPUS    AI_PLAYER_DECODER_OPUS
#else
#define _AI_PLAYER_DECODER_OPUS    0
#endif
#if defined(ENABLE_AI_PLAYER_DECODER_OGGOPUS) && (ENABLE_AI_PLAYER_DECODER_OGGOPUS==1)
#define _AI_PLAYER_DECODER_OGGOPUS AI_PLAYER_DECODER_OGGOPUS
#else
#define _AI_PLAYER_DECODER_OGGOPUS 0
#endif
#define AI_PLAYER_DECODER (AI_PLAYER_DECODER_MP3 | AI_PLAYER_DECODER_WAV | _AI_PLAYER_DECODER_OPUS | _AI_PLAYER_DECODER_OGGOPUS)
#endif

#ifndef AI_PLAYER_SUPPORT_MIX_MODE
#define AI_PLAYER_SUPPORT_MIX_MODE (1)
#endif

#ifndef AI_PLAYER_SUPPORT_RESAMPLE
#define AI_PLAYER_SUPPORT_RESAMPLE  (1)
#endif

#ifndef AI_PLAYER_SUPPORT_DIGITAL_VOLUME
#define AI_PLAYER_SUPPORT_DIGITAL_VOLUME  (1)
#endif

#ifndef AI_PLAYER_SUPPORT_DEFAULT_CONSUMER
#define AI_PLAYER_SUPPORT_DEFAULT_CONSUMER  (1)
#endif

typedef void* AI_PLAYER_HANDLE;
typedef void* AI_PLAYLIST_HANDLE;
typedef void* PLAYER_CONSUMER_HANDLE;

/**
 * @brief Player state enumeration
 *
 * Represents the current playback state of the AI player.
 */
typedef enum {
    AI_PLAYER_STOPPED,
    AI_PLAYER_PLAYING,
    AI_PLAYER_PAUSED
}AI_PLAYER_STATE_T;

/**
 * @brief Player state event structure
 *
 * Contains the player handle and its current state when the event occurs.
 */
typedef struct {
    AI_PLAYER_HANDLE handle;
    AI_PLAYER_STATE_T state;
} AI_PLAYER_EVT_T;

/**
 * @brief Player state event definition
 *
 * This event is triggered whenever the player's playback state changes.
 * It provides information about which player instance generated the event
 * and its current state.
 *
 * Event name: "ai.player.state"
 * Event data type: (AI_PLAYER_EVT_T *)
 */
#define EVENT_AI_PLAYER_STATE "ai.player.state" // (AI_PLAYER_EVT_T *)

/**
 * @brief Player working mode enumeration
 *
 * Defines the playback mode of the audio player. The player supports both
 * foreground and background playback, typically used to handle priority
 * between prompt sounds (e.g., voice assistant responses) and continuous
 * background music.
 *
 * By default, the player operates in **preemptive mode**, where foreground
 * playback will interrupt the background audio. When the foreground audio
 * finishes, the background playback will automatically resume.
 *
 * The player also supports a **mixing mode**, which can be enabled via a
 * dedicated API. In mixing mode, both foreground and background audio are
 * played simultaneously. The background volume is automatically reduced
 * (e.g., to 50%) to ensure the foreground audio remains prominent.
 *
 * Modes:
 * - **AI_PLAYER_MODE_FOREGROUND**: Foreground mode, typically used for
 *   high-priority audio such as voice prompts or alerts.
 * - **AI_PLAYER_MODE_BACKGROUND**: Background mode, typically used for
 *   continuous playback such as background music.
 */
typedef enum {
    AI_PLAYER_MODE_FOREGROUND = 0,
    AI_PLAYER_MODE_BACKGROUND,
    AI_PLAYER_MODE_MAX
} AI_PLAYER_MODE_E;

/**
 * @brief Audio player data source type enumeration
 *
 * Defines the possible input sources for the audio player:
 * - AI_PLAYER_SRC_MEM: Play audio data from memory buffer.
 * - AI_PLAYER_SRC_FILE: Play audio data from a local file system.
 * - AI_PLAYER_SRC_URL: Play audio data from a network URL/stream.
 */
typedef enum {
    AI_PLAYER_SRC_MEM = 0,
    AI_PLAYER_SRC_FILE,
    AI_PLAYER_SRC_URL,
    AI_PLAYER_SRC_MAX
} AI_PLAYER_SRC_E;

/**
 * @brief Audio codec type enumeration.
 *
 * This enumeration defines the supported audio codec formats
 * that can be used by the AI audio processing or playback system.
 *
 * Each value represents a specific audio compression or container format:
 * - AI_AUDIO_CODEC_MP3   : MPEG-1 Layer III codec (compressed audio).
 * - AI_AUDIO_CODEC_WAV   : Waveform Audio File Format (uncompressed PCM).
 * - AI_AUDIO_CODEC_SPEEX : Speex codec optimized for speech.
 * - AI_AUDIO_CODEC_OPUS  : Opus codec for high-quality interactive audio.
 * - AI_AUDIO_CODEC_OGGOPUS : Ogg container format with Opus codec.
 * - AI_AUDIO_CODEC_MAX   : Upper boundary marker (not an actual codec).
 */
typedef enum {
    AI_AUDIO_CODEC_MP3 = 0,
    AI_AUDIO_CODEC_WAV,
    AI_AUDIO_CODEC_SPEEX,
    AI_AUDIO_CODEC_OPUS,
    AI_AUDIO_CODEC_OGGOPUS,
    AI_AUDIO_CODEC_MAX
} AI_AUDIO_CODEC_E;

/**
 * @brief Audio output consumer interface definition.
 *
 * This structure defines the set of callback functions that a consumer 
 * (e.g., speaker driver, audio file dumper, or network streamer) must 
 * implement to receive audio data from the AI player.
 *
 * Each function pointer represents a lifecycle or data operation:
 * - open()       : Initialize the consumer instance and allocate resources 
 *                  if needed. Called before any other operation.
 * - close()      : Release all allocated resources and deinitialize the 
 *                  consumer instance.
 * - start()      : Configure and prepare the consumer for audio streaming, 
 *                  e.g., set sample rate, data format, and channel count.
 * - write()      : Push audio PCM frames or encoded data into the consumer 
 *                  sink for playback or processing.
 * - stop()       : Stop the audio stream without destroying the consumer 
 *                  instance. Typically used to pause or end playback.
 * - set_volume() : Adjust the output volume level of the consumer. The 
 *                  framework passes a normalized integer value (e.g., 0–100).
 *
 * @note The player framework calls these functions to manage playback 
 *       output. A consumer must provide valid implementations for all 
 *       function pointers to ensure proper audio output behavior.
 */
typedef struct {
    OPERATE_RET (*open)(PLAYER_CONSUMER_HANDLE *handle);
    OPERATE_RET (*close)(PLAYER_CONSUMER_HANDLE handle);
    OPERATE_RET (*start)(PLAYER_CONSUMER_HANDLE handle);
    OPERATE_RET (*write)(PLAYER_CONSUMER_HANDLE handle, const void *buf, uint32_t len);
    OPERATE_RET (*stop)(PLAYER_CONSUMER_HANDLE handle);
    OPERATE_RET (*set_volume)(PLAYER_CONSUMER_HANDLE handle, uint32_t volume);
} AI_PLAYER_CONSUMER_T;

/**
 * @brief AI player configuration structure.
 *
 * Defines the basic audio parameters used by the AI player.
 * These settings will be applied to consumers when starting playback.
 */
typedef struct {
    TKL_AUDIO_SAMPLE_E sample;      // Audio sample rate
    TKL_AUDIO_DATABITS_E datebits;  // Audio data bits per sample
    TKL_AUDIO_CHANNEL_E channel;    // Audio channel count
} AI_PLAYER_CFG_T;

/**
 * @brief Initialize the AI player service.
 *
 * This function must be called once before creating or using any AI player instance.
 * It sets up global resources such as:
 * - Audio configuration (sample rate, bit depth, channels)
 * - Internal tasks and message queues
 * - Audio driver or output backends
 *
 * @param[in] cfg  Pointer to the player configuration (must not be NULL).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_service_init(AI_PLAYER_CFG_T *cfg);

/**
 * @brief Deinitialize the AI player service.
 *
 * This function releases all resources allocated by
 * tuya_ai_player_service_init(). After calling this function,
 * no player instance can be created or used until
 * tuya_ai_player_service_init() is called again.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_service_deinit(void);

/**
 * @brief Create an audio player instance.
 *
 * @param[in]  mode    Player working mode.
 * @param[out] handle  Pointer to the player handle returned on success.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_create(AI_PLAYER_MODE_E mode, AI_PLAYER_HANDLE *handle);

/**
 * @brief Destroy an audio player instance and release all resources.
 *
 * @param[in] handle Player handle to be destroyed.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_destroy(AI_PLAYER_HANDLE handle);

/**
 * @brief Start playback from a specific data source.
 *
 * @param[in] handle Player handle.
 * @param[in] src    Data source type (memory, file, or URL).
 * @param[in] value  Source value:
 *                   - For memory: reserved or NULL (use feed API instead).
 *                   - For file:   file path string.
 *                   - For URL:    URL string.
 * @param[in] codec  Audio codec type (e.g., MP3, WAV).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_start(AI_PLAYER_HANDLE handle, AI_PLAYER_SRC_E src, char *value, AI_AUDIO_CODEC_E codec);

/**
 * @brief Feed raw audio data to the player (used in memory mode).
 *
 * This function pushes audio data into the player for playback when using 
 * memory-based input. The data buffer is not copied internally, so the caller 
 * should ensure its validity until the function returns.
 *
 * Special case:
 * - If @p data == NULL and @p len == 0, it indicates the end of the stream.
 *
 * @param[in] handle Player handle.
 * @param[in] data   Pointer to audio data buffer.
 * @param[in] len    Length of the data buffer in bytes.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_feed(AI_PLAYER_HANDLE handle, uint8_t *data, uint32_t len);

/**
 * @brief Stop playback immediately.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_stop(AI_PLAYER_HANDLE handle);

/**
 * @brief Pause playback.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_pause(AI_PLAYER_HANDLE handle);

/**
 * @brief Resume playback after being paused.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_resume(AI_PLAYER_HANDLE handle);

/**
 * @brief Get the current playback state of the player
 *
 * @param[in] handle  Handle of the player instance
 *
 * @return Current player state (see @ref AI_PLAYER_STATE_T)
 */
AI_PLAYER_STATE_T tuya_ai_player_get_state(AI_PLAYER_HANDLE handle);

/**
 * @brief Get the current audio codec of the player
 *
 * @param[in] handle  Handle of the player instance
 *
 * @return Current audio codec (see @ref AI_AUDIO_CODEC_E)
 */
AI_AUDIO_CODEC_E tuya_ai_player_get_codec(AI_PLAYER_HANDLE handle);

/**
 * @brief Set the consumer for the audio player (default is tkl_audio).
 *
 * This function binds a user-implemented consumer (AI_PLAYER_CONSUMER_T)
 * to the player service. The player service will invoke the consumer’s
 * callbacks to handle decoded audio output.
 *
 * @param[in] consumer  Pointer to user-implemented consumer interface
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_set_consumer(AI_PLAYER_CONSUMER_T *consumer);

/**
 * @brief Set playback volume.
 *
 * This function adjusts the playback volume of the AI player.
 * - If @p handle is NULL, it adjusts the global analog (hardware) volume.
 * - If @p handle is not NULL, it adjusts the digital volume for the specified player instance.
 *
 * The valid range of @p volume is 0–100, where a higher value indicates a higher volume.
 *
 * @param[in] handle Player handle. NULL for global analog volume control.
 * @param[in] volume Volume level (0–100, higher means louder).
 *
 * @return OPRT_OK on success. Other error codes are defined in tuya_error_code.h.
 */
OPERATE_RET tuya_ai_player_set_volume(AI_PLAYER_HANDLE handle, int volume);

/**
 * @brief Get the current playback volume.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[out] volume Pointer to store the current volume level.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_get_volume(AI_PLAYER_HANDLE handle, int *volume);

/**
 * @brief Enable or disable mute.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[in] mute   TRUE to mute, FALSE to unmute.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_set_mute(AI_PLAYER_HANDLE handle, bool mute);

/**
 * @brief Get the mute status.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[out] mute   Pointer to store mute status (TRUE if muted).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_get_mute(AI_PLAYER_HANDLE handle, bool *mute);

/**
 * @brief Set the player mixing mode
 *
 * Configures whether the player operates in preemptive mode or mixing mode
 * when handling foreground and background playback.
 *
 * In **preemptive mode** (default), foreground playback takes priority:
 * - When foreground audio starts, background playback is automatically paused.
 * - When foreground playback ends, background playback resumes.
 *
 * In **mixing mode**, both foreground and background audio can be played
 * simultaneously:
 * - The background volume is automatically reduced (e.g., to 50%) while
 *   foreground audio is active.
 * - Once foreground playback ends, the background volume returns to normal.
 *
 * @param[in] enable  TRUE to enable mixing mode, FALSE to use preemptive mode.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h.
 *
 * @note This setting affects the behavior of both foreground and background
 *       players globally. It should be configured before playback starts.
 */
OPERATE_RET tuya_ai_player_set_mix_mode(bool enable);

/**
 * @brief Enable or disable the decoder for PCM output.
 *
 * This function controls whether the AI player decodes the input audio stream.
 * When enabled (default), the player decodes the input data into PCM format before output.
 * When disabled, the player bypasses decoding and outputs the raw audio data directly.
 *
 * @param[in] enable  TRUE to enable decoding (output PCM), FALSE to disable decoding (output raw data).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h.
 */
OPERATE_RET tuya_ai_player_set_decoder_mode(bool enable);

/**
 * @brief Playlist configuration parameters
 */
typedef struct {
    bool loop;        /** Whether the playlist should loop when reaching the end */
    bool auto_play;   /** Whether playback should start automatically after adding the first item */
    uint32_t interval;    /** Interval (in ms) to wait before automatically switching to the next item */
    uint32_t capacity;    /** Maximum number of items that the playlist can hold */
} AI_PLAYLIST_CFG_T;

/**
 * @brief Playlist status information
 */
typedef struct {
    uint32_t count;   /** Total number of items in the playlist */
    uint32_t index;   /** Current playing item index (0-based) */
} AI_PLAYLIST_INFO_T;

/**
 * @brief Create a new playlist instance
 *
 * @param[in]  player   Player handle to bind with the playlist
 * @param[in]  cfg      Playlist configuration (loop, auto_play, etc.)
 * @param[out] handle   Returned playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_create(AI_PLAYER_HANDLE player, AI_PLAYLIST_CFG_T *cfg, AI_PLAYLIST_HANDLE *handle);

/**
 * @brief Destroy a playlist instance and release resources
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_destroy(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Add a new item to the playlist
 *
 * @param[in] handle Playlist handle
 * @param[in] src    Source type of the item (e.g. file, URL. NO memory)
 * @param[in] value  Source value (e.g. file path or URL string)
 * @param[in] codec  Audio codec type (e.g., MP3, WAV)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_add(AI_PLAYLIST_HANDLE handle, AI_PLAYER_SRC_E src, char *value, AI_AUDIO_CODEC_E codec);

/**
 * @brief Switch to the previous item in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_prev(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Switch to the next item in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_next(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Stop playback of the current playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_stop(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Restart playback of the playlist from the beginning
 *
 * This function resets the current index to the first item in the playlist
 * and starts playback from that item.
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_restart(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Clear all items in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_clear(AI_PLAYLIST_HANDLE handle);

/**
 * @brief Play the specified playlist item by index.
 *
 * Stops the currently playing item (if any), updates the current index,
 * and starts playback of the target item.
 *
 * Behavior:
 * - If index is out of range (>= current item count) an error is returned.
 *
 * @param[in] handle Playlist handle.
 * @param[in] index  Zero-based item index to play.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_play(AI_PLAYLIST_HANDLE handle, uint32_t index);

/**
 * @brief Configure playlist looping behavior.
 *
 * Sets whether the playlist loops after the last item, and optionally
 * whether it should loop a single track instead of the whole list.
 *
 * Logic:
 * - loop == FALSE: no looping; playback stops at the last item.
 * - loop == TRUE  && single == FALSE: loop all items (restart from first after last).
 * - loop == TRUE  && single == TRUE : repeat the current item indefinitely (single-track loop).
 *
 * If loop is FALSE the single parameter is ignored.
 *
 * @param[in] handle  Playlist handle.
 * @param[in] loop    TRUE enables looping behavior, FALSE disables it.
 * @param[in] single  TRUE enables single-track repeat when loop is TRUE.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_set_loop(AI_PLAYLIST_HANDLE handle, bool loop, bool single);

/**
 * @brief Get playlist information
 *
 * @param[in]  handle Playlist handle
 * @param[out] info   Returned playlist information (count, current index)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_get_info(AI_PLAYLIST_HANDLE handle, AI_PLAYLIST_INFO_T *info);

#ifdef __cplusplus
}
#endif

#endif  // __SVC_AI_PLAYER_H__
