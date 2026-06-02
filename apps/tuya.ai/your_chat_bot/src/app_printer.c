/**
 * @file app_printer.c
 * @brief app_printer module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
#include "tdl_printer_manage.h"
#include "tal_image_jpeg_codec.h"

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "image_album.h"
#include "ai_picture.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_PRINTER_HANDLE sg_printer_hdl = NULL;
static MUTEX_HANDLE       sg_print_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __printer_mutex_init(void)
{
    if (sg_print_mutex != NULL) {
        return OPRT_OK;
    }
    return tal_mutex_create_init(&sg_print_mutex);
}

static OPERATE_RET __printer_lock_acquire(void)
{
    if (sg_print_mutex == NULL) {
        __printer_mutex_init();
    }
    return tal_mutex_lock(sg_print_mutex);
}

static void __printer_lock_release(void)
{
    if (sg_print_mutex != NULL) {
        tal_mutex_unlock(sg_print_mutex);
    }
}

OPERATE_RET app_print_jpeg_img(uint8_t *jpeg, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == jpeg || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET lock_rt = __printer_lock_acquire();
    if (lock_rt != OPRT_OK) {
        PR_WARN("print: printer is busy, rejecting request");
        return OPRT_COM_ERROR;
    }

    /* Reject non-JPEG payloads up front. The image album can now hold both
     * JPEG and PNG (a recent feature), but the printer pipeline only knows
     * how to decode JPEG. Without this guard tal_image_jpeg_get_info()
     * just returns OPRT_INVALID_PARM (-2) which is hard to read at the
     * call site. JPEG SOI marker = 0xFF 0xD8 0xFF. */
    if (len < 3 ||
        jpeg[0] != 0xFF || jpeg[1] != 0xD8 || jpeg[2] != 0xFF) {
        PR_ERR("print: unsupported image format "
               "(magic=%02x %02x %02x, len=%u). "
               "Only JPEG is supported.",
               (len > 0) ? jpeg[0] : 0,
               (len > 1) ? jpeg[1] : 0,
               (len > 2) ? jpeg[2] : 0, len);
        __printer_lock_release();
        return OPRT_NOT_SUPPORTED;
    }

    TAL_IMAGE_JPEG_INFO_T jpeg_info = {0};
    rt = tal_image_jpeg_get_info(jpeg, len, &jpeg_info);
    if (rt != OPRT_OK) {
        PR_ERR("print: jpeg_get_info failed, rt:%d", rt);
        __printer_lock_release();
        return rt;
    }

    if (NULL == sg_printer_hdl) {
        rt = tdl_printer_find(PRINTER_NAME, &sg_printer_hdl);
        if (rt != OPRT_OK) {
            PR_ERR("print: printer_find failed, rt:%d", rt);
            __printer_lock_release();
            return rt;
        }
    }

    rt = tdl_printer_open(sg_printer_hdl, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("print: printer_open failed, rt:%d", rt);
        __printer_lock_release();
        return rt;
    }

    TDL_PRINTER_DEV_INFO_T dev_info = {0};
    tdl_printer_get_dev_info(sg_printer_hdl, &dev_info);
    uint16_t print_width = (uint16_t)dev_info.dots_per_line;
    if (0 == print_width) {
        PR_ERR("print: invalid dots_per_line");
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_INVALID_PARM;
    }

    uint16_t src_width  = (uint16_t)jpeg_info.width;
    uint16_t src_height = (uint16_t)jpeg_info.height;

    /* Decode directly to printer dimensions (scale down if image is wider) */
    uint16_t out_w = (src_width > print_width) ? print_width : src_width;
    uint16_t out_h = (src_width > print_width)
                     ? (uint16_t)((uint32_t)src_height * print_width / src_width)
                     : src_height;
    if (out_h == 0) {
        out_h = 1;
    }
    uint32_t bitmap_bytes = ((uint32_t)(out_w + 7u) / 8u) * out_h;

    uint8_t *bitmap_buf = (uint8_t *)Malloc(bitmap_bytes);
    if (NULL == bitmap_buf) {
        PR_ERR("print: malloc %u bytes for bitmap failed", bitmap_bytes);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T out = {
        .out_buf      = bitmap_buf,
        .out_buf_size = bitmap_bytes,
        .out_width    = out_w,
        .out_height   = out_h,
    };
    rt = tal_image_jpeg_decode_bitmap(jpeg, (uint32_t)len, &out, 128);
    if (rt != OPRT_OK) {
        PR_ERR("print: jpeg_decode_bitmap failed, rt:%d", rt);
        Free(bitmap_buf);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return rt;
    }

    tdl_printer_start(sg_printer_hdl);

    rt = tdl_printer_send_bitmap(sg_printer_hdl, 0, out_w, out_h, bitmap_buf);
    Free(bitmap_buf);
    if (rt != OPRT_OK) {
        PR_ERR("print: send_bitmap failed, rt:%d", rt);
    }

    tdl_printer_end(sg_printer_hdl);

    tdl_printer_close(sg_printer_hdl);

    __printer_lock_release();

    return rt;
}

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)

OPERATE_RET app_print_img_from_album(const char *filename)
{
    OPERATE_RET rt = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_hdl = image_album_find_by_name(ai_picture_get_album_name());
    if (NULL == album_hdl) {
        PR_ERR("print: album not found");
        return OPRT_INVALID_PARM;
    }

    uint8_t *jpeg_data = NULL;
    size_t   jpeg_size = 0;
    rt = image_album_read(album_hdl, filename, 0, &jpeg_data, &jpeg_size);
    if (rt != OPRT_OK || NULL == jpeg_data) {
        PR_ERR("print: image_album_read \"%s\" failed, rt:%d", filename, rt);
        return (rt != OPRT_OK) ? rt : OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_LOG(app_print_jpeg_img(jpeg_data, (uint32_t)jpeg_size));

    image_album_free_file_data(jpeg_data);
    jpeg_data = NULL;

    return rt;
}
#endif

#endif