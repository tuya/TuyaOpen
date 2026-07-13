/**
 * @file ai_ui_camera.c
 * @brief ai_ui_camera module — bridges camera preview with platform UI callbacks.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO ==1)
#include "ai_ui_camera.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM ==1)
#include "ai_ui_image_album.h"
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
static AI_UI_CAMERA_INTFS_T sg_camera_intfs;

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET ai_ui_camera_register(AI_UI_CAMERA_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_camera_intfs, intfs, sizeof(AI_UI_CAMERA_INTFS_T));

    return OPRT_OK;
}

OPERATE_RET ai_ui_camera_open(void)
{
    if (sg_camera_intfs.disp_open) {
        sg_camera_intfs.disp_open();
    }


#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM ==1)
    AI_UI_IMG_T img = {0};
    img.data = NULL;
    OPERATE_RET rt = ai_ui_image_album_get_latest_img(&img);
    if(rt == OPRT_OK) {
        if (sg_camera_intfs.disp_set_thumbnail_jpeg) {
            sg_camera_intfs.disp_set_thumbnail_jpeg(img.data, img.len);
        }
    }else {
        if (sg_camera_intfs.disp_set_thumbnail_jpeg) {
            sg_camera_intfs.disp_set_thumbnail_jpeg(NULL, 0);
        }
    }

    ai_ui_image_album_free_img(&img);
#endif

    return OPRT_OK;
}

OPERATE_RET ai_ui_camera_flush(AI_UI_VIDEO_T *video)
{
    if (NULL == video) {
        return OPRT_INVALID_PARM;
    }

    if (sg_camera_intfs.disp_yuv_flush) {
        sg_camera_intfs.disp_yuv_flush(video);
    }

    return OPRT_OK;
}

OPERATE_RET ai_ui_camera_set_thumbnail_jpeg(uint8_t *jpeg, uint32_t len)
{
    if (NULL == jpeg || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    if (sg_camera_intfs.disp_set_thumbnail_jpeg) {
        sg_camera_intfs.disp_set_thumbnail_jpeg(jpeg, len);
    }

    return OPRT_OK;
}

OPERATE_RET ai_ui_camera_close(void)
{
    if (sg_camera_intfs.disp_close) {
        sg_camera_intfs.disp_close();
    }

    return OPRT_OK;
}
#endif