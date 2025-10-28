/**
 * @file audio_analysis.c
 * @brief audio_analysis module is used to 
 * @version 0.1
 * @date 2025-06-27
 */

#include "audio_analysis.h"

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "wav_encode.h"

#include "tuya_speaker_service.h"

#include "tal_log.h"
#include "tal_system.h"
#include "tkl_memory.h"
#include "tkl_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AUDIO_ANALYSIS_PLAY_ID        "audio_analysis"

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static const CHAR_T * AUDIO_ANALYSIS_TYPE_STR[] = {
    "SINGLE",
    "RANG",
    "SWEEP",
    "SWEEPSPECIAL",
    "MINSIN"
};

static double pi = 3.14159265358979f;
static double ratio = 0.9438743126816935f;

/***********************************************************
***********************function define**********************
***********************************************************/

// sr 采样率
// freq 频率
// amp 振幅（dB）0, -3, -6 etc.
// t 时长（秒）
int generate_single_pcm(char *signal, int sr, int freq, int amp, int t)
{
    int16_t *s = (int16_t *)signal;
    double a = powf(10, amp/20.0f);
    TAL_PR_DEBUG("amp = %d, %.3f", amp, a);
    
    int samples = sr*t;
    for (int i=0; i<samples; i++) {
        s[i] = (int16_t)(a*sin(2*pi*freq*i*1.0f/sr)*32767);
    }

    return 0;
}

int generate_rand_pcm(char *signal, int amp, int t)
{
    int16_t *s = (int16_t *)signal;
    float a = powf(10, amp/20.0f);
    TAL_PR_DEBUG("amp = %d, %.3f", amp, a);
    int samples = 16000*t;
    for (int i=0; i<samples; i++) {
        float rand_val = ((float)tal_system_get_random(0xFF) / 0xFF) * 2.0 - 1.0;
        s[i] = (int16_t)(a*rand_val*32767);
    }
    return 0;
}

// 硬件要求至少10s
int generate_sweep_pcm(char *signal, int sr, int f0, int f1, int amp, int t)
{
    int16_t *s = (int16_t *)(signal);
    float a = powf(10, amp/20.0f);
    TAL_PR_DEBUG("amp = %d, %.3f", amp, a);

    float k = logf(f0*1.0/f1)/t;  // 对数因子
    double phase = 0.0;

    int samples = sr*t;
    TAL_PR_DEBUG("Generating sweep PCM: sr=%d, f0=%d, f1=%d, t=%d, samples=%d", sr, f0, f1, t, samples);

    /*f(t) = f0 * (f1/f0)**(t/t1)*/
    for (int i=0; i<samples; i++) {
        float t = (float)i / sr;  // 当前时间
        float frequency = f0 * expf(-k * i/sr);  // 对数频率公式
        // 计算相位增量并更新相位
        float phase_increment = 2.0 * pi * frequency / sr;
        phase += phase_increment;
        
        // 生成采样点（限制在16位范围内）
        s[i] = (int16_t)(a * sinf(phase)*32767.0);

        // 每 10s sleep 5 ms
        static SYS_TICK_T last_ms = 0;
        SYS_TICK_T cur_ms = tal_system_get_millisecond();
        if (cur_ms - last_ms > 10*1000 || cur_ms < last_ms) {
            tal_system_sleep(5); // sleep 5ms
            last_ms = cur_ms;
        }

        // 每过 5% 的时间打印一次进度
        if (i % (samples / 20) == 0) {
            TAL_PR_DEBUG("Generating sweep PCM: %.2f%% completed", (i * 100.0) / samples);
        }
    }

    return 0;
}

