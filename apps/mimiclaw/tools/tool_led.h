#pragma once

#include "mimi_base.h"

OPERATE_RET tool_led_init(void);
OPERATE_RET tool_led_control_execute(const char *input_json, char *output, size_t output_size);
