// #ifndef AUDIO_ANALYSIS_H
// #define AUDIO_ANALYSIS_H

// #include "tuya_cloud_types.h"


// int generate_single(char *signal, int sr, int freq, int amp, int t);

// int generate_lfm(char *signal, int sr, int f0, int f1, int amp, int t);

// int generate_rand(char *signal, int amp, int t);

// int generate_rand_wav(char *signal, int amp, int t);

// #endif

/**
 * @file audio_analysis.h
 * @brief audio_analysis module is used to 
 * @version 0.1
 * @date 2025-06-27
 */

#ifndef __AUDIO_ANALYSIS_H__
#define __AUDIO_ANALYSIS_H__

#include "tuya_cloud_types.h"
#include "tuya_app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t AUDIO_ANALYSIS_TYPE_E;
#define AUDIO_ANALYSIS_TYPE_RANG   0x00
#define AUDIO_ANALYSIS_TYPE_SINGLE 0x01
#define AUDIO_ANALYSIS_TYPE_SWEEP  0x02
#define AUDIO_ANALYSIS_TYPE_SWEEPSPECIAL  0x03
#define AUDIO_ANALYSIS_TYPE_MINSIN  0x04
#define AUDIO_ANALYSIS_TYPE_MAX    0x05

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    INT_T amp;                   // 振幅（dB）
    INT_T freq;                  // 频率（Hz）
    INT_T freq_start;            // 起始频率（Hz）
    INT_T freq_end;              // 结束频率（Hz）
    INT_T t;                     // 时长（秒）
    INT_T sr;                    // 采样率（Hz）
} AUDIO_ANALYSIS_PARAMS_T;

// 单频默认配置
#define AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SINGLE(params) \
    do { \
        (params)->amp = 0; /* dB */ \
        (params)->freq = 1000; /* Hz */ \
        (params)->t = 2; /* 秒 */ \
        (params)->sr = 16000; /* 采样率 */ \
    } while(0)

// 随机信号默认配置
#define AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_RANG(params) \
    do { \
        (params)->amp = -3; /* dB */ \
        (params)->t = 2; /* 秒 */ \
        (params)->sr = 16000; /* 采样率 */ \
    } while(0)

// 扫频默认配置
#define AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SWEEP(params) \
    do { \
        (params)->amp = -3; /* dB */ \
        (params)->freq_start = 7500; /* 起始频率 */ \
        (params)->freq_end = 100; /* 结束频率 */ \
        (params)->t = 10; /* 秒 */ \
        (params)->sr = 16000; /* 采样率 */ \
    } while(0)

// 扫频特殊默认配置
#define AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SWEEPSPECIAL(params) \
    do { \
        (params)->amp = -3; /* dB */ \
        (params)->freq_start = 1000; /* 起始频率 */ \
        (params)->freq_end = 5000; /* 结束频率 */ \
        (params)->t = 10; /* 秒 */ \
        (params)->sr = 16000; /* 采样率 */ \
    } while(0)

// 最小单频默认配置
#define AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_MINSIN(params) \
    do { \
        (params)->amp = -90; /* dB */ \
        (params)->freq = 1000; /* Hz */ \
        (params)->t = 2; /* 秒 */ \
        (params)->sr = 16000; /* 采样率 */ \
    } while(0)


/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET audio_analysis_play(AUDIO_ANALYSIS_TYPE_E type, AUDIO_ANALYSIS_PARAMS_T *params);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_ANALYSIS_H__ */
