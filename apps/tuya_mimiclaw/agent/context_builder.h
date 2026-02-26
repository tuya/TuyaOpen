#pragma once

#include "mimi_base.h"

OPERATE_RET context_build_system_prompt(char *buf, size_t size);
OPERATE_RET context_build_messages(const char *history_json, const char *user_message, char *buf, size_t size);
