/**
 * @file ai_ui_page.h
 * @brief AI UI page manager — tracks the active page and handles open/close transitions.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_PAGE_H__
#define __AI_UI_PAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_UI_PAGE_DEFAULT      AI_UI_PAGE_CHAT
#define AI_UI_PAGE_STACK_DEPTH  4

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_UI_PAGE_CHAT,
    AI_UI_PAGE_CAMERA,
    AI_UI_PAGE_ALBUM_VIEW,
    AI_UI_PAGE_ALBUM_ALL,
    AI_UI_PAGE_ALBUM_SELECT,
    AI_UI_PAGE_MAX
} AI_UI_PAGE_E;

typedef struct {
    OPERATE_RET (*open)(void *arg);
    OPERATE_RET (*close)(void);
} AI_UI_PAGE_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register open/close callbacks for a page.
 *
 * @param page  Page identifier.
 * @param intfs Pointer to the interface callbacks.
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET ai_ui_page_register(AI_UI_PAGE_E page, AI_UI_PAGE_INTFS_T *intfs);

/**
 * @brief Switch to the specified page.
 *        Closes the current page first (if different), then opens the new one.
 *
 * @param page Page to open.
 * @param arg  Optional argument passed to the page's open callback (may be NULL).
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET ai_ui_page_open(AI_UI_PAGE_E page, void *arg);

/**
 * @brief Close the current page and return to the previous page in the history stack.
 *        If the stack is empty, opens the default page (AI_UI_PAGE_DEFAULT).
 *
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET ai_ui_page_close(void);

/**
 * @brief Get the currently active page.
 *
 * @return AI_UI_PAGE_E Current page identifier.
 */
AI_UI_PAGE_E ai_ui_page_get_current(void);

/**
 * @brief Update page tracking without triggering open/close callbacks.
 *        Used for sub-page switches within the same page group (e.g. album views).
 *
 * @param page New current page identifier.
 */
void ai_ui_page_set_current(AI_UI_PAGE_E page);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_PAGE_H__ */
