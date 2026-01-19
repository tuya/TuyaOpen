/**
 * @file mixer_fixed.h
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

#ifndef __AI_MIXER_FIXED_H__
#define __AI_MIXER_FIXED_H__
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET ai_player_mixer_process(uint8_t *src_decode_buf, uint8_t *dst_decode_buf, uint32_t samples);

OPERATE_RET ai_player_volume_process(uint8_t *decode_buf, uint32_t len, int volume, int max_volume, uint32_t datebits);

#ifdef __cplusplus
}
#endif

#endif  // __AI_MIXER_FIXED_H__
