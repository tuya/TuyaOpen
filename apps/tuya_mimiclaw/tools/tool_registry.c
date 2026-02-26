#include "tool_registry.h"

#include "tools/tool_files.h"
#include "tools/tool_cron.h"
#include "tools/tool_get_time.h"
#include "tools/tool_web_search.h"

#include "cJSON.h"

static const char *TAG = "tools";

#define MAX_TOOLS 12

static mimi_tool_t s_tools[MAX_TOOLS];
static int         s_tool_count = 0;
static char       *s_tools_json = NULL;

static void register_tool(const mimi_tool_t *tool)
{
    if (!tool || s_tool_count >= MAX_TOOLS) {
        return;
    }

    s_tools[s_tool_count++] = *tool;
    MIMI_LOGI(TAG, "register tool: %s", tool->name);
}

static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return;
    }

    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);

        cJSON *schema = cJSON_Parse(s_tools[i].input_schema_json);
        if (!schema) {
            schema = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(tool, "input_schema", schema);

        cJSON_AddItemToArray(arr, tool);
    }

    if (s_tools_json) {
        cJSON_free(s_tools_json);
        s_tools_json = NULL;
    }
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
}

OPERATE_RET tool_registry_init(void)
{
    s_tool_count = 0;

    (void)tool_web_search_init();

    register_tool(&(mimi_tool_t){
        .name        = "web_search",
        .description = "Search the web for current information",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}",
        .execute = tool_web_search_execute,
    });

    register_tool(&(mimi_tool_t){
        .name              = "get_current_time",
        .description       = "Get the current date and time",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{}}",
        .execute           = tool_get_time_execute,
    });

    register_tool(&(mimi_tool_t){
        .name        = "read_file",
        .description = "Read a file from /spiffs/",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",
        .execute = tool_read_file_execute,
    });

    register_tool(&(mimi_tool_t){
        .name              = "write_file",
        .description       = "Write file content",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{"
                             "\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
        .execute           = tool_write_file_execute,
    });

    register_tool(&(mimi_tool_t){
        .name        = "edit_file",
        .description = "Replace old string with new string",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"old_string\":{\"type\":\"string\"},"
            "\"new_string\":{\"type\":\"string\"}},\"required\":[\"path\",\"old_string\",\"new_string\"]}",
        .execute = tool_edit_file_execute,
    });

    register_tool(&(mimi_tool_t){
        .name              = "list_dir",
        .description       = "List files in /spiffs/",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"prefix\":{\"type\":\"string\"}}}",
        .execute           = tool_list_dir_execute,
    });

    register_tool(&(mimi_tool_t){
        .name = "cron_add",
        .description =
            "Schedule a recurring or one-shot task. The message will trigger an agent turn when the job fires.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{"
            "\"name\":{\"type\":\"string\",\"description\":\"Short name for the job\"},"
            "\"schedule_type\":{\"type\":\"string\",\"description\":\"'every' for recurring interval or 'at' for "
            "one-shot at a unix timestamp\"},"
            "\"interval_s\":{\"type\":\"integer\",\"description\":\"Interval in seconds (required for 'every')\"},"
            "\"at_epoch\":{\"type\":\"integer\",\"description\":\"Unix timestamp to fire at (required for 'at')\"},"
            "\"message\":{\"type\":\"string\",\"description\":\"Message to inject when the job fires, triggering an "
            "agent turn\"},"
            "\"channel\":{\"type\":\"string\",\"description\":\"Optional reply channel (e.g. 'telegram'). If omitted, "
            "current turn channel is used when available\"},"
            "\"chat_id\":{\"type\":\"string\",\"description\":\"Optional reply chat_id. Required when "
            "channel='telegram'. If omitted during a Telegram turn, current chat_id is used\"}"
            "},"
            "\"required\":[\"name\",\"schedule_type\",\"message\"]}",
        .execute = tool_cron_add_execute,
    });

    register_tool(&(mimi_tool_t){
        .name              = "cron_list",
        .description       = "List all scheduled cron jobs with their status, schedule, and IDs.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute           = tool_cron_list_execute,
    });

    register_tool(&(mimi_tool_t){
        .name              = "cron_remove",
        .description       = "Remove a scheduled cron job by its ID.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"job_id\":{\"type\":\"string\",\"description\":"
                             "\"The 8-character job ID to remove\"}},\"required\":[\"job_id\"]}",
        .execute           = tool_cron_remove_execute,
    });

    build_tools_json();
    return OPRT_OK;
}

const char *tool_registry_get_tools_json(void)
{
    return s_tools_json;
}

OPERATE_RET tool_registry_execute(const char *name, const char *input_json, char *output, size_t output_size)
{
    if (!name || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(name, s_tools[i].name) == 0) {
            return s_tools[i].execute(input_json ? input_json : "{}", output, output_size);
        }
    }

    snprintf(output, output_size, "Error: unknown tool: %s", name);
    return OPRT_NOT_FOUND;
}
