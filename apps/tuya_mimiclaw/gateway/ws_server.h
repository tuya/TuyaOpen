#pragma once

#include "mimi_base.h"

OPERATE_RET ws_server_start(void);
OPERATE_RET ws_server_send(const char *chat_id, const char *text);
OPERATE_RET ws_server_stop(void);
