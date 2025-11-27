/**
 * @file tdl_tp_manage.c
 * @brief Tp device management layer implementation
 *
 * This file implements the TDL (Tuya Device Library) layer for tp device
 * management. It provides device registration, device discovery, and unified
 * tp interface functions for various tp controllers. The management layer
 * abstracts the underlying TDD drivers and provides a common API for tp operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_gpio.h"
#include "tkl_memory.h"

#include "tal_api.h"
#include "tuya_list.h"

#include "tdl_tp_driver.h"
#include "tdl_tp_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    struct tuya_list_head node;
    bool is_open;
    char name[TP_DEV_NAME_MAX_LEN + 1];
    MUTEX_HANDLE mutex;

    TDD_TP_DEV_HANDLE_T tdd_hdl;
    TDD_TP_INTFS_T intfs;

    TDD_TP_CONFIG_T config;
} TP_DEVICE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static struct tuya_list_head sg_tp_list = LIST_HEAD_INIT(sg_tp_list);

/***********************************************************
***********************function define**********************
***********************************************************/
static TP_DEVICE_T *__find_tp_device(char *name)
{
    TP_DEVICE_T *tp_dev = NULL;
    struct tuya_list_head *pos = NULL;

    if (NULL == name) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_tp_list)
    {
        tp_dev = tuya_list_entry(pos, TP_DEVICE_T, node);
        if (0 == strncmp(tp_dev->name, name, TP_DEV_NAME_MAX_LEN)) {
            return tp_dev;
        }
    }

    return NULL;
}

TDL_TP_HANDLE_T tdl_tp_find_dev(char *name)
{
    return (TDL_TP_HANDLE_T)__find_tp_device(name);
}

OPERATE_RET tdl_tp_dev_open(TDL_TP_HANDLE_T tp_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    TP_DEVICE_T *tp_dev = NULL;

    if (NULL == tp_hdl) {
        return OPRT_INVALID_PARM;
    }

    tp_dev = (TP_DEVICE_T *)tp_hdl;

    if (tp_dev->is_open) {
        return OPRT_OK;
    }

    if (NULL == tp_dev->mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&tp_dev->mutex));
    }

    if (tp_dev->intfs.open) {
        TUYA_CALL_ERR_RETURN(tp_dev->intfs.open(tp_dev->tdd_hdl));
    }

    tp_dev->is_open = true;

    return OPRT_OK;
}

OPERATE_RET tdl_tp_dev_read(TDL_TP_HANDLE_T tp_hdl, uint8_t max_num, TDL_TP_POS_T *point,
                               uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TP_DEVICE_T *tp_dev = NULL;

    if (NULL == tp_hdl || NULL == point || NULL == point_num) {
        return OPRT_INVALID_PARM;
    }

    tp_dev = (TP_DEVICE_T *)tp_hdl;

    if (false == tp_dev->is_open) {
        return OPRT_COM_ERROR;
    }

    if (tp_dev->intfs.read) {
        tal_mutex_lock(tp_dev->mutex);
        rt = tp_dev->intfs.read(tp_dev->tdd_hdl, max_num, point, point_num);

        uint32_t adj_flags = ((tp_dev->config.flags.swap_xy) || (tp_dev->config.flags.mirror_x) ||
                              (tp_dev->config.flags.mirror_y));
        if (adj_flags) {
            // Apply adjustments to the tp points
            for (uint8_t i = 0; i < *point_num; i++) {
                if (tp_dev->config.flags.swap_xy) {
                    uint16_t temp = point[i].x;
                    point[i].x = point[i].y;
                    point[i].y = temp;
                }
                if (tp_dev->config.flags.mirror_x) {
                    point[i].x = tp_dev->config.x_max - point[i].x;
                }
                if (tp_dev->config.flags.mirror_y) {
                    point[i].y = tp_dev->config.y_max - point[i].y;
                }
            }
        }
        tal_mutex_unlock(tp_dev->mutex);
        if (OPRT_OK != rt) {
            PR_ERR("Failed to read tp data: %d", rt);
        }
    }

    return rt;
}

OPERATE_RET tdl_tp_dev_close(TDL_TP_HANDLE_T tp_hdl)
{
    OPERATE_RET rt = OPRT_OK;
    TP_DEVICE_T *tp_dev = NULL;

    if (NULL == tp_hdl) {
        return OPRT_INVALID_PARM;
    }

    tp_dev = (TP_DEVICE_T *)tp_hdl;

    if (false == tp_dev->is_open) {
        return OPRT_OK;
    }

    if (tp_dev->intfs.close) {
        TUYA_CALL_ERR_RETURN(tp_dev->intfs.close(tp_dev->tdd_hdl));
    }

    tp_dev->is_open = false;

    return OPRT_OK;
}

OPERATE_RET tdl_tp_device_register(char *name, TDD_TP_DEV_HANDLE_T tdd_hdl, TDD_TP_CONFIG_T *tp_cfg,
                                      TDD_TP_INTFS_T *intfs)
{
    TP_DEVICE_T *tp_dev = NULL;

    if (NULL == name || NULL == tdd_hdl || NULL == tp_cfg || NULL == intfs) {
        return OPRT_INVALID_PARM;
    }

    NEW_LIST_NODE(TP_DEVICE_T, tp_dev);
    if (NULL == tp_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(tp_dev, 0, sizeof(TP_DEVICE_T));

    strncpy(tp_dev->name, name, TP_DEV_NAME_MAX_LEN);

    tp_dev->tdd_hdl = tdd_hdl;

    memcpy(&tp_dev->config, tp_cfg, sizeof(TDD_TP_CONFIG_T));
    memcpy(&tp_dev->intfs, intfs, sizeof(TDD_TP_INTFS_T));

    tuya_list_add(&tp_dev->node, &sg_tp_list);

    return OPRT_OK;
}