#pragma once

#include "mimi_base.h"

typedef struct {
    const char *name;
    const char *description;
    const char *input_schema_json;
    OPERATE_RET (*execute)(const char *input_json, char *output, size_t output_size);
} mimi_tool_t;

OPERATE_RET tool_registry_init(void);
const char *tool_registry_get_tools_json(void);
OPERATE_RET tool_registry_execute(const char *name, const char *input_json, char *output, size_t output_size);
