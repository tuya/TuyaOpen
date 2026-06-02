/**
 * @file ai_ui_page.c
 * @brief AI UI page manager — tracks the active page and handles open/close transitions.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "ai_ui_page.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_PAGE_INTFS_T sg_page_intfs[AI_UI_PAGE_MAX];
static AI_UI_PAGE_E       sg_current_page = AI_UI_PAGE_DEFAULT;
static AI_UI_PAGE_E       sg_page_stack[AI_UI_PAGE_STACK_DEPTH];
static int                sg_stack_top = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __stack_push(AI_UI_PAGE_E page)
{
    if (sg_stack_top < AI_UI_PAGE_STACK_DEPTH) {
        sg_page_stack[sg_stack_top++] = page;
    }
}

static AI_UI_PAGE_E __stack_pop(void)
{
    if (sg_stack_top > 0) {
        return sg_page_stack[--sg_stack_top];
    }
    return AI_UI_PAGE_DEFAULT;
}

OPERATE_RET ai_ui_page_register(AI_UI_PAGE_E page, AI_UI_PAGE_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    if (page >= AI_UI_PAGE_MAX) {
        return OPRT_INVALID_PARM;
    }

    sg_page_intfs[page] = *intfs;

    return OPRT_OK;
}

OPERATE_RET ai_ui_page_open(AI_UI_PAGE_E page, void *arg)
{
    if (page >= AI_UI_PAGE_MAX) {
        return OPRT_INVALID_PARM;
    }

    if (page == sg_current_page) {
        return OPRT_OK;
    }

    AI_UI_PAGE_E prev = sg_current_page;

    /* Push current page onto history stack */
    __stack_push(prev);

    /* Open the new page FIRST — old page stays visible during slow opens,
     * preventing a flash of the background page. */
    OPERATE_RET rt = OPRT_OK;
    if (sg_page_intfs[page].open) {
        rt = sg_page_intfs[page].open(arg);
    }

    if (rt == OPRT_OK) {
        /* New page is ready — now close the old one */
        if (sg_page_intfs[prev].close) {
            sg_page_intfs[prev].close();
        }
        sg_current_page = page;
    } else {
        /* Open failed — old page is still visible, just pop the stack */
        __stack_pop();
    }

    return rt;
}

OPERATE_RET ai_ui_page_close(void)
{
    if (sg_current_page == AI_UI_PAGE_DEFAULT && sg_stack_top == 0) {
        return OPRT_OK;
    }

    /* Close the current page */
    if (sg_page_intfs[sg_current_page].close) {
        sg_page_intfs[sg_current_page].close();
    }

    /* Pop the previous page from the stack */
    AI_UI_PAGE_E prev = __stack_pop();

    OPERATE_RET rt = OPRT_OK;
    if (sg_page_intfs[prev].open) {
        rt = sg_page_intfs[prev].open(NULL);
    }

    sg_current_page = prev;

    return rt;
}

AI_UI_PAGE_E ai_ui_page_get_current(void)
{
    return sg_current_page;
}

void ai_ui_page_set_current(AI_UI_PAGE_E page)
{
    if (page < AI_UI_PAGE_MAX) {
        sg_current_page = page;
    }
}
