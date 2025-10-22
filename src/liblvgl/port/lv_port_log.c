/**
 * @file lv_port_log.c
 * @brief lv_port_log module is used to
 * @version 0.1
 * @date 2025-05-20
 */

#include "tal_log.h"

#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

#if LV_USE_LOG
void lv_port_log_print(lv_log_level_t level, const char *buf)
{
    TAL_LOG_LEVEL_E log_level = TAL_LOG_LEVEL_DEBUG;

    switch (level) {
    case LV_LOG_LEVEL_TRACE: {
        log_level = TAL_LOG_LEVEL_TRACE;
    } break;
    case LV_LOG_LEVEL_INFO: {
        log_level = TAL_LOG_LEVEL_INFO;
    } break;
    case LV_LOG_LEVEL_WARN: {
        log_level = TAL_LOG_LEVEL_WARN;
    } break;
    case LV_LOG_LEVEL_ERROR: {
        log_level = TAL_LOG_LEVEL_ERR;
    } break;
    case LV_LOG_LEVEL_USER:
    case LV_LOG_LEVEL_NONE: {
        log_level = TAL_LOG_LEVEL_DEBUG;
    } break;
    default: {
        log_level = TAL_LOG_LEVEL_DEBUG;
    } break;
    }

    tal_log_print(log_level, "lvgl", __LINE__, (char *)buf);
}
#endif