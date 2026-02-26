#pragma once

#include "mimi_base.h"

OPERATE_RET tool_web_search_init(void);
OPERATE_RET tool_web_search_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_web_search_set_key(const char *api_key);
