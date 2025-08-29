/**
 * @file tdl_touch_manage.c
 * @brief Touch device management layer implementation
 *
 * This file implements the TDL (Tuya Device Library) layer for touch device
 * management. It provides device registration, device discovery, and unified
 * touch interface functions for various touch controllers. The management layer
 * abstracts the underlying TDD drivers and provides a common API for touch operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_gpio.h"
#include "tkl_memory.h"

#include "tal_api.h"
#include "tuya_list.h"

#include "tdl_touch_driver.h"
#include "tdl_touch_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    struct tuya_list_head node;
    bool is_open;
    char name[TOUCH_DEV_NAME_MAX_LEN + 1];
    MUTEX_HANDLE mutex;

    TDD_TOUCH_DEV_HANDLE_T tdd_hdl;
    TDD_TOUCH_INTFS_T intfs;

    TDL_TOUCH_CONFIG_T config;
} TOUCH_DEVICE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static struct tuya_list_head sg_touch_list = LIST_HEAD_INIT(sg_touch_list);

/***********************************************************
***********************function define**********************
***********************************************************/
static TOUCH_DEVICE_T *__find_touch_device(char *name)
{
    TOUCH_DEVICE_T *touch_dev = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == name) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_touch_list)
    {
        touch_dev = tuya_list_entry(pos, TOUCH_DEVICE_T, node);
        if (0 == strncmp(touch_dev->name, name, TOUCH_DEV_NAME_MAX_LEN)) {
            return touch_dev;
        }
    }

    return NULL;
}

TDL_TOUCH_HANDLE_T tdl_touch_find_dev(char *name)
{
    return (TDL_TOUCH_HANDLE_T)__find_touch_device(name);
}

OPERATE_RET tdl_touch_dev_open(TDL_TOUCH_HANDLE_T touch_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    TOUCH_DEVICE_T *touch_dev = NULL;

    if (NULL == touch_hdl) {
        return OPRT_INVALID_PARM;
    }

    touch_dev = (TOUCH_DEVICE_T *)touch_hdl;

    if (touch_dev->is_open) {
        return OPRT_OK;
    }

    if (NULL == touch_dev->mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&touch_dev->mutex));
    }

    if (touch_dev->intfs.open) {
        TUYA_CALL_ERR_RETURN(touch_dev->intfs.open(touch_dev->tdd_hdl));
    }

    touch_dev->is_open = true;

    return OPRT_OK;
}

OPERATE_RET tdl_touch_dev_read(TDL_TOUCH_HANDLE_T touch_hdl, uint8_t max_num, TDL_TOUCH_POS_T *point,
                               uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TOUCH_DEVICE_T *touch_dev = NULL;

    if (NULL == touch_hdl || NULL == point || NULL == point_num) {
        return OPRT_INVALID_PARM;
    }

    touch_dev = (TOUCH_DEVICE_T *)touch_hdl;

    if (false == touch_dev->is_open) {
        return OPRT_COM_ERROR;
    }

    if (touch_dev->intfs.read) {
        tal_mutex_lock(touch_dev->mutex);
        rt = touch_dev->intfs.read(touch_dev->tdd_hdl, max_num, point, point_num);

        uint32_t adj_flags = ((touch_dev->config.flags.swap_xy) || (touch_dev->config.flags.mirror_x) ||
                              (touch_dev->config.flags.mirror_y));
        if (adj_flags) {
            // Apply adjustments to the touch points
            for (uint8_t i = 0; i < *point_num; i++) {
                if (touch_dev->config.flags.swap_xy) {
                    uint16_t temp = point[i].x;
                    point[i].x = point[i].y;
                    point[i].y = temp;
                }
                if (touch_dev->config.flags.mirror_x) {
                    point[i].x = touch_dev->config.x_max - point[i].x;
                }
                if (touch_dev->config.flags.mirror_y) {
                    point[i].y = touch_dev->config.y_max - point[i].y;
                }
            }
        }
        tal_mutex_unlock(touch_dev->mutex);
        if (OPRT_OK != rt) {
            PR_ERR("Failed to read touch data: %d", rt);
        }
    }

    return rt;
}

OPERATE_RET tdl_touch_dev_close(TDL_TOUCH_HANDLE_T touch_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    TOUCH_DEVICE_T *touch_dev = NULL;

    if (NULL == touch_hdl) {
        return OPRT_INVALID_PARM;
    }

    touch_dev = (TOUCH_DEVICE_T *)touch_hdl;

    if (false == touch_dev->is_open) {
        return OPRT_OK;
    }

    if (touch_dev->intfs.close) {
        TUYA_CALL_ERR_RETURN(touch_dev->intfs.close(touch_dev->tdd_hdl));
    }

    touch_dev->is_open = false;

    return OPRT_OK;
}

OPERATE_RET tdl_touch_device_register(char *name, TDD_TOUCH_DEV_HANDLE_T tdd_hdl, TDL_TOUCH_CONFIG_T *tp_cfg,
                                      TDD_TOUCH_INTFS_T *intfs)
{
    TOUCH_DEVICE_T *touch_dev = NULL;

    if (NULL == name || NULL == tdd_hdl || NULL == tp_cfg || NULL == intfs) {
        return OPRT_INVALID_PARM;
    }

    NEW_LIST_NODE(TOUCH_DEVICE_T, touch_dev);
    if (NULL == touch_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(touch_dev, 0, sizeof(TOUCH_DEVICE_T));

    strncpy(touch_dev->name, name, TOUCH_DEV_NAME_MAX_LEN);

    touch_dev->tdd_hdl = tdd_hdl;

    memcpy(&touch_dev->config, tp_cfg, sizeof(TDL_TOUCH_CONFIG_T));
    memcpy(&touch_dev->intfs, intfs, sizeof(TDD_TOUCH_INTFS_T));

    tuya_list_add(&touch_dev->node, &sg_touch_list);

    return OPRT_OK;
}