/*注意时长！*/
int generate_sweep_special_pcm(char *signal, int sr)
{
    short freq_table[22] = {1000, 7500, 5800, 4500, 3500, 2750, 2150, 1700, 1300, 1000,
                                785, 600, 475, 370, 285, 225, 175, 135, 100, 80, 65, 50};
    float tlen[22] = {0.5, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 
                        0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.5, 0.5, 0.5};

    float totaltime = 0.0f;
    for (int i=0; i<21; i++) {
        totaltime += tlen[i];
    }
    TAL_PR_DEBUG("total time = %.3f, \n", totaltime);
    int buffsize = (int)(totaltime*sr);
    
    int16_t *s = (int16_t *)(signal);
    int bias = 0;
    double phi = 0.0;
    for (int i=0; i<21; i++) {          
        short tt = (short)(tlen[i]*sr);       
        for (int j=0; j<tt; j++) {    
            phi = phi + 2*pi*freq_table[i]/sr; 
            bias += 1;    
            *(s + bias) =  (int16_t)(0.5*sinf(phi)*32767.0); // 振幅系数固定0.8(-2dB) 0.5(-6dB)            
        }
    }    
}

int generate_min_signal_pcm(char *signal, int sr, int amp, int t)
{
    int16_t *s = (int16_t *)signal;
    double a = powf(10, amp/20.0f);
    TAL_PR_DEBUG("amp = %d, %.3f\n", amp, a);    
    
    int samples = sr*t;
    for (int i=0; i<samples; i++) {
        #if 0
        if (i%4==0) {
            s[i] = 0;
        } else if (i%4==1) {
            s[i] = -1;
        } else if (i%4==2) {
            s[i] = 0;
        } else if (i%4==3) {
            s[i] = 1;
        }
        #endif
        s[i] = (int16_t)(a*sin(2*pi*1000*i*1.0f/sr)*32767 + ((float)tal_system_get_random(0xFF)/0xFF) * 2.0 - 1.0);        
    }
    return 0;
}



