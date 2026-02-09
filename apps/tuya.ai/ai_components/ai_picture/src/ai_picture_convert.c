/**
 * @file ai_picture_convert.c
 * @brief Picture format conversion implementation.
 *
 * This file provides functions for converting picture formats, supporting
 * JPEG to YUV422, RGB565, and RGB888 conversions using hardware JPEG decoder.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_jpeg_codec.h"

#include "ai_picture_convert.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool                     is_eof;
    uint8_t                 *in_frame;
    uint32_t                 in_frame_size;
    uint32_t                 in_frame_offset;
    JPEG_DEC_OUT_FMT         jpeg_fmt;
    AI_PICTURE_INFO_T        out_info;
    AI_PICTURE_CONVERT_CFG_T cfg;
} AI_PICTURE_CONVERT_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_CONVERT_CTX_T *sg_convert_ctx = NULL;
static MUTEX_HANDLE              sg_mutex = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Free conversion context and release allocated memory.
 *
 * @param ctx Pointer to the conversion context structure.
 */
static void __free_convert_ctx(AI_PICTURE_CONVERT_CTX_T *ctx)
{
    if(ctx) {
        if(ctx->in_frame) {
            Free(ctx->in_frame);
            ctx->in_frame = NULL;
        }

        if(ctx->out_info.frame) {
            Free(ctx->out_info.frame);
            ctx->out_info.frame = NULL;
        }

        Free(ctx);
        ctx = NULL;
    }
}

