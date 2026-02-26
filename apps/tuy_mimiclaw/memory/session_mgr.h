#pragma once

#include "mimi_base.h"

typedef void (*session_list_cb_t)(const char *name, void *user_data);

OPERATE_RET session_mgr_init(void);
OPERATE_RET session_append(const char *chat_id, const char *role, const char *content);
OPERATE_RET session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs);
OPERATE_RET session_clear(const char *chat_id);
OPERATE_RET session_clear_all(uint32_t *out_removed);
OPERATE_RET session_list(session_list_cb_t cb, void *user_data, uint32_t *out_count);
