/**
 * @file u8g2_port_setup.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_semaphore.h"
#include "tal_memory.h"

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdl_display_manage.h"

#include "u8g2.h"
#include "u8g2_port.h"
#include "u8g2_port_setup.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
typedef struct {
    TDL_DISP_HANDLE_T      hdl;
    TDL_DISP_DEV_INFO_T    info;
    TDL_FB_MANAGE_HANDLE_T fb_mag_hdl;
    TDL_DISP_FRAME_BUFF_T *fb;
    u8x8_display_info_t    u8x8_info;
}U8G2_PORT_DISP_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __u8x8_port_display_setup_memory(TDL_DISP_DEV_INFO_T *disp_info,\
                                             u8x8_display_info_t *u8x8_info)
{
    if((disp_info == NULL) || (u8x8_info == NULL)) {
        return;
    }

    u8x8_info->tile_width   =  (disp_info->width+7)/8;
    u8x8_info->tile_height  =  (disp_info->height+7) / 8;

    u8x8_info->pixel_width  =  disp_info->width;
    u8x8_info->pixel_height =  disp_info->height;
}

static void __u8x8_port_display_draw_tile(u8x8_tile_t *tile, U8G2_PORT_DISP_T *disp)
{
    TDL_DISP_RECT_T rect;

    if((tile == NULL) || (disp == NULL)) {
        return;
    }

    if((tile->y_pos * 8) >= disp->info.height) {
        return;
    }

    if(NULL == disp->fb) {
        disp->fb = tdl_disp_get_free_fb(disp->fb_mag_hdl);
        if(NULL == disp->fb) {
            PR_ERR("get free fb failed\r\n");
            return;
        }
    }

    /* Initialize rect structure */
    memset(&rect, 0, sizeof(rect));

    rect.x0 = tile->x_pos * 8;
    rect.y0 = tile->y_pos * 8;
    rect.x1 = rect.x0 + tile->cnt * 8 -1;
    rect.y1 = ((rect.y0 + 7) > disp->info.height) ? (disp->info.height - 1) : (rect.y0 + 7);

    tdl_disp_draw_copy(disp->fb, &rect, tile->tile_ptr);

    if((rect.y1 + 1 )>= disp->info.height) {
        tdl_disp_dev_flush(disp->hdl, disp->fb);
        disp->fb = NULL;
    }
}

static uint8_t __u8x8_port_display_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    OPERATE_RET rt = OPRT_OK;
    U8G2_PORT_DISP_T *disp = NULL;

    if(NULL ==  u8x8) {
        return 0;
    }

    disp = (U8G2_PORT_DISP_T *)u8g2_GetUserPtr(u8x8);
    if(NULL == disp) {
        return 0;
    }

    switch(msg) {
        case U8X8_MSG_DISPLAY_SETUP_MEMORY: {
            __u8x8_port_display_setup_memory(&disp->info, &disp->u8x8_info);
            u8x8_d_helper_display_setup_memory(u8x8, &disp->u8x8_info);
        }
        break;
        case U8X8_MSG_DISPLAY_INIT: {
           TUYA_CALL_ERR_LOG(tdl_disp_dev_open(disp->hdl));
           TUYA_CALL_ERR_LOG(tdl_disp_set_brightness(disp->hdl, 100));
        }
        break;
        case U8X8_MSG_DISPLAY_DRAW_TILE: {
            u8x8_tile_t *tile = (u8x8_tile_t *)arg_ptr;

            __u8x8_port_display_draw_tile(tile, disp);
        }
        break;
        default:
            return 0;
    }

    return (rt == OPRT_OK) ? 1 : 0;
}

static U8G2_PORT_DISP_T *__u8x8_create_port_disp(void *disp_name)
{
    OPERATE_RET rt = OPRT_OK;
    U8G2_PORT_DISP_T *disp = NULL;
    TDL_DISP_HANDLE_T disp_hdl = NULL;

    disp_hdl = tdl_disp_find_dev((char *)disp_name);
    TUYA_CHECK_NULL_GOTO(disp_hdl, __CREATE_FAILE);

    disp = (U8G2_PORT_DISP_T *)Malloc(sizeof(U8G2_PORT_DISP_T));
    TUYA_CHECK_NULL_GOTO(disp, __CREATE_FAILE);
    {
        /* Clear the structure manually */
        U8G2_PORT_DISP_T zero_disp = {0};
        *disp = zero_disp;
    }

    disp->hdl = disp_hdl;
    TUYA_CALL_ERR_GOTO(tdl_disp_dev_get_info(disp->hdl, &disp->info), __CREATE_FAILE);
    if(disp->info.has_vram == false) {
        PR_ERR("display has no vram not support\r\n");
        goto __CREATE_FAILE;
    }

    TUYA_CALL_ERR_GOTO(tdl_disp_fb_manage_init(&disp->fb_mag_hdl), __CREATE_FAILE);

    TUYA_CALL_ERR_GOTO(tdl_disp_fb_manage_add(disp->fb_mag_hdl, disp->info.fmt,\
                                              disp->info.width, disp->info.height),\
                                              __CREATE_FAILE);

    return disp;

__CREATE_FAILE:
    if(disp) {
        if(disp->fb_mag_hdl) {
            tdl_disp_fb_manage_release(&disp->fb_mag_hdl);
            disp->fb_mag_hdl = NULL;
        }

        Free(disp);
        disp = NULL;
    }

    return NULL;
}

