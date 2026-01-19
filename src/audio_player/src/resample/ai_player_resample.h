/**
 * @file ai_player_resample.h
 * @brief 
 * @version 0.1
 * @date 2025-09-22
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

#ifndef __AI_PLAYER_RESAMPLE_H__
#define __AI_PLAYER_RESAMPLE_H__
#include "tuya_cloud_types.h"
#include "../decoder/ai_player_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET ai_player_resample_init(TKL_AUDIO_SAMPLE_E sample, TKL_AUDIO_DATABITS_E datebits, TKL_AUDIO_CHANNEL_E channel);
OPERATE_RET ai_player_resample_deinit(void);
OPERATE_RET ai_player_resample_process(uint8_t *in_buf, DECODER_OUTPUT_T *in_cfg, uint8_t *out_buf, int *out_size);

#ifdef __cplusplus
}
#endif

#endif  // __AI_PLAYER_RESAMPLE_H__
