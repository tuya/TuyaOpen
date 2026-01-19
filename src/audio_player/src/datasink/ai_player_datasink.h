/**
 * @file ai_player_datasink.h
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

#ifndef __AI_PLAYER_DATASINK_H__
#define __AI_PLAYER_DATASINK_H__

#include "svc_ai_player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* PLAYER_DATASINK;

OPERATE_RET ai_player_datasink_init(PLAYER_DATASINK *handle);
OPERATE_RET ai_player_datasink_deinit(PLAYER_DATASINK handle);
OPERATE_RET ai_player_datasink_start(PLAYER_DATASINK handle, AI_PLAYER_SRC_E src, char *value);
OPERATE_RET ai_player_datasink_stop(PLAYER_DATASINK handle);
OPERATE_RET ai_player_datasink_read(PLAYER_DATASINK handle, uint8_t *buf, uint32_t len, uint32_t *out_len); // OPRT_NOT_FOUND -- eof
OPERATE_RET ai_player_datasink_feed(PLAYER_DATASINK handle, uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif  // __AI_PLAYER_DATASINK_H__
