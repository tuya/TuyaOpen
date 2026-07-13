/**
 * @file ai_ui_wechat_common.h
 * @brief Internal header shared among wechat UI page files.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_WECHAT_COMMON_H__
#define __AI_UI_WECHAT_COMMON_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Rounded-screen safe inset
 * --------------------------------------------------------------------------- */
#if defined(AI_UI_WECHAT_SCREEN_CORNER_RADIUS) && (AI_UI_WECHAT_SCREEN_CORNER_RADIUS > 0)
#define WECHAT_ROUNDED_CORNER_RADIUS   AI_UI_WECHAT_SCREEN_CORNER_RADIUS
#else
#define WECHAT_ROUNDED_CORNER_RADIUS   0
#endif

/**
 * Safe inset derived from physical corner radius R.
 * Max clip depth at 45° = R*(1-cos45°) ≈ R*0.293, so R/2 is conservative.
 */
#define WECHAT_SAFE_INSET  ((WECHAT_ROUNDED_CORNER_RADIUS + 1) / 2)

/**
 * Extended touch area added to every corner button so small targets near
 * the rounded-screen edge are easier to tap.
 */
#define WECHAT_BTN_EXT_CLICK  12

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */

/* Chat page */
void ai_ui_wechat_chat_init(lv_obj_t *parent);
void ai_ui_wechat_chat_register(void);

/* Camera page */
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
void ai_ui_wechat_camera_init(lv_obj_t *parent);
void ai_ui_wechat_camera_register(void);
#endif

/* Album page */
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
void ai_ui_wechat_album_init(lv_obj_t *parent);
void ai_ui_wechat_album_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_WECHAT_COMMON_H__ */
