/**
 * @file ai_ui_image_album.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_IMAGE_ALBUM_H__
#define __AI_UI_IMAGE_ALBUM_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

void ai_ui_image_album_open(void);

void ai_ui_image_album_view_next(void);

void ai_ui_image_album_view_prev(void);

void ai_ui_image_album_view_all_open(void);

void ai_ui_image_album_view_all_close(void);

void ai_ui_image_album_select_open(void);

void ai_ui_image_album_select_close(void);

void ai_ui_image_album_get_img(char *name, AI_UI_IMG_T *img);

OPERATE_RET ai_ui_image_album_get_latest_img(AI_UI_IMG_T *img);

void ai_ui_image_album_free_img(AI_UI_IMG_T *img);

void ai_ui_image_album_reload(void);

void ai_ui_image_album_close(void);

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
void ai_ui_image_album_show_print_result(bool ok);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_IMAGE_ALBUM_H__ */
