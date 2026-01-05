/**
 * @file tdl_audio_manage.c
 * @brief Implementation of Tuya Driver Layer audio management system.
 *
 * This file implements the audio management functionality for the Tuya Driver
 * Layer (TDL). It provides a unified interface for managing multiple audio
 * drivers, including driver registration, device discovery, and audio operations.
 * The implementation uses a linked list to maintain registered audio drivers
 * and provides thread-safe access to audio resources.
 *
 * Key features implemented:
 * - Dynamic audio driver registration and management
 * - Linked list-based driver storage for efficient lookup
 * - Audio device discovery by name
 * - Unified interface for audio operations (open, play, volume control, close)
 * - Memory management for driver nodes
 * - Error handling and validation
 *
 * The management layer acts as a bridge between applications and the underlying
 * audio drivers, providing a consistent API regardless of the hardware platform.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tdl_audio_driver.h"
#include "tdl_audio_manage.h"

// #include "tal_api.h"
#include "tal_memory.h"
#include "tal_log.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct list_node {
    struct list_node *next;

    char name[TDL_AUDIO_NAME_LEN_MAX + 1];

    TDD_AUDIO_HANDLE_T tdd_hdl;
    TDD_AUDIO_INTFS_T  tdd_intfs;
    TDD_AUDIO_INFO_T   tdd_info;
} TDL_AUDIO_NODE_T;

typedef struct {
    TDL_AUDIO_NODE_T *head;
    TDL_AUDIO_NODE_T *tail;
} TDL_AUDIO_LIST_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_AUDIO_LIST_T sg_audio_list = {
    .head = NULL,
    .tail = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static TDL_AUDIO_NODE_T *__audio_node_create(void)
{
    TDL_AUDIO_NODE_T *node = NULL;

    node = (TDL_AUDIO_NODE_T *)tal_malloc(sizeof(TDL_AUDIO_NODE_T));
    if (NULL == node) {
        return NULL;
    }

    memset(node, 0, sizeof(TDL_AUDIO_NODE_T));

    return node;
}

static TDL_AUDIO_NODE_T *__audio_node_find(char *name)
{
    TDL_AUDIO_NODE_T *node = sg_audio_list.head;

    while (NULL != node) {
        if (0 == strcmp(node->name, name)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static OPERATE_RET __audio_node_add(TDL_AUDIO_NODE_T *node)
{
    if (NULL == sg_audio_list.head) {
        sg_audio_list.head = node;
        sg_audio_list.tail = node;
    } else {
        sg_audio_list.tail->next = node;
        sg_audio_list.tail = node;
    }

    return OPRT_OK;
}

OPERATE_RET tdl_audio_find(char *name, TDL_AUDIO_HANDLE_T *handle)
{
    TDL_AUDIO_NODE_T *node = NULL;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    node = __audio_node_find(name);
    if (NULL == node) {
        PR_ERR("audio driver %s not exist", name);
        return OPRT_INVALID_PARM;
    }

    *handle = (TDL_AUDIO_HANDLE_T)node;

    return OPRT_OK;
}

OPERATE_RET tdl_audio_get_info(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_INFO_T *info)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;
    uint32_t per_ms_size = 0;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(info, OPRT_INVALID_PARM);

    info->sample_rate   = node->tdd_info.sample_rate;
    info->sample_bits   = node->tdd_info.sample_bits;
    info->sample_ch_num = node->tdd_info.sample_ch_num;
    info->sample_tm_ms  = node->tdd_info.sample_tm_ms;
    
    per_ms_size = node->tdd_info.sample_rate * node->tdd_info.sample_ch_num *\
                  (node->tdd_info.sample_bits / 8) / 1000;
    info->frame_size = node->tdd_info.sample_tm_ms * per_ms_size;

    return OPRT_OK;
}

OPERATE_RET tdl_audio_open(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);

    if (NULL == node->tdd_hdl) {
        PR_ERR("audio driver %s not register", node->name);
        return OPRT_INVALID_PARM;
    }

    if (NULL == node->tdd_intfs.open) {
        PR_ERR("audio driver %s not support open", node->name);
        return OPRT_INVALID_PARM;
    }

    return node->tdd_intfs.open(node->tdd_hdl, mic_cb);
}

OPERATE_RET tdl_audio_play(TDL_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);

    if (NULL == node->tdd_hdl) {
        PR_ERR("audio driver %s not register", node->name);
        return OPRT_INVALID_PARM;
    }

    if (NULL == node->tdd_intfs.play) {
        PR_ERR("audio driver %s not support play", node->name);
        return OPRT_INVALID_PARM;
    }

    return node->tdd_intfs.play(node->tdd_hdl, data, len);
}

OPERATE_RET tdl_audio_play_stop(TDL_AUDIO_HANDLE_T handle)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);

    if (NULL == node->tdd_hdl) {
        PR_ERR("audio driver %s not register", node->name);
        return OPRT_INVALID_PARM;
    }

    if (NULL == node->tdd_intfs.config) {
        PR_ERR("audio driver %s not support config", node->name);
        return OPRT_INVALID_PARM;
    }

    return node->tdd_intfs.config(node->tdd_hdl, TDD_AUDIO_CMD_PLAY_STOP, NULL);
}

OPERATE_RET tdl_audio_volume_set(TDL_AUDIO_HANDLE_T handle, uint8_t volume)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);

    if (NULL == node->tdd_hdl) {
        PR_ERR("audio driver %s not register", node->name);
        return OPRT_INVALID_PARM;
    }

    if (NULL == node->tdd_intfs.config) {
        PR_ERR("audio driver %s not support config", node->name);
        return OPRT_INVALID_PARM;
    }

    return node->tdd_intfs.config(node->tdd_hdl, TDD_AUDIO_CMD_SET_VOLUME, &volume);
}

OPERATE_RET tdl_audio_close(TDL_AUDIO_HANDLE_T handle)
{
    TDL_AUDIO_NODE_T *node = (TDL_AUDIO_NODE_T *)handle;

    TUYA_CHECK_NULL_RETURN(node, OPRT_INVALID_PARM);

    if (NULL == node->tdd_hdl) {
        PR_ERR("audio driver %s not register", node->name);
        return OPRT_INVALID_PARM;
    }

    if (NULL == node->tdd_intfs.close) {
        PR_ERR("audio driver %s not support close", node->name);
        return OPRT_INVALID_PARM;
    }

    return node->tdd_intfs.close(node->tdd_hdl);
}

OPERATE_RET tdl_audio_driver_register(char *name, TDD_AUDIO_HANDLE_T tdd_hdl,\
                                      TDD_AUDIO_INTFS_T *intfs, TDD_AUDIO_INFO_T *info)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(tdd_hdl, OPRT_INVALID_PARM);

    TDL_AUDIO_NODE_T *node = __audio_node_find(name);
    if (NULL != node) {
        PR_ERR("audio driver %s already exist", name);
        return OPRT_INVALID_PARM;
    }

    node = __audio_node_create();
    TUYA_CHECK_NULL_RETURN(node, OPRT_MALLOC_FAILED);

    node->tdd_hdl = tdd_hdl;
    strncpy(node->name, name, TDL_AUDIO_NAME_LEN_MAX);
    memcpy(&node->tdd_intfs, intfs, sizeof(TDD_AUDIO_INTFS_T));
    memcpy(&node->tdd_info,  info,  sizeof(TDD_AUDIO_INFO_T));

    __audio_node_add(node);

    return rt;
}