OPERATE_RET audio_analysis_play(AUDIO_ANALYSIS_TYPE_E type, AUDIO_ANALYSIS_PARAMS_T *params)
{
    OPERATE_RET rt = OPRT_OK;
    CHAR_T *signal = NULL;
    INT_T signal_len = 0;

    if (type >= AUDIO_ANALYSIS_TYPE_MAX || type < 0) {
        TAL_PR_ERR("Invalid audio analysis type: %d", type);
        return OPRT_INVALID_PARM;
    }

    if (params == NULL) {
        TAL_PR_ERR("Audio analysis params is NULL");
        return OPRT_INVALID_PARM;
    }

    // 打印参数
    TAL_PR_DEBUG("Audio analysis type: %s, amp: %d dB, freq: %d Hz, freq_start: %d Hz, freq_end: %d Hz, t: %d s, sr: %d Hz",
                 AUDIO_ANALYSIS_TYPE_STR[type],
                 params->amp, params->freq, params->freq_start, params->freq_end, params->t, params->sr);

    switch (type) {
        case AUDIO_ANALYSIS_TYPE_RANG: {
            int amp = params->amp; // 振幅（dB）
            int t = params->t; // 时长（秒）
            int sr = params->sr; // 采样率（Hz）

            signal_len = sr * t * sizeof(int16_t) + 44; // 采样点数 + WAV头
            signal = (CHAR_T *)tkl_system_psram_malloc(signal_len);
            if (signal == NULL) {
                TAL_PR_ERR("malloc audio analysis signal fail");
                return OPRT_MALLOC_FAILED;
            }
            memset(signal, 0, signal_len);
            // 生成随机信号
            generate_rand_pcm(signal + 44, amp, t);
        } break;
        case AUDIO_ANALYSIS_TYPE_SINGLE: {
            // 默认参数
            int amp = params->amp; // 振幅（dB）
            int t = params->t; // 时长（秒）
            int sr = params->sr; // 采样率（Hz）
            int freq = params->freq; // 频率（Hz）

            signal_len = sr * t * sizeof(int16_t) + 44; // 采样点数 + WAV头
            signal = (CHAR_T *)tkl_system_psram_malloc(signal_len);
            if (signal == NULL) {
                TAL_PR_ERR("malloc audio analysis signal fail");
                return OPRT_MALLOC_FAILED;
            }
            memset(signal, 0, signal_len);
            // 生成单频信号
            generate_single_pcm(signal + 44, sr, freq, amp, t);
        } break;
        case AUDIO_ANALYSIS_TYPE_SWEEP: {
            int amp = params->amp; // 振幅（dB）
            int t = params->t; // 时长（秒）
            int sr = params->sr; // 采样率（Hz）
            int f0 = params->freq_start; // 起始频率（Hz）
            int f1 = params->freq_end; // 结束频率（Hz）

            signal_len = sr * t * sizeof(int16_t) + 44; // 采样点数 + WAV头
            signal = (CHAR_T *)tkl_system_psram_malloc(signal_len);
            if (signal == NULL) {
                TAL_PR_ERR("malloc audio analysis signal fail");
                return OPRT_MALLOC_FAILED;
            }
            memset(signal, 0, signal_len);
            // 生成扫频信号
            generate_sweep_pcm(signal + 44, sr, f0, f1, amp, t);
        } break;
        case AUDIO_ANALYSIS_TYPE_SWEEPSPECIAL: {
            int sr = params->sr; // 采样率（Hz）
            int t = params->t; // 时长（秒），至少7.4s

            signal_len = t * sr * sizeof(int16_t) + 44; // 10秒的采样点数 + WAV头
            signal = (CHAR_T *)tkl_system_psram_malloc(signal_len);
            if (signal == NULL) {
                TAL_PR_ERR("malloc audio analysis signal fail");
                return OPRT_MALLOC_FAILED;
            }
            memset(signal, 0, signal_len);
            // 生成特殊扫频信号
            generate_sweep_special_pcm(signal + 44, sr);
        } break;
        case AUDIO_ANALYSIS_TYPE_MINSIN: {
            // 最小单频
            int amp = params->amp; // 振幅（dB）
            int t = params->t; // 时长（秒）
            int sr = params->sr; // 采样率（Hz）
            int freq = params->freq; // 频率（Hz）

            signal_len = sr * t * sizeof(int16_t) + 44; // 采样点数 + WAV头
            signal = (CHAR_T *)tkl_system_psram_malloc(signal_len);
            if (signal == NULL) {
                TAL_PR_ERR("malloc audio analysis signal fail");
                return OPRT_MALLOC_FAILED;
            }
            memset(signal, 0, signal_len);
            // 生成最小单频信号
            generate_min_signal_pcm(signal + 44, sr, amp, t);
        } break;
        default: {
            TAL_PR_ERR("Unsupported audio analysis type: %d", type);
            rt = OPRT_INVALID_PARM;
        } break;
    }

    // 设置WAV头
    if (signal != NULL && signal_len > 44) {
        rt = app_get_wav_head((uint32_t)(signal_len - 44), 1, 16000, 16, 1, signal);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("Failed to get WAV header: %d", rt);
            tkl_system_psram_free(signal);
            signal = NULL;
            return rt;
        }
    } else {
        TAL_PR_ERR("Signal is NULL or length is invalid");
        return OPRT_INVALID_PARM;
    }

    // 播放音频
    TAL_PR_DEBUG("Playing audio analysis signal, type: %s, length: %d", AUDIO_ANALYSIS_TYPE_STR[type], signal_len);

    if (NULL != signal) {
#if 0
        // FIXME: 2025.06.30: tuya_speaker_service_tone_play_data 单次播放音频数据超过 65536 字节会播放不完整
        tuya_speaker_service_tone_play_data(AUDIO_ANALYSIS_PLAY_ID, TUYA_AI_CHAT_AUDIO_FORMAT_WAV, signal, signal_len);
#else
        TKL_AUDIO_FRAME_INFO_T frame_info = {
            .pbuf = signal + 44, // WAV头后面的数据
            .used_size = signal_len - 44, // WAV头不算在内
        };
        tkl_ao_put_frame(TKL_AUDIO_TYPE_BOARD, 0, NULL, &frame_info);
#endif
        tkl_system_psram_free(signal);
        signal = NULL;
    }

    return rt;
}