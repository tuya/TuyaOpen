#include "bus/message_bus.h"
#include "im_config.h"

static const char *TAG = "bus";

static QUEUE_HANDLE s_inbound_queue  = NULL;
static QUEUE_HANDLE s_outbound_queue = NULL;

OPERATE_RET message_bus_init(void)
{
    if (s_inbound_queue && s_outbound_queue) return OPRT_OK;
    OPERATE_RET rt = tal_queue_create_init(&s_inbound_queue, sizeof(im_msg_t), IM_BUS_QUEUE_LEN);
    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "create inbound queue failed: %d", rt);
        return rt;
    }
    rt = tal_queue_create_init(&s_outbound_queue, sizeof(im_msg_t), IM_BUS_QUEUE_LEN);
    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "create outbound queue failed: %d", rt);
        tal_queue_free(s_inbound_queue);
        s_inbound_queue = NULL;
        return rt;
    }
    IM_LOGI(TAG, "message bus initialized, queue=%d", IM_BUS_QUEUE_LEN);
    return OPRT_OK;
}

OPERATE_RET message_bus_push_inbound(const im_msg_t *msg)
{
    if (!s_inbound_queue || !msg) return OPRT_INVALID_PARM;
    return tal_queue_post(s_inbound_queue, (void *)msg, 1000);
}

OPERATE_RET message_bus_pop_inbound(im_msg_t *msg, uint32_t timeout_ms)
{
    if (!s_inbound_queue || !msg) return OPRT_INVALID_PARM;
    return tal_queue_fetch(s_inbound_queue, msg, timeout_ms == UINT32_MAX ? QUEUE_WAIT_FOREVER : timeout_ms);
}

OPERATE_RET message_bus_push_outbound(const im_msg_t *msg)
{
    if (!s_outbound_queue || !msg) return OPRT_INVALID_PARM;
    return tal_queue_post(s_outbound_queue, (void *)msg, 1000);
}

OPERATE_RET message_bus_pop_outbound(im_msg_t *msg, uint32_t timeout_ms)
{
    if (!s_outbound_queue || !msg) return OPRT_INVALID_PARM;
    return tal_queue_fetch(s_outbound_queue, msg, timeout_ms == UINT32_MAX ? QUEUE_WAIT_FOREVER : timeout_ms);
}
