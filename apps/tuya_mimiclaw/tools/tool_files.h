#pragma once

#include "mimi_base.h"

OPERATE_RET tool_read_file_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_write_file_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_edit_file_execute(const char *input_json, char *output, size_t output_size);
OPERATE_RET tool_list_dir_execute(const char *input_json, char *output, size_t output_size);
