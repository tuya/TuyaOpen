#include "context_builder.h"
#include "memory/memory_store.h"
#include "mimi_config.h"
#include "skills/skill_loader.h"

#include "cJSON.h"
#include "tal_fs.h"

static const char *TAG = "context";
/* Reused scratch buffer for memory/skills blocks to keep stack usage low. */
#define CONTEXT_TMP_BUF_SIZE 4096

static size_t append_file(char *buf, size_t size, size_t offset, const char *path, const char *header)
{
    TUYA_FILE f = tal_fopen(path, "r");
    if (!f || !buf || size == 0 || offset >= size - 1) {
        if (f) {
            tal_fclose(f);
        }
        return offset;
    }

    if (header) {
        offset += snprintf(buf + offset, size - offset, "\n## %s\n\n", header);
        if (offset >= size - 1) {
            tal_fclose(f);
            return size - 1;
        }
    }

    int n = tal_fread(buf + offset, (int)(size - offset - 1), f);
    if (n > 0) {
        offset += (size_t)n;
    }
    buf[offset] = '\0';
    tal_fclose(f);
    return offset;
}

OPERATE_RET context_build_system_prompt(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return OPRT_INVALID_PARM;
    }

    size_t off = 0;
    off += snprintf(
        buf + off, size - off,
        "# MimiClaw\n\n"
        "You are MimiClaw, a personal AI assistant running on a TuyaOpen device.\n"
        "You communicate through Telegram, Discord, and WebSocket.\n\n"
        "Be helpful, accurate, and concise.\n\n"
        "## Available Tools\n"
        "You have access to the following tools:\n"
        "- web_search: Search the web for current information. "
        "Use this when you need up-to-date facts, news, weather, or anything beyond your training data.\n"
        "- get_current_time: Get the current date and time. "
        "You do NOT have an internal clock - always use this tool when you need to know the time or date.\n"
        "- read_file: Read a file from SPIFFS (path must start with /spiffs/).\n"
        "- write_file: Write/overwrite a file on SPIFFS.\n"
        "- edit_file: Find-and-replace edit a file on SPIFFS.\n"
        "- list_dir: List files on SPIFFS, optionally filter by prefix.\n"
        "- cron_add: Schedule a recurring or one-shot task. The message will trigger an agent turn when the job "
        "fires.\n"
        "- cron_list: List all scheduled cron jobs.\n"
        "- cron_remove: Remove a scheduled cron job by ID.\n\n"
        "When using cron_add for Telegram delivery, always set channel='telegram' and a valid numeric chat_id.\n\n"
        "Use tools when needed. Provide your final answer as text after using tools.\n\n"
        "## Memory\n"
        "You have persistent memory stored on local flash:\n"
        "- Long-term memory: /spiffs/memory/MEMORY.md\n"
        "- Daily notes: /spiffs/memory/daily/<YYYY-MM-DD>.md\n\n"
        "IMPORTANT: Actively use memory to remember things across conversations.\n"
        "- When you learn something new about the user (name, preferences, habits, context), write it to MEMORY.md.\n"
        "- When something noteworthy happens in a conversation, append it to today's daily note.\n"
        "- Always read_file MEMORY.md before writing, so you can edit_file to update without losing existing content.\n"
        "- Use get_current_time to know today's date before writing daily notes.\n"
        "- Keep MEMORY.md concise and organized - summarize, do not dump raw conversation.\n"
        "- You should proactively save memory without being asked. If the user tells you their name, preferences, "
        "or important facts, persist them immediately.\n\n"
        "## Skills\n"
        "Skills are specialized instruction files stored in /spiffs/skills/.\n"
        "When a task matches a skill, read the full skill file for detailed instructions.\n"
        "You can create new skills using write_file to /spiffs/skills/<name>.md.\n");

    off = append_file(buf, size, off, MIMI_SOUL_FILE, "Personality");
    off = append_file(buf, size, off, MIMI_USER_FILE, "User Info");

    char *tmp_buf = tal_malloc(CONTEXT_TMP_BUF_SIZE);
    if (!tmp_buf) {
        return OPRT_MALLOC_FAILED;
    }

    memset(tmp_buf, 0, CONTEXT_TMP_BUF_SIZE);
    if (memory_read_long_term(tmp_buf, CONTEXT_TMP_BUF_SIZE) == OPRT_OK && tmp_buf[0]) {
        off += snprintf(buf + off, size - off, "\n## Long-term Memory\n\n%s\n", tmp_buf);
    }

    memset(tmp_buf, 0, CONTEXT_TMP_BUF_SIZE);
    if (memory_read_recent(tmp_buf, CONTEXT_TMP_BUF_SIZE, 3) == OPRT_OK && tmp_buf[0]) {
        off += snprintf(buf + off, size - off, "\n## Recent Notes\n\n%s\n", tmp_buf);
    }

    memset(tmp_buf, 0, CONTEXT_TMP_BUF_SIZE);
    size_t skills_len = skill_loader_build_summary(tmp_buf, CONTEXT_TMP_BUF_SIZE);
    if (skills_len > 0) {
        off += snprintf(buf + off, size - off,
                        "\n## Available Skills\n\n"
                        "Available skills (use read_file to load full instructions):\n%s\n",
                        tmp_buf);
    }

    tal_free(tmp_buf);

    MIMI_LOGI(TAG, "system prompt bytes=%u", (unsigned)off);
    return OPRT_OK;
}

OPERATE_RET context_build_messages(const char *history_json, const char *user_message, char *buf, size_t size)
{
    if (!user_message || !buf || size == 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *history = NULL;
    if (history_json && history_json[0]) {
        history = cJSON_Parse(history_json);
    }
    if (!history || !cJSON_IsArray(history)) {
        cJSON_Delete(history);
        history = cJSON_CreateArray();
    }

    cJSON *user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "role", "user");
    cJSON_AddStringToObject(user, "content", user_message);
    cJSON_AddItemToArray(history, user);

    char *json = cJSON_PrintUnformatted(history);
    cJSON_Delete(history);

    if (!json) {
        return OPRT_CR_CJSON_ERR;
    }

    strncpy(buf, json, size - 1);
    buf[size - 1] = '\0';
    cJSON_free(json);
    return OPRT_OK;
}
