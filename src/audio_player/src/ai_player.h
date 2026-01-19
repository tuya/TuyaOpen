/**
 * @file ai_player.h
 * @brief 
 * @version 0.1
 * @date 2025-09-24
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

#ifndef __AI_PLAYER_H__
#define __AI_PLAYER_H__

#include "tuya_cloud_types.h"
#include "./decoder/ai_player_decoder.h"
#include "./datasink/ai_player_datasink.h"
#include "./resample/ai_player_resample.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLAYER_IDLE_TIMEOUT_MS  0xFFFFffff
#define PLAYER_BUSY_TIMEOUT_MS  1
#define PLAYER_MAX_VOLUME     100

typedef enum {
    PLAYER_CMD_START = 0,
    PLAYER_CMD_STOP,
    PLAYER_CMD_PAUSE,
    PLAYER_CMD_RESUME,
    PLAYER_CMD_EXIT,
    PLAYER_CMD_SERVICE_DEINIT,
    PLAYER_CMD_MAX
} AI_PLAYER_CMD_E;

typedef struct {
    uint8_t mode;  // AI_PLAYER_MODE_E
    uint8_t cmd;   // AI_PLAYER_CMD_E
    union {
        struct {
            AI_PLAYER_SRC_E src;
            char *value;
            AI_AUDIO_CODEC_E codec;
        } cmd_start;
    } param;
} AI_PLAYER_MSG_T;

typedef struct {
    AI_PLAYER_STATE_T state;
    AI_PLAYER_MODE_E  mode;
    PLAYER_DATASINK sink;
    PLAYER_DECODER decoder;
    uint8_t *framebuf;
    uint32_t offset;
    uint8_t *decode_buf;
    uint32_t decode_size;
    int volume;
    bool mute;
    bool has_pending_output;  // TRUE: decoder has pending data, skip reading new input
    AI_PLAYLIST_HANDLE playlist;
    void (*playlist_cb)(AI_PLAYLIST_HANDLE playlist, AI_PLAYER_STATE_T state);
} AI_PLAYER_T;

extern AI_PLAYER_CONSUMER_T g_consumer_speaker;

#ifdef __cplusplus
}
#endif

#endif /* __AI_PLAYER_H__ */
