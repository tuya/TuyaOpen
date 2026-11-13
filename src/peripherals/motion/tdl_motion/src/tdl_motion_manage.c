/**
 * @file tdl_motion_manage.c
 * @brief Tuya Driver Layer motion sensor management implementation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdl_motion_manage.h"
#include "tdl_motion_driver.h"

#include "tal_api.h"

#define TDL_MOTION_NAME_LEN 32U

#ifdef MOTION_MAX_DEVICES
#define TDL_MOTION_MAX_DEVICES MOTION_MAX_DEVICES
#else
#define TDL_MOTION_MAX_DEVICES 4U
#endif

typedef struct {
    bool registered;
    char name[TDL_MOTION_NAME_LEN + 1U];
    void *dev_ctx;
    TDD_MOTION_INTFS_T intfs;
    uint32_t capabilities;
    MUTEX_HANDLE mutex;

    TDL_MOTION_CFG_T cfg;
    TDL_MOTION_EVENT_CB callback;
    void *user_data;
    bool opened;
} TDL_MOTION_NODE_T;

static TDL_MOTION_NODE_T s_motion_nodes[TDL_MOTION_MAX_DEVICES];
static MUTEX_HANDLE s_motion_registry_mutex;
static bool s_motion_initialized;

static OPERATE_RET __tdl_motion_local_init(void)
{
    OPERATE_RET rt;

    if (s_motion_initialized) {
        return OPRT_OK;
    }

    rt = tal_mutex_create_init(&s_motion_registry_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }

    s_motion_initialized = true;
    return OPRT_OK;
}

static bool __tdl_motion_valid_handle(TDL_MOTION_HANDLE handle)
{
    uint32_t index;

    for (index = 0U; index < TDL_MOTION_MAX_DEVICES; index++) {
        if (handle == (TDL_MOTION_HANDLE)&s_motion_nodes[index]) {
            return s_motion_nodes[index].registered;
        }
    }
    return false;
}

static void __tdl_motion_irq_notify(void *arg, TDL_MOTION_EVENT_E event)
{
    TDL_MOTION_NODE_T *node = (TDL_MOTION_NODE_T *)arg;
    TDL_MOTION_EVENT_CB callback;
    void *user_data;

    if (node == NULL) {
        return;
    }

    callback = node->callback;
    user_data = node->user_data;
    if (callback != NULL) {
        callback((TDL_MOTION_HANDLE)node, event, user_data);
    }
}

OPERATE_RET tdl_motion_device_register(char *name, void *dev_ctx,
                                       const TDD_MOTION_INTFS_T *intfs,
                                       uint32_t capabilities)
{
    OPERATE_RET rt;
    uint32_t index;
    uint32_t free_index = TDL_MOTION_MAX_DEVICES;

    if (name == NULL || strlen(name) > TDL_MOTION_NAME_LEN || dev_ctx == NULL ||
        intfs == NULL || intfs->open == NULL || intfs->close == NULL ||
        intfs->read == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __tdl_motion_local_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    tal_mutex_lock(s_motion_registry_mutex);
    for (index = 0U; index < TDL_MOTION_MAX_DEVICES; index++) {
        if (s_motion_nodes[index].registered &&
            strcmp(s_motion_nodes[index].name, name) == 0) {
            tal_mutex_unlock(s_motion_registry_mutex);
            return OPRT_COM_ERROR;
        }
        if (!s_motion_nodes[index].registered && free_index == TDL_MOTION_MAX_DEVICES) {
            free_index = index;
        }
    }
    if (free_index == TDL_MOTION_MAX_DEVICES) {
        tal_mutex_unlock(s_motion_registry_mutex);
        return OPRT_MALLOC_FAILED;
    }

    memset(&s_motion_nodes[free_index], 0, sizeof(TDL_MOTION_NODE_T));
    strncpy(s_motion_nodes[free_index].name, name, TDL_MOTION_NAME_LEN);
    s_motion_nodes[free_index].dev_ctx = dev_ctx;
    s_motion_nodes[free_index].intfs = *intfs;
    s_motion_nodes[free_index].capabilities = capabilities;
    rt = tal_mutex_create_init(&s_motion_nodes[free_index].mutex);
    if (rt == OPRT_OK) {
        s_motion_nodes[free_index].registered = true;
    }
    tal_mutex_unlock(s_motion_registry_mutex);
    return rt;
}

TDL_MOTION_HANDLE tdl_motion_find_dev(char *name)
{
    TDL_MOTION_HANDLE handle = NULL;
    uint32_t index;

    if (name == NULL || __tdl_motion_local_init() != OPRT_OK) {
        return NULL;
    }

    tal_mutex_lock(s_motion_registry_mutex);
    for (index = 0U; index < TDL_MOTION_MAX_DEVICES; index++) {
        if (s_motion_nodes[index].registered &&
            strcmp(s_motion_nodes[index].name, name) == 0) {
            handle = (TDL_MOTION_HANDLE)&s_motion_nodes[index];
            break;
        }
    }
    tal_mutex_unlock(s_motion_registry_mutex);
    return handle;
}

OPERATE_RET tdl_motion_open(TDL_MOTION_HANDLE *handle, TDL_MOTION_OPEN_CFG_T *cfg)
{
    TDL_MOTION_NODE_T *node;
    TDD_MOTION_IRQ_CFG_T irq_cfg;
    OPERATE_RET rt;

    if (handle == NULL || *handle == NULL || cfg == NULL ||
        !__tdl_motion_valid_handle(*handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)(*handle);
    tal_mutex_lock(node->mutex);
    if (node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }

    memset(&irq_cfg, 0, sizeof(irq_cfg));
    irq_cfg.cb = __tdl_motion_irq_notify;
    irq_cfg.arg = node;
    rt = node->intfs.open(node->dev_ctx, cfg,
                          cfg->events_enabled ? &irq_cfg : NULL);
    if (rt == OPRT_OK) {
        node->cfg = cfg->sample_cfg;
        node->opened = true;
    }
    tal_mutex_unlock(node->mutex);
    return rt;
}

OPERATE_RET tdl_motion_close(TDL_MOTION_HANDLE handle)
{
    TDL_MOTION_NODE_T *node;
    OPERATE_RET rt;

    if (!__tdl_motion_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_OK;
    }

    rt = node->intfs.close(node->dev_ctx);
    if (rt == OPRT_OK) {
        node->opened = false;
    }
    tal_mutex_unlock(node->mutex);
    return rt;
}

OPERATE_RET tdl_motion_config(TDL_MOTION_HANDLE handle, TDL_MOTION_CFG_T *cfg)
{
    TDL_MOTION_NODE_T *node;
    OPERATE_RET rt;

    if (cfg == NULL || !__tdl_motion_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }
    if (node->intfs.config == NULL) {
        tal_mutex_unlock(node->mutex);
        return OPRT_NOT_SUPPORTED;
    }

    rt = node->intfs.config(node->dev_ctx, cfg);
    if (rt == OPRT_OK) {
        node->cfg = *cfg;
    }
    tal_mutex_unlock(node->mutex);
    return rt;
}

OPERATE_RET tdl_motion_read(TDL_MOTION_HANDLE handle, TDL_MOTION_DATA_T *data)
{
    TDL_MOTION_NODE_T *node;
    OPERATE_RET rt;

    if (data == NULL || !__tdl_motion_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }

    memset(data, 0, sizeof(*data));
    rt = node->intfs.read(node->dev_ctx, data);
    if (rt == OPRT_OK && data->timestamp_ms == 0U) {
        data->timestamp_ms = (uint32_t)tal_system_get_millisecond();
    }
    tal_mutex_unlock(node->mutex);
    return rt;
}

OPERATE_RET tdl_motion_get_caps(TDL_MOTION_HANDLE handle, uint32_t *caps)
{
    TDL_MOTION_NODE_T *node;

    if (caps == NULL || !__tdl_motion_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)handle;
    *caps = node->capabilities;
    return OPRT_OK;
}

OPERATE_RET tdl_motion_set_callback(TDL_MOTION_HANDLE handle,
                                    TDL_MOTION_EVENT_CB cb,
                                    void *user_data)
{
    TDL_MOTION_NODE_T *node;

    if (!__tdl_motion_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_MOTION_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    node->callback = cb;
    node->user_data = user_data;
    tal_mutex_unlock(node->mutex);
    return OPRT_OK;
}
