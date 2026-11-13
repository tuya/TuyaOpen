/**
 * @file tdl_encoder_manage.c
 * @brief Tuya Driver Layer encoder management implementation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdl_encoder_manage.h"
#include "tdl_encoder_driver.h"

#include "tal_api.h"

#define TDL_ENCODER_NAME_LEN               32U
#define TDL_ENCODER_EVENT_QUEUE_LEN        8U
#define TDL_ENCODER_POLL_INTERVAL_MS       1U

#ifdef ENCODER_MAX_DEVICES
#define TDL_ENCODER_MAX_DEVICES            ENCODER_MAX_DEVICES
#else
#define TDL_ENCODER_MAX_DEVICES            4U
#endif

#ifndef ENCODER_TASK_STACK_SIZE
#define ENCODER_TASK_STACK_SIZE            2048U
#endif

typedef struct {
    bool registered;
    char name[TDL_ENCODER_NAME_LEN + 1U];
    void *dev_ctx;
    TDD_ENCODER_INTFS_T intfs;
    MUTEX_HANDLE mutex;

    TDL_ENCODER_HW_CFG_T hw_cfg;
    TDL_ENCODER_CFG_T cfg;
    TDL_ENCODER_CB callback;
    void *user_data;
    bool opened;

    int64_t step_accum;
    int32_t position;
    int32_t unread_delta;

    uint8_t button_raw;
    uint8_t button_stable;
    uint32_t raw_change_ms;
    uint32_t press_start_ms;
    uint32_t click_start_ms;
    uint32_t long_hold_start_ms;
    uint8_t click_count;
    bool long_started;
} TDL_ENCODER_NODE_T;

static TDL_ENCODER_NODE_T s_encoder_nodes[TDL_ENCODER_MAX_DEVICES];
static MUTEX_HANDLE s_encoder_registry_mutex;
static THREAD_HANDLE s_encoder_thread;
static bool s_encoder_initialized;

static OPERATE_RET __tdl_encoder_local_init(void)
{
    OPERATE_RET rt;

    if (s_encoder_initialized) {
        return OPRT_OK;
    }

    rt = tal_mutex_create_init(&s_encoder_registry_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    s_encoder_initialized = true;
    return OPRT_OK;
}

static bool __tdl_encoder_valid_handle(TDL_ENCODER_HANDLE handle)
{
    uint32_t index;

    for (index = 0U; index < TDL_ENCODER_MAX_DEVICES; index++) {
        if (handle == (TDL_ENCODER_HANDLE)&s_encoder_nodes[index]) {
            return s_encoder_nodes[index].registered;
        }
    }
    return false;
}

static void __tdl_encoder_queue_event(TDL_ENCODER_EVENT_T *events, uint8_t *count,
                                      TDL_ENCODER_EVENT_TYPE_E type,
                                      int32_t position, int32_t delta,
                                      uint32_t timestamp_ms)
{
    if (*count >= TDL_ENCODER_EVENT_QUEUE_LEN) {
        return;
    }

    events[*count].type = type;
    events[*count].position = position;
    events[*count].delta = delta;
    events[*count].timestamp_ms = timestamp_ms;
    (*count)++;
}

static int32_t __tdl_encoder_saturating_add(int32_t value, int32_t delta)
{
    if (delta > 0 && value > INT32_MAX - delta) {
        return INT32_MAX;
    }
    if (delta < 0 && value < INT32_MIN - delta) {
        return INT32_MIN;
    }
    return value + delta;
}

static void __tdl_encoder_process_rotation(TDL_ENCODER_NODE_T *node,
                                           int32_t raw_delta,
                                           TDL_ENCODER_EVENT_T *events,
                                           uint8_t *event_count)
{
    int32_t delta;
    int32_t previous_position;
    int32_t applied_delta;

    if (node->hw_cfg.reverse) {
        raw_delta = -raw_delta;
    }

    node->step_accum += raw_delta;
    delta = node->step_accum / node->hw_cfg.steps_per_detent;
    node->step_accum %= node->hw_cfg.steps_per_detent;
    if (delta == 0) {
        return;
    }

    previous_position = node->position;
    node->position = __tdl_encoder_saturating_add(node->position, delta);
    applied_delta = node->position - previous_position;
    if (applied_delta == 0) {
        return;
    }
    node->unread_delta = __tdl_encoder_saturating_add(node->unread_delta,
                                                       applied_delta);
    __tdl_encoder_queue_event(events, event_count, TDL_ENCODER_EVENT_ROTATE,
                              node->position, applied_delta,
                              tal_system_get_millisecond());
}

static void __tdl_encoder_process_button(TDL_ENCODER_NODE_T *node,
                                         uint8_t pressed, uint32_t now,
                                         TDL_ENCODER_EVENT_T *events,
                                         uint8_t *event_count)
{
    if (pressed != node->button_raw) {
        node->button_raw = pressed;
        node->raw_change_ms = now;
    }

    if (node->button_raw != node->button_stable &&
        (node->hw_cfg.debounce_ms == 0U ||
         now - node->raw_change_ms >= node->hw_cfg.debounce_ms)) {
        node->button_stable = node->button_raw;

        if (node->button_stable) {
            node->press_start_ms = now;
            node->long_hold_start_ms = now;
            node->long_started = false;
            __tdl_encoder_queue_event(events, event_count,
                                      TDL_ENCODER_EVENT_BUTTON_DOWN,
                                      node->position, 0, now);
        } else {
            __tdl_encoder_queue_event(events, event_count,
                                      TDL_ENCODER_EVENT_BUTTON_UP,
                                      node->position, 0, now);
            if (!node->long_started) {
                node->click_count++;
                node->click_start_ms = now;
                if (node->click_count >= 2U) {
                    __tdl_encoder_queue_event(events, event_count,
                                              TDL_ENCODER_EVENT_BUTTON_DOUBLE_CLICK,
                                              node->position, 0, now);
                    node->click_count = 0U;
                } else if (node->cfg.click_ms == 0U) {
                    __tdl_encoder_queue_event(events, event_count,
                                              TDL_ENCODER_EVENT_BUTTON_CLICK,
                                              node->position, 0, now);
                    node->click_count = 0U;
                }
            } else {
                node->click_count = 0U;
            }
        }
    }

    if (!node->button_stable) {
        if (node->click_count == 1U &&
            now - node->click_start_ms >= node->cfg.click_ms) {
            __tdl_encoder_queue_event(events, event_count,
                                      TDL_ENCODER_EVENT_BUTTON_CLICK,
                                      node->position, 0, now);
            node->click_count = 0U;
        }
        return;
    }

    if (!node->long_started && node->cfg.long_press_ms != 0U &&
        now - node->press_start_ms >= node->cfg.long_press_ms) {
        node->long_started = true;
        node->long_hold_start_ms = now;
        node->click_count = 0U;
        __tdl_encoder_queue_event(events, event_count,
                                  TDL_ENCODER_EVENT_BUTTON_LONG_START,
                                  node->position, 0, now);
        return;
    }

    if (node->long_started && node->cfg.long_hold_ms != 0U &&
        now - node->long_hold_start_ms >= node->cfg.long_hold_ms) {
        node->long_hold_start_ms = now;
        __tdl_encoder_queue_event(events, event_count,
                                  TDL_ENCODER_EVENT_BUTTON_LONG_HOLD,
                                  node->position, 0, now);
    }
}

static uint8_t __tdl_encoder_process_node(TDL_ENCODER_NODE_T *node,
                                          TDL_ENCODER_EVENT_T *events)
{
    TDD_ENCODER_DATA_T data;
    TDL_ENCODER_CB callback;
    void *user_data;
    uint8_t event_count = 0U;
    uint32_t now;

    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return 0U;
    }
    if (node->intfs.read(node->dev_ctx, &data) != OPRT_OK) {
        tal_mutex_unlock(node->mutex);
        return 0U;
    }

    now = tal_system_get_millisecond();
    __tdl_encoder_process_rotation(node, data.delta, events, &event_count);
    __tdl_encoder_process_button(node, data.button_pressed, now, events, &event_count);

    callback = node->callback;
    user_data = node->user_data;
    tal_mutex_unlock(node->mutex);

    if (callback != NULL) {
        uint8_t index;
        for (index = 0U; index < event_count; index++) {
            callback((TDL_ENCODER_HANDLE)node, &events[index], user_data);
        }
    }

    return event_count;
}

static void __tdl_encoder_task(void *arg)
{
    TDL_ENCODER_EVENT_T events[TDL_ENCODER_EVENT_QUEUE_LEN];
    uint32_t index;

    (void)arg;
    while (1) {
        for (index = 0U; index < TDL_ENCODER_MAX_DEVICES; index++) {
            if (s_encoder_nodes[index].registered) {
                (void)__tdl_encoder_process_node(&s_encoder_nodes[index], events);
            }
        }
        tal_system_sleep(TDL_ENCODER_POLL_INTERVAL_MS);
    }
}

static OPERATE_RET __tdl_encoder_task_start(void)
{
    THREAD_CFG_T thread_cfg = {
        .stackDepth = ENCODER_TASK_STACK_SIZE,
        .priority = THREAD_PRIO_2,
        .thrdname = "encoder",
        .psram_mode = 0,
    };

    if (s_encoder_thread != NULL) {
        return OPRT_OK;
    }
    return tal_thread_create_and_start(&s_encoder_thread, NULL, NULL,
                                       __tdl_encoder_task, NULL, &thread_cfg);
}

TDL_ENCODER_HANDLE tdl_encoder_find_dev(char *name)
{
    uint32_t index;
    TDL_ENCODER_HANDLE handle = NULL;

    if (name == NULL || __tdl_encoder_local_init() != OPRT_OK) {
        return NULL;
    }

    tal_mutex_lock(s_encoder_registry_mutex);
    for (index = 0U; index < TDL_ENCODER_MAX_DEVICES; index++) {
        if (s_encoder_nodes[index].registered &&
            strcmp(s_encoder_nodes[index].name, name) == 0) {
            handle = (TDL_ENCODER_HANDLE)&s_encoder_nodes[index];
            break;
        }
    }
    tal_mutex_unlock(s_encoder_registry_mutex);
    return handle;
}

OPERATE_RET tdl_encoder_device_register(char *name, void *dev_ctx,
                                        const TDL_ENCODER_HW_CFG_T *hw_cfg,
                                        const TDD_ENCODER_INTFS_T *intfs)
{
    OPERATE_RET rt;
    uint32_t index;
    uint32_t free_index = TDL_ENCODER_MAX_DEVICES;

    if (name == NULL || strlen(name) > TDL_ENCODER_NAME_LEN || dev_ctx == NULL ||
        hw_cfg == NULL || hw_cfg->steps_per_detent == 0U ||
        hw_cfg->steps_per_detent > 64U || intfs == NULL ||
        intfs->open == NULL || intfs->close == NULL || intfs->read == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __tdl_encoder_local_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    tal_mutex_lock(s_encoder_registry_mutex);
    for (index = 0U; index < TDL_ENCODER_MAX_DEVICES; index++) {
        if (s_encoder_nodes[index].registered &&
            strcmp(s_encoder_nodes[index].name, name) == 0) {
            tal_mutex_unlock(s_encoder_registry_mutex);
            return OPRT_COM_ERROR;
        }
        if (!s_encoder_nodes[index].registered && free_index == TDL_ENCODER_MAX_DEVICES) {
            free_index = index;
        }
    }
    if (free_index == TDL_ENCODER_MAX_DEVICES) {
        tal_mutex_unlock(s_encoder_registry_mutex);
        return OPRT_MALLOC_FAILED;
    }

    memset(&s_encoder_nodes[free_index], 0, sizeof(TDL_ENCODER_NODE_T));
    strncpy(s_encoder_nodes[free_index].name, name, TDL_ENCODER_NAME_LEN);
    s_encoder_nodes[free_index].dev_ctx = dev_ctx;
    s_encoder_nodes[free_index].hw_cfg = *hw_cfg;
    s_encoder_nodes[free_index].intfs = *intfs;
    rt = tal_mutex_create_init(&s_encoder_nodes[free_index].mutex);
    if (rt == OPRT_OK) {
        s_encoder_nodes[free_index].registered = true;
    }
    tal_mutex_unlock(s_encoder_registry_mutex);
    return rt;
}

OPERATE_RET tdl_encoder_open(TDL_ENCODER_HANDLE *handle, TDL_ENCODER_CFG_T *cfg)
{
    TDL_ENCODER_NODE_T *node;
    TDD_ENCODER_DATA_T initial_data;
    OPERATE_RET rt;

    if (handle == NULL || *handle == NULL || cfg == NULL ||
        !__tdl_encoder_valid_handle(*handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)(*handle);
    tal_mutex_lock(node->mutex);
    if (node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }

    node->cfg = *cfg;
    node->step_accum = 0;
    node->position = 0;
    node->unread_delta = 0;
    node->click_count = 0U;
    node->long_started = false;

    rt = node->intfs.open(node->dev_ctx, NULL);
    if (rt != OPRT_OK) {
        tal_mutex_unlock(node->mutex);
        return rt;
    }

    rt = node->intfs.read(node->dev_ctx, &initial_data);
    if (rt != OPRT_OK) {
        (void)node->intfs.close(node->dev_ctx);
        tal_mutex_unlock(node->mutex);
        return rt;
    }

    node->button_raw = initial_data.button_pressed;
    node->button_stable = initial_data.button_pressed;
    node->raw_change_ms = tal_system_get_millisecond();
    node->opened = true;
    tal_mutex_unlock(node->mutex);

    rt = __tdl_encoder_task_start();
    if (rt != OPRT_OK) {
        tal_mutex_lock(node->mutex);
        node->opened = false;
        (void)node->intfs.close(node->dev_ctx);
        tal_mutex_unlock(node->mutex);
        return rt;
    }

    return OPRT_OK;
}

OPERATE_RET tdl_encoder_close(TDL_ENCODER_HANDLE handle)
{
    TDL_ENCODER_NODE_T *node;
    OPERATE_RET rt;

    if (!__tdl_encoder_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_OK;
    }

    rt = node->intfs.close(node->dev_ctx);
    if (rt == OPRT_OK) {
        node->opened = false;
        node->step_accum = 0;
        node->unread_delta = 0;
        node->click_count = 0U;
        node->long_started = false;
    }
    tal_mutex_unlock(node->mutex);
    return rt;
}

OPERATE_RET tdl_encoder_set_callback(TDL_ENCODER_HANDLE handle,
                                     TDL_ENCODER_CB cb, void *user_data)
{
    TDL_ENCODER_NODE_T *node;

    if (!__tdl_encoder_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    node->callback = cb;
    node->user_data = user_data;
    tal_mutex_unlock(node->mutex);
    return OPRT_OK;
}

OPERATE_RET tdl_encoder_read_position(TDL_ENCODER_HANDLE handle, int32_t *position)
{
    TDL_ENCODER_NODE_T *node;

    if (position == NULL || !__tdl_encoder_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }
    *position = node->position;
    tal_mutex_unlock(node->mutex);
    return OPRT_OK;
}

OPERATE_RET tdl_encoder_read_delta(TDL_ENCODER_HANDLE handle, int32_t *delta)
{
    TDL_ENCODER_NODE_T *node;

    if (delta == NULL || !__tdl_encoder_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }
    *delta = node->unread_delta;
    node->unread_delta = 0;
    tal_mutex_unlock(node->mutex);
    return OPRT_OK;
}

OPERATE_RET tdl_encoder_read_button(TDL_ENCODER_HANDLE handle, uint8_t *pressed)
{
    TDL_ENCODER_NODE_T *node;

    if (pressed == NULL || !__tdl_encoder_valid_handle(handle)) {
        return OPRT_INVALID_PARM;
    }

    node = (TDL_ENCODER_NODE_T *)handle;
    tal_mutex_lock(node->mutex);
    if (!node->opened) {
        tal_mutex_unlock(node->mutex);
        return OPRT_COM_ERROR;
    }
    *pressed = node->button_stable;
    tal_mutex_unlock(node->mutex);
    return OPRT_OK;
}
