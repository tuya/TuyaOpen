/**
 * @file tuya_ai_private.h
 * @author aoding.xia@tuya.com
 * @brief ai private header
 * @version 0.1
 * @date 2025-04-04
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
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
#ifndef __TUYA_AI_PRIVATE_H__
#define __TUYA_AI_PRIVATE_H__

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"

#include "tal_memory.h"
#include "tal_thread.h"

#if defined(AI_HEAP_IN_PSRAM) && (AI_HEAP_IN_PSRAM == 1)
#define OS_MALLOC(size)       tal_psram_malloc(size)
#define OS_FREE(ptr)          tal_psram_free(ptr)
#define OS_CALLOC(num, size)  tal_psram_calloc(num, size)
#define OS_REALLOC(ptr, size) tal_psram_realloc(ptr, size)
#else
#define OS_MALLOC(size)       tal_malloc(size)
#define OS_FREE(ptr)          tal_free(ptr)
#define OS_CALLOC(num, size)  tal_calloc(num, size)
#define OS_REALLOC(ptr, size) tal_realloc(ptr, size)
#endif

#endif
