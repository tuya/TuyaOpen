/**
* Copyright (C) by Tuya Inc                                                  
* All rights reserved                                                        
*
* @file audio_recorder.h
* @brief audio input and output processing
* @version 1.0
* @author linch
* @date 2025-05-13
*
*/

#ifndef __TUYA_AUDIO_RECORDER_H__
#define __TUYA_AUDIO_RECORDER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tkl_audio.h"

#define AUDIO_RECORDER_TOTAL_TIME           5000        // 默认的总录音时长 5S
#define AUDIO_RECORDER_SLICE_TIME           80          // 每次output输出是音频数据 80ms

typedef enum {
    AUDIO_RECODER_VAD_START,
    AUDIO_RECODER_VAD_SPEAK,
    AUDIO_RECODER_VAD_END,
    AUDIO_RECODER_MODE_UPDATE,
    AUDIO_RECODER_WAKEUP,
    AUDIO_RECODER_START,
    AUDIO_RECODER_STOP,
} audio_recorder_stat_t;


typedef enum {
    //! 长按说话，按键打断
    AUDIO_RECODER_MODE_KEY_HOLD = 0,
    //！按键说话，按键打断，30S无对话，需要再次按键
    AUDIO_RECODER_MODE_KEY_ONCE,
    //！按键/唤醒说话，唤醒/按键打断，对话完后需要再次唤醒
    AUDIO_RECODER_MODE_WAKEUP,
    //！按键/唤醒说话，唤醒/按键/说话打断，30S无对话，需要再次按键/唤醒
    AUDIO_RECODER_MODE_FREE,
    //! 未初始化获取返回
    AUDIO_RECODER_MODE_MAX, 
} audio_recorder_mode_t;

typedef struct {
    audio_recorder_stat_t   state;
    audio_recorder_mode_t   mode;
    uint8_t                *data;
    uint16_t                datalen;
} audio_recorder_msg_t;

typedef void (*recorder_output_cb)(audio_recorder_msg_t *msg, void *user_data);

typedef struct {
    TKL_AUDIO_SAMPLE_E          sample_rate;       // audio sample rate
    TKL_AUDIO_DATABITS_E        sample_bits;       // audio sample bits
    TKL_AUDIO_CHANNEL_E         channel;           // audio channel
    TUYA_GPIO_NUM_E             spk_io;            // speaker enable pin
    TUYA_GPIO_LEVEL_E           spk_io_level;      // pin polarity, 0 high enable, 1 low enable

    audio_recorder_mode_t       mode;

    //! vad cache = vad_active_ms + vad_off_ms
    uint16_t                    vad_off_ms;        // 语音活动补偿时间，单位ms
    uint16_t                    vad_active_ms;     // 语音活动检测阈值，单位ms
    uint16_t                    vad_silence_ms;    // 语音静音触发阈值，单位ms

    uint16_t                    total_ms;          // ref macro, AUDIO_RECORDER_TOTAL_TIME
    uint16_t                    slice_ms;          // ref macro, AUDIO_RECORDER_SLICE_TIME

    recorder_output_cb           output_cb;
    void                        *user_data;
} audio_recorder_cfg_t;


#define AUDIO_RECORDER_CFG_INIT(__cfg, __user_cb, __user_data, __spk_mode, __spk_io, __spk_io_level) {  \
    (__cfg)->sample_rate    = TKL_AUDIO_SAMPLE_16K;                                         \
    (__cfg)->sample_bits    = TKL_AUDIO_DATABITS_16;                                        \
    (__cfg)->channel        = TKL_AUDIO_CHANNEL_MONO;                                       \
    (__cfg)->mode           = __spk_mode;                                                   \
    (__cfg)->total_ms       = AUDIO_RECORDER_TOTAL_TIME;                                    \
    (__cfg)->slice_ms       = AUDIO_RECORDER_SLICE_TIME;                                    \
    (__cfg)->vad_off_ms     = 300;                                                          \
    (__cfg)->vad_active_ms  = 300;                                                          \
    (__cfg)->vad_silence_ms = 500;                                                          \
    (__cfg)->spk_io         = __spk_io;                                                     \
    (__cfg)->spk_io_level   = __spk_io_level;                                               \
    (__cfg)->output_cb      = __user_cb;                                                    \
    (__cfg)->user_data      = __user_data;                                                  \
}

int audio_recorder_init(audio_recorder_cfg_t *cfg);
int audio_recorder_start(void);
int audio_recorder_stop(void);
int audio_recorder_mode_set(audio_recorder_mode_t mode);
int audio_recorder_key_vad_set(int vad_flag);
audio_recorder_mode_t audio_recorder_mode_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AUDIO_RECORDER_H__ */
