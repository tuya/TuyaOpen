/**
 * @file mixer_fixed.c
 * @brief 
 * @version 0.1
 * @date 2025-10-09
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

#include "mixer_fixed.h"

#define CLAMP_S16(x)  ((x) > 32767 ? 32767 : ((x) < -32768 ? -32768 : (x)))

void mix_pcm_s16_mono_16k(int16_t *src, int16_t *dst, int samples)
{
    for (int i = 0; i < samples; i++) {
        int32_t mixed = (int32_t)(src[i]  + (dst[i]));
        dst[i] = (int16_t)CLAMP_S16(mixed);
    }
}

OPERATE_RET ai_player_mixer_process(uint8_t *src_decode_buf, uint8_t *dst_decode_buf, uint32_t samples)
{
    mix_pcm_s16_mono_16k((int16_t *)src_decode_buf, (int16_t *)dst_decode_buf, samples);
    return OPRT_OK;
}

OPERATE_RET ai_player_volume_process(uint8_t *decode_buf, uint32_t len, int volume, int max_volume, uint32_t datebits)
{
    if(datebits != 16) {
        return OPRT_OK; // only support 16bits
    }

    int samples = len / 2; // 16bits = 2 bytes
    int i = 0;
    for(i = 0; i < samples; i++) {
        int32_t sample = ((int16_t *)decode_buf)[i];
        sample = (sample * volume) / max_volume;
        ((int16_t *)decode_buf)[i] = (int16_t)CLAMP_S16(sample);
    }

    return OPRT_OK;
}
