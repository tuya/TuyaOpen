/**
 * @file datasink_cfg.h
 * @brief 
 * @version 0.1
 * @date 2025-09-23
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

#ifndef __DATASINK_CFG_H__
#define __DATASINK_CFG_H__

#include "tuya_cloud_types.h"
#include "ai_player_datasink.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    OPERATE_RET (*start)(char *value, void* *handle);
    OPERATE_RET (*stop)(void* handle);
    OPERATE_RET (*exit)(void* handle);
    OPERATE_RET (*feed)(void* handle, uint8_t *data, uint32_t len);
    OPERATE_RET (*read)(void* handle, uint8_t *data, uint32_t len, uint32_t *out_len);
} DATASINK_T;

#ifdef __cplusplus
}
#endif

#endif // __DATASINK_CFG_H__