/**
 * @brief Get JPEG decoder output format based on target frame format.
 *
 * @param out_fmt Target output frame format.
 * @param jpeg_fmt Pointer to store the corresponding JPEG decoder output format.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __get_jpeg_decode_fmt(TUYA_FRAME_FMT_E out_fmt, JPEG_DEC_OUT_FMT *jpeg_fmt)
{
    if(NULL == jpeg_fmt) {
        return OPRT_INVALID_PARM;
    }

    if(out_fmt == TUYA_FRAME_FMT_YUV422) {
        *jpeg_fmt = JPEG_DEC_OUT_YUV422;
    }else if(out_fmt == TUYA_FRAME_FMT_RGB565) {
        *jpeg_fmt = JPEG_DEC_OUT_RGB565;
    }else if(out_fmt == TUYA_FRAME_FMT_RGB888) {
        *jpeg_fmt = JPEG_DEC_OUT_RGB888;
    }else {
        PR_ERR("not support out_fmt %d", out_fmt);
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

/**
 * @brief Start picture format conversion with specified configuration.
 *
 * @param cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_start(AI_PICTURE_CONVERT_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    JPEG_DEC_OUT_FMT jpeg_fmt = 0;

    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    if(cfg->in_fmt != TUYA_FRAME_FMT_JPEG) {
        PR_ERR("only support in_fmt JPEG");
        return OPRT_NOT_SUPPORTED;
    }

    if(cfg->in_frame_size == 0) {
        PR_ERR("invalid convert cfg");
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__get_jpeg_decode_fmt(cfg->out_fmt, &jpeg_fmt));

    if(sg_mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_mutex));
    }

    sg_convert_ctx = (AI_PICTURE_CONVERT_CTX_T *)Malloc(sizeof(AI_PICTURE_CONVERT_CTX_T));
    TUYA_CHECK_NULL_RETURN(sg_convert_ctx, OPRT_MALLOC_FAILED);
    memset(sg_convert_ctx, 0, sizeof(AI_PICTURE_CONVERT_CTX_T));

    memcpy(&sg_convert_ctx->cfg, cfg, sizeof(AI_PICTURE_CONVERT_CFG_T));

    sg_convert_ctx->in_frame_size = cfg->in_frame_size;
    sg_convert_ctx->in_frame = (uint8_t *)Malloc(sg_convert_ctx->in_frame_size);
    TUYA_CHECK_NULL_GOTO(sg_convert_ctx->in_frame, __START_ERR);
    memset(sg_convert_ctx->in_frame, 0, sg_convert_ctx->in_frame_size);

    sg_convert_ctx->jpeg_fmt = jpeg_fmt;

    return OPRT_OK;

__START_ERR:
    __free_convert_ctx(sg_convert_ctx);
    sg_convert_ctx = NULL;

    return rt;
}

/**
 * @brief Feed input data for conversion.
 *
 * @param data Pointer to the input data buffer.
 * @param len Length of the input data in bytes.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_feed(uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(sg_mutex, OPRT_INVALID_PARM);

    if(NULL == data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    if(NULL == sg_convert_ctx || NULL == sg_convert_ctx->in_frame) {
        PR_ERR("convert not started");
        return OPRT_COM_ERROR;
    }

    if(sg_convert_ctx->in_frame_offset>= sg_convert_ctx->in_frame_size) {
        PR_ERR("feed data exceed in_frame_size");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_mutex);

    uint32_t copy_len = (len > (sg_convert_ctx->in_frame_size - sg_convert_ctx->in_frame_offset)) ? \
                        (sg_convert_ctx->in_frame_size - sg_convert_ctx->in_frame_offset) : len;

    memcpy(sg_convert_ctx->in_frame + sg_convert_ctx->in_frame_offset, data, copy_len);
    sg_convert_ctx->in_frame_offset += copy_len;

    tal_mutex_unlock(sg_mutex);

    return OPRT_OK;
}

/**
 * @brief Perform picture format conversion and get output information.
 *
 * @param out_info Pointer to store the output picture information.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert(AI_PICTURE_INFO_T *out_info)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(out_info, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(sg_mutex, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_mutex);

    if(NULL == sg_convert_ctx || NULL == sg_convert_ctx->in_frame) {
        PR_ERR("convert not started");
        tal_mutex_unlock(sg_mutex);
        return OPRT_COM_ERROR;
    }

    TKL_JPEG_CODEC_INFO_T jpeg_info;
    memset(&jpeg_info, 0, sizeof(TKL_JPEG_CODEC_INFO_T));
    TUYA_CALL_ERR_GOTO(tkl_jpeg_codec_img_info_get(sg_convert_ctx->in_frame,\
                                                   sg_convert_ctx->in_frame_offset,\
                                                   &jpeg_info), __CONVER_ERR);
    sg_convert_ctx->out_info.fmt    = sg_convert_ctx->cfg.out_fmt;
    sg_convert_ctx->out_info.width  = jpeg_info.out_width;
    sg_convert_ctx->out_info.height = jpeg_info.out_height;

    sg_convert_ctx->out_info.frame_size = jpeg_info.out_width * jpeg_info.out_height * \
                                          ((sg_convert_ctx->jpeg_fmt == JPEG_DEC_OUT_RGB888) ? 3 : 2);

    if(sg_convert_ctx->out_info.frame) {
        Free(sg_convert_ctx->out_info.frame);
        sg_convert_ctx->out_info.frame = NULL;
    }

    sg_convert_ctx->out_info.frame = (uint8_t *)Malloc(sg_convert_ctx->out_info.frame_size);
    TUYA_CHECK_NULL_GOTO(sg_convert_ctx->out_info.frame, __CONVER_ERR);
    memset(sg_convert_ctx->out_info.frame, 0, sg_convert_ctx->out_info.frame_size);    

    TUYA_CALL_ERR_GOTO(tkl_jpeg_codec_convert(sg_convert_ctx->in_frame, \
                                              sg_convert_ctx->out_info.frame, \
                                              &jpeg_info, \
                                              sg_convert_ctx->jpeg_fmt), __CONVER_ERR);

    memcpy(out_info, &sg_convert_ctx->out_info, sizeof(AI_PICTURE_INFO_T));

    tal_mutex_unlock(sg_mutex);

    return OPRT_OK;

__CONVER_ERR:
    if(sg_convert_ctx->out_info.frame) {
        Free(sg_convert_ctx->out_info.frame);
        sg_convert_ctx->out_info.frame = NULL;
    }

    tal_mutex_unlock(sg_mutex);

    return rt;
}

/**
 * @brief Stop picture format conversion and free resources.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_stop(void)
{
    TUYA_CHECK_NULL_RETURN(sg_mutex, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_mutex);

    __free_convert_ctx(sg_convert_ctx);
    sg_convert_ctx = NULL;

    tal_mutex_unlock(sg_mutex);

    return OPRT_OK;
}