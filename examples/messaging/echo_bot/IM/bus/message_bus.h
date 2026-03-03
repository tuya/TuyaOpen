#ifndef __MESSAGE_BUS_H__
#define __MESSAGE_BUS_H__

#include "im_platform.h"

#define IM_CHAN_TELEGRAM  "telegram"
#define IM_CHAN_DISCORD   "discord"
#define IM_CHAN_FEISHU    "feishu"
typedef struct {
    char  channel[16];
    char  chat_id[96];
    char *content;
} im_msg_t;

OPERATE_RET message_bus_init(void);
OPERATE_RET message_bus_push_inbound(const im_msg_t *msg);
OPERATE_RET message_bus_pop_inbound(im_msg_t *msg, uint32_t timeout_ms);
OPERATE_RET message_bus_push_outbound(const im_msg_t *msg);
OPERATE_RET message_bus_pop_outbound(im_msg_t *msg, uint32_t timeout_ms);

#endif /* __MESSAGE_BUS_H__ */
