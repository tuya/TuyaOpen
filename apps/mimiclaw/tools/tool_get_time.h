#pragma once

#include "mimi_base.h"

/**
 * @brief Initialize timezone from MIMI_TIMEZONE config. Call once at startup.
 */
void tool_get_time_init(void);

OPERATE_RET tool_get_time_execute(const char *input_json, char *output, size_t output_size);
