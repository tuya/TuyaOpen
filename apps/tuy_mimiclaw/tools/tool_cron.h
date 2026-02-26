#pragma once

#include "mimi_base.h"

OPERATE_RET tool_cron_add_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_cron_list_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_cron_remove_execute(const char *input_json, char *output, size_t output_size);
