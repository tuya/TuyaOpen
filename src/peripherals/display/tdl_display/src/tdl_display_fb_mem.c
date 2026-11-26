/**
 * @file tdl_display_fb_mem.c
 * @brief Frame buffer allocation for display operations.
 *
 * The frame buffer descriptor is always allocated from internal memory. Only
 * the pixel storage follows DISP_FB_RAM_TP_E. This keeps descriptor field
 * updates away from external memory implementations that only guarantee
 * aligned word accesses.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdl_display_manage.h"
#include "tkl_memory.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define TDL_DISP_DRAW_BUF_ALIGN 64U

#if (defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)) || \
    (defined(ENABLE_PSRAM) && (ENABLE_PSRAM == 1))
#define TDL_DISP_PSRAM_ENABLED 1
#else
#define TDL_DISP_PSRAM_ENABLED 0
#endif

TDL_DISP_FRAME_BUFF_T *tdl_disp_create_frame_buff(DISP_FB_RAM_TP_E type, uint32_t len)
{
    TDL_DISP_FRAME_BUFF_T *fb = NULL;
    DISP_FB_RAM_TP_E fb_type = DISP_FB_TP_SRAM;
    uint8_t *frame_alloc = NULL;
    uint8_t *frame = NULL;
    uint32_t frame_alloc_len = 0U;

    if (len > UINT32_MAX - (TDL_DISP_DRAW_BUF_ALIGN - 1U)) {
        return NULL;
    }
    frame_alloc_len = len + TDL_DISP_DRAW_BUF_ALIGN - 1U;

    fb = (TDL_DISP_FRAME_BUFF_T *)tkl_system_malloc(sizeof(*fb));
    if (fb == NULL) {
        return NULL;
    }
    memset(fb, 0, sizeof(*fb));

    if (type == DISP_FB_TP_PSRAM) {
#if TDL_DISP_PSRAM_ENABLED
        frame_alloc = (uint8_t *)tkl_system_psram_malloc(frame_alloc_len);
        if (frame_alloc == NULL) {
            tkl_system_free(fb);
            return NULL;
        }
        fb_type = DISP_FB_TP_PSRAM;
#else
        frame_alloc = (uint8_t *)tkl_system_malloc(frame_alloc_len);
        if (frame_alloc == NULL) {
            tkl_system_free(fb);
            return NULL;
        }
        fb_type = DISP_FB_TP_SRAM;
#endif
    } else {
        frame_alloc = (uint8_t *)tkl_system_malloc(frame_alloc_len);
        if (frame_alloc == NULL) {
            tkl_system_free(fb);
            return NULL;
        }
        fb_type = DISP_FB_TP_SRAM;
    }

    frame = frame_alloc + TDL_DISP_DRAW_BUF_ALIGN - 1U;
    frame = (uint8_t *)((uintptr_t)frame & ~(uintptr_t)(TDL_DISP_DRAW_BUF_ALIGN - 1U));

    if (fb_type == DISP_FB_TP_PSRAM) {
#if defined(ENABLE_PSRAM) && (ENABLE_PSRAM == 1)
        if (len != 0U && tkl_system_memset(frame, 0, len) == NULL) {
            tkl_system_psram_free(frame_alloc);
            tkl_system_free(fb);
            return NULL;
        }
#else
        memset(frame, 0, len);
#endif
    } else {
        memset(frame, 0, len);
    }

    fb->type = fb_type;
    fb->len = len;
    fb->frame = frame;
    fb->frame_alloc = frame_alloc;
    return fb;
}

void tdl_disp_free_frame_buff(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    if (frame_buff == NULL) {
        return;
    }

    if (frame_buff->frame_alloc != NULL) {
        if (frame_buff->type == DISP_FB_TP_PSRAM) {
#if TDL_DISP_PSRAM_ENABLED
            tkl_system_psram_free(frame_buff->frame_alloc);
#else
            tkl_system_free(frame_buff->frame_alloc);
#endif
        } else {
            tkl_system_free(frame_buff->frame_alloc);
        }
        tkl_system_free(frame_buff);
        return;
    }

    /* Compatibility with frame buffers created before split allocation. */
    if (frame_buff->type == DISP_FB_TP_PSRAM) {
#if TDL_DISP_PSRAM_ENABLED
        tkl_system_psram_free(frame_buff);
#else
        tkl_system_free(frame_buff);
#endif
    } else {
        tkl_system_free(frame_buff);
    }
}
