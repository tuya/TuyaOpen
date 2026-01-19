/**
 * @file ai_player_resample.c
 * @brief 
 * @version 0.1
 * @date 2025-09-25
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

#include "tal_log.h"
#include "tal_memory.h"
#include "ai_player_resample.h"
#include "resample_fixed.h"

typedef struct {
    TKL_AUDIO_SAMPLE_E sample;
    TKL_AUDIO_DATABITS_E datebits;
    TKL_AUDIO_CHANNEL_E channel;
} RESAMPLE_CTX_T;

static RESAMPLE_CTX_T s_resample_ctx = {0};

OPERATE_RET ai_player_resample_init(TKL_AUDIO_SAMPLE_E sample, TKL_AUDIO_DATABITS_E datebits, TKL_AUDIO_CHANNEL_E channel)
{
    s_resample_ctx.sample = sample;
    s_resample_ctx.datebits = datebits;
    s_resample_ctx.channel = channel;

    return OPRT_OK;
}

OPERATE_RET ai_player_resample_deinit(void)
{
    return OPRT_OK;
}

OPERATE_RET ai_player_resample_process(uint8_t *in_buf, DECODER_OUTPUT_T *in_cfg, uint8_t *out_buf, int *out_size)
{
    if(in_buf == NULL || out_buf == NULL || out_size == NULL || *out_size <= 0 || in_cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    size_t out_frames_out = 0;
    OPERATE_RET ret = -1;

    ret = resample_to_16k_fixed((const int16_t *)in_buf, in_cfg->samples, in_cfg->sample, in_cfg->channel, (int16_t *)out_buf, &out_frames_out);
    if(ret == 0) {
        *out_size = out_frames_out * 2;
    }

    // PR_DEBUG("resample in %d out %d", in_cfg->samples, out_frames_out);

    return ret;
}