/*
 * Low-level horizontal/vertical line drawing function for horizontal byte layout
 * Memory layout: Bytes are arranged horizontally (row by row)
 *                LSB (bit 0) is on the left, MSB (bit 7) is on the right
 *                Pixel 0 (left) -> bit 0 (LSB), Pixel 1 -> bit 1, ..., Pixel 7 (right) -> bit 7 (MSB)
 * 
 * This matches display hardware that expects LSB-first bit order
 */
static void __u8g2_ll_hvline_horizontal(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t len, uint8_t dir)
{
    uint16_t offset;
    uint8_t *ptr;
    uint8_t bit_pos;
    uint8_t mask;
    uint8_t or_mask, xor_mask;
    uint8_t tile_width = u8g2_GetU8x8(u8g2)->display_info->tile_width;

    if (len == 0) {
        return;
    }

    /* Calculate bit position within byte (0-7) */
    /* Pixel 0 is on the left, pixel 7 is on the right */
    /* Pixel 0 -> bit 0 (LSB), Pixel 1 -> bit 1, ..., Pixel 7 -> bit 7 (MSB) */
    bit_pos = x;
    bit_pos &= 7;  /* Only the lowest 3 bits are needed */
    
    /* LSB is on the left, so start with bit 0 and shift left (toward MSB) */
    /* x=0 -> bit 0 (LSB), x=1 -> bit 1, ..., x=7 -> bit 7 (MSB) */
    mask = 1;  /* 0x01 = LSB (bit 0) */
    mask <<= bit_pos;

    /* Calculate OR and XOR masks based on draw_color */
    or_mask = 0;
    xor_mask = 0;
    if (u8g2->draw_color <= 1) {
        or_mask = mask;
    }
    if (u8g2->draw_color != 1) {
        xor_mask = mask;
    }

    /* Calculate byte offset in buffer */
    offset = y;  /* y might be 8 or 16 bit, but we need 16 bit */
    offset *= tile_width;
    offset += x >> 3;  /* x divided by 8 to get byte position */
    ptr = u8g2->tile_buf_ptr;
    if (ptr == NULL) {
        return;
    }
    ptr += offset;
    
    if (dir == 0) {  /* Horizontal line (left to right: pixel 0 -> pixel N) */
        do {
            /* Apply pixel operation based on draw_color */
            /* draw_color = 0: clear pixel (or_mask=mask, xor_mask=mask) -> *ptr |= mask; *ptr ^= mask; -> clears bit */
            /* draw_color = 1: set pixel (or_mask=mask, xor_mask=0) -> *ptr |= mask; -> sets bit */
            /* draw_color = 2: XOR pixel (or_mask=0, xor_mask=mask) -> *ptr ^= mask; -> XORs bit */
            if (u8g2->draw_color <= 1) {
                *ptr |= or_mask;
            }
            if (u8g2->draw_color != 1) {
                *ptr ^= xor_mask;
            }
            
            /* Move to next bit (leftward, toward MSB) */
            /* This moves from pixel N to pixel N+1 (left to right) */
            mask <<= 1;
            or_mask <<= 1;
            xor_mask <<= 1;
            
            if (mask == 0) {  /* Crossed to next byte (mask overflowed from 128 to 0) */
                mask = 1;  /* Reset to LSB (bit 0) for next byte */
                ptr++;  /* Move to next byte */
                
                /* Recalculate OR and XOR masks for new byte */
                if (u8g2->draw_color <= 1) {
                    or_mask = 1;
                } else {
                    or_mask = 0;
                }
                if (u8g2->draw_color != 1) {
                    xor_mask = 1;
                } else {
                    xor_mask = 0;
                }
            }
            
            len--;
        } while (len != 0);
    } else {  /* Vertical line (top to bottom) */
        /* For vertical line, x position is fixed, so mask doesn't change */
        /* We just move to the next row (next byte in the same column) */
        do {
            /* Apply pixel operation based on draw_color */
            if (u8g2->draw_color <= 1) {
                *ptr |= or_mask;
            }
            if (u8g2->draw_color != 1) {
                *ptr ^= xor_mask;
            }
            
            /* Move to next row (same x position, different y) */
            ptr += tile_width;
            len--;
        } while (len != 0);
    }
}

OPERATE_RET u8g2_Setup_tdl_display_f(u8g2_t *u8g2, void *disp_name)
{
    U8G2_PORT_DISP_T *disp = NULL;

    if(NULL == u8g2) {
        return OPRT_INVALID_PARM;
    }

    disp = __u8x8_create_port_disp(disp_name);
    if(NULL == disp) {
        PR_ERR("create port disp failed\r\n");
        return OPRT_COM_ERROR;
    }
    u8x8_GetUserPtr(u8g2_GetU8x8(u8g2)) = (void *)disp;

    u8g2_SetupDisplay(u8g2, __u8x8_port_display_cb, NULL, NULL, NULL);

    uint8_t tile_buf_height = (disp->info.height + 7) / 8;

    PR_DEBUG("tile_buf_height:%d\r\n", tile_buf_height);
    
    u8g2_SetupBuffer(u8g2, NULL, tile_buf_height, __u8g2_ll_hvline_horizontal, U8G2_R0);

    u8g2_alloc_buffer(u8g2);

    return OPRT_OK;
}

#endif