#pragma once

#include "mimi_base.h"

#define MIMI_CHAN_TELEGRAM  "telegram"
#define MIMI_CHAN_DISCORD   "discord"
#define MIMI_CHAN_FEISHU    "feishu"
#define MIMI_CHAN_WEBSOCKET "websocket"
#define MIMI_CHAN_CLI       "cli"
#define MIMI_CHAN_SYSTEM    "system"

typedef struct {
    char  channel[16];
    char  chat_id[96];
    char *content;
} mimi_msg_t;

OPERATE_RET message_bus_init(void);
OPERATE_RET message_bus_push_inbound(const mimi_msg_t *msg);
OPERATE_RET message_bus_pop_inbound(mimi_msg_t *msg, uint32_t timeout_ms);
OPERATE_RET message_bus_push_outbound(const mimi_msg_t *msg);
OPERATE_RET message_bus_pop_outbound(mimi_msg_t *msg, uint32_t timeout_ms);
