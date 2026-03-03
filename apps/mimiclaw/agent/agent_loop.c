#include "agent_loop.h"

#include "agent/context_builder.h"
#include "bus/message_bus.h"
#include "llm/llm_proxy.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "tools/tool_registry.h"

#include "cJSON.h"

static const char   *TAG            = "agent";
static THREAD_HANDLE s_agent_thread = NULL;

#define TOOL_OUTPUT_SIZE (8 * 1024)

static uint32_t bounded_random_index(uint32_t count)
{
    if (count == 0) {
        return 0;
    }

    return (uint32_t)tal_system_get_random(0xFFFFFFFFu) % count;
}

static cJSON *build_assistant_content(const llm_response_t *resp)
{
    cJSON *content = cJSON_CreateArray();
    if (!content || !resp) {
        return content;
    }

    if (resp->text && resp->text_len > 0) {
        cJSON *text_block = cJSON_CreateObject();
        if (text_block) {
            cJSON_AddStringToObject(text_block, "type", "text");
            cJSON_AddStringToObject(text_block, "text", resp->text);
            cJSON_AddItemToArray(content, text_block);
        }
    }

    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call       = &resp->calls[i];
        cJSON                 *tool_block = cJSON_CreateObject();
        if (!tool_block) {
            continue;
        }

        cJSON_AddStringToObject(tool_block, "type", "tool_use");
        cJSON_AddStringToObject(tool_block, "id", call->id);
        cJSON_AddStringToObject(tool_block, "name", call->name);

        cJSON *input = cJSON_Parse(call->input);
        if (!input) {
            input = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(tool_block, "input", input);
        cJSON_AddItemToArray(content, tool_block);
    }

    return content;
}

static void json_set_string(cJSON *obj, const char *key, const char *value)
{
    if (!obj || !key || !value) {
        return;
    }

    cJSON_DeleteItemFromObject(obj, key);
    cJSON_AddStringToObject(obj, key, value);
}

static void append_turn_context_prompt(char *prompt, size_t size, const mimi_msg_t *msg)
{
    if (!prompt || size == 0 || !msg) {
        return;
    }

    size_t off = strnlen(prompt, size - 1);
    if (off >= size - 1) {
        return;
    }

    int n = snprintf(
        prompt + off, size - off,
        "\n## Current Turn Context\n"
        "- source_channel: %s\n"
        "- source_chat_id: %s\n"
        "- If using cron_add for Telegram in this turn, set channel='telegram' and chat_id to source_chat_id.\n"
        "- Never use chat_id 'cron' for Telegram messages.\n",
        msg->channel[0] ? msg->channel : "(unknown)", msg->chat_id[0] ? msg->chat_id : "(empty)");
    if (n < 0 || (size_t)n >= (size - off)) {
        prompt[size - 1] = '\0';
    }
}

static char *patch_tool_input_with_context(const llm_tool_call_t *call, const mimi_msg_t *msg)
{
    if (!call || !msg || strcmp(call->name, "cron_add") != 0) {
        return NULL;
    }

    cJSON *root = cJSON_Parse(call->input ? call->input : "{}");
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        root = cJSON_CreateObject();
    }
    if (!root) {
        return NULL;
    }

    bool        changed      = false;
    cJSON      *channel_item = cJSON_GetObjectItem(root, "channel");
    const char *channel      = cJSON_IsString(channel_item) ? channel_item->valuestring : NULL;

    if ((!channel || channel[0] == '\0') && msg->channel[0] != '\0') {
        json_set_string(root, "channel", msg->channel);
        channel = msg->channel;
        changed = true;
    }

    /* Patch chat_id for Telegram and Feishu if AI used invalid 'cron' placeholder */
    bool same_channel = channel && msg->channel[0] != '\0' && strcmp(channel, msg->channel) == 0;
    bool needs_patch =
        same_channel && (strcmp(channel, MIMI_CHAN_TELEGRAM) == 0 || strcmp(channel, MIMI_CHAN_FEISHU) == 0);
    if (needs_patch && msg->chat_id[0] != '\0') {
        cJSON      *chat_item = cJSON_GetObjectItem(root, "chat_id");
        const char *chat_id   = cJSON_IsString(chat_item) ? chat_item->valuestring : NULL;
        if (!chat_id || chat_id[0] == '\0' || strcmp(chat_id, "cron") == 0) {
            json_set_string(root, "chat_id", msg->chat_id);
            changed = true;
        }
    }

    char *patched = NULL;
    if (changed) {
        patched = cJSON_PrintUnformatted(root);
        if (patched) {
            MIMI_LOGI(TAG, "patched cron_add target to %s:%s", msg->channel, msg->chat_id);
        }
    }

    cJSON_Delete(root);
    return patched;
}

static cJSON *build_tool_results(const llm_response_t *resp, const mimi_msg_t *msg, char *tool_output,
                                 size_t tool_output_size)
{
    cJSON *content = cJSON_CreateArray();
    if (!content || !resp || !tool_output || tool_output_size == 0) {
        return content;
    }

    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call          = &resp->calls[i];
        const char            *tool_input    = call->input ? call->input : "{}";
        char                  *patched_input = patch_tool_input_with_context(call, msg);
        if (patched_input) {
            tool_input = patched_input;
        }

        tool_output[0] = '\0';

        MIMI_LOGI(TAG, ">>> TOOL EXEC: name=%s id=%s input=%s", call->name, call->id,
                  tool_input ? tool_input : "(null)");
        OPERATE_RET rt = tool_registry_execute(call->name, tool_input, tool_output, tool_output_size);
        if (rt != OPRT_OK && tool_output[0] == '\0') {
            snprintf(tool_output, tool_output_size, "Tool execute failed: %d", rt);
        }
        cJSON_free(patched_input);

        MIMI_LOGI(TAG, "<<< TOOL RESULT: name=%s rt=%d result_bytes=%u", call->name, rt, (unsigned)strlen(tool_output));
        if (strlen(tool_output) <= 512) {
            MIMI_LOGI(TAG, "    tool output: %s", tool_output);
        } else {
            MIMI_LOGI(TAG, "    tool output (first 512): %.512s ...[truncated]", tool_output);
        }

        cJSON *result_block = cJSON_CreateObject();
        if (!result_block) {
            continue;
        }

        cJSON_AddStringToObject(result_block, "type", "tool_result");
        cJSON_AddStringToObject(result_block, "tool_use_id", call->id);
        cJSON_AddStringToObject(result_block, "content", tool_output);
        cJSON_AddItemToArray(content, result_block);
    }

    return content;
}

static void agent_loop_task(void *arg)
{
    (void)arg;

    const char *working_phrases[] = {
        "mimi is working...", "mimi is thinking...", "mimi is pondering...", "mimi is on it...", "mimi is cooking...",
    };
    const uint32_t phrase_count = (uint32_t)(sizeof(working_phrases) / sizeof(working_phrases[0]));

    char *system_prompt = calloc(1, MIMI_CONTEXT_BUF_SIZE);
    char *history_json  = calloc(1, MIMI_LLM_STREAM_BUF_SIZE);
    char *tool_output   = calloc(1, TOOL_OUTPUT_SIZE);

    MIMI_LOGI(TAG, "agent loop started");
    if (!system_prompt || !history_json || !tool_output) {
        MIMI_LOGE(TAG, "alloc buffers failed");
        free(system_prompt);
        free(history_json);
        free(tool_output);
        return;
    }

    const char *tools_json = tool_registry_get_tools_json();

    while (1) {
        mimi_msg_t in_msg = {0};
        if (message_bus_pop_inbound(&in_msg, MIMI_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }

        MIMI_LOGI(TAG, "================ AGENT TURN START ================");
        MIMI_LOGI(TAG, "processing msg channel=%s chat_id=%s", in_msg.channel, in_msg.chat_id);
        MIMI_LOGI(TAG, "user input: %s", in_msg.content ? in_msg.content : "(null)");

        if (context_build_system_prompt(system_prompt, MIMI_CONTEXT_BUF_SIZE) != OPRT_OK) {
            free(in_msg.content);
            continue;
        }
        append_turn_context_prompt(system_prompt, MIMI_CONTEXT_BUF_SIZE, &in_msg);
        MIMI_LOGI(TAG, "llm turn context channel=%s chat_id=%s", in_msg.channel, in_msg.chat_id);

        if (session_get_history_json(in_msg.chat_id, history_json, MIMI_LLM_STREAM_BUF_SIZE, MIMI_AGENT_MAX_HISTORY) !=
            OPRT_OK) {
            snprintf(history_json, MIMI_LLM_STREAM_BUF_SIZE, "[]");
        }

        cJSON *messages = cJSON_Parse(history_json);
        if (!messages || !cJSON_IsArray(messages)) {
            cJSON_Delete(messages);
            messages = cJSON_CreateArray();
        }

        const char *user_text = in_msg.content ? in_msg.content : "";
        cJSON      *user_msg  = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", user_text);
        cJSON_AddItemToArray(messages, user_msg);

        char *final_text          = NULL;
        int   iteration           = 0;
        bool  sent_working_status = false;

        while (iteration < MIMI_AGENT_MAX_TOOL_ITER) {
#if MIMI_AGENT_SEND_WORKING_STATUS
            if (!sent_working_status && strcmp(in_msg.channel, MIMI_CHAN_SYSTEM) != 0) {
                mimi_msg_t status = {0};
                strncpy(status.channel, in_msg.channel, sizeof(status.channel) - 1);
                strncpy(status.chat_id, in_msg.chat_id, sizeof(status.chat_id) - 1);
                uint32_t phrase_index = bounded_random_index(phrase_count);
                status.content        = strdup(working_phrases[phrase_index]);
                if (status.content) {
                    if (message_bus_push_outbound(&status) != OPRT_OK) {
                        MIMI_LOGW(TAG, "drop working status: outbound queue full");
                        free(status.content);
                    } else {
                        sent_working_status = true;
                    }
                }
            }
#endif

            MIMI_LOGI(TAG, "--- LLM call iteration=%d ---", iteration + 1);
            bool           force_tool = (iteration == 0); /* first iteration: force tool use */
            llm_response_t resp;
            OPERATE_RET    rt = llm_chat_tools_ex(system_prompt, messages, tools_json, force_tool, &resp);
            if (rt != OPRT_OK) {
                MIMI_LOGE(TAG, "llm_chat_tools failed: rt=%d iteration=%d", rt, iteration + 1);
                break;
            }

            MIMI_LOGI(TAG, "LLM response: tool_use=%s text_len=%u call_count=%d", resp.tool_use ? "true" : "false",
                      (unsigned)(resp.text ? strlen(resp.text) : 0), resp.call_count);

            if (!resp.tool_use) {
                if (resp.text && resp.text[0] != '\0') {
                    final_text = strdup(resp.text);
                    MIMI_LOGI(TAG, "final text (len=%u): %.512s%s", (unsigned)strlen(resp.text), resp.text,
                              strlen(resp.text) > 512 ? "...[truncated]" : "");
                } else {
                    MIMI_LOGW(TAG, "LLM returned no text and no tool calls");
                }
                llm_response_free(&resp);
                break;
            }

            MIMI_LOGI(TAG, "tool iteration=%d call_count=%d", iteration + 1, resp.call_count);
            for (int tc_i = 0; tc_i < resp.call_count; tc_i++) {
                MIMI_LOGI(TAG, "  tool_call[%d]: name=%s id=%s input=%s", tc_i, resp.calls[tc_i].name,
                          resp.calls[tc_i].id, resp.calls[tc_i].input ? resp.calls[tc_i].input : "(null)");
            }

            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON_AddItemToObject(asst_msg, "content", build_assistant_content(&resp));
            cJSON_AddItemToArray(messages, asst_msg);

            cJSON *result_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(result_msg, "role", "user");
            cJSON_AddItemToObject(result_msg, "content",
                                  build_tool_results(&resp, &in_msg, tool_output, TOOL_OUTPUT_SIZE));
            cJSON_AddItemToArray(messages, result_msg);

            llm_response_free(&resp);
            iteration++;
        }

        cJSON_Delete(messages);

        MIMI_LOGI(TAG, "agent loop completed: iterations=%d final_text=%s", iteration,
                  (final_text && final_text[0] != '\0') ? "present" : "empty");

        if (final_text && final_text[0] != '\0') {
            MIMI_LOGI(TAG, "sending final response (len=%u): %.512s%s", (unsigned)strlen(final_text), final_text,
                      strlen(final_text) > 512 ? "...[truncated]" : "");
            OPERATE_RET save_user_rt = session_append(in_msg.chat_id, "user", user_text);
            OPERATE_RET save_asst_rt = session_append(in_msg.chat_id, "assistant", final_text);
            if (save_user_rt != OPRT_OK || save_asst_rt != OPRT_OK) {
                MIMI_LOGW(TAG, "session save failed chat=%s user_rt=%d asst_rt=%d", in_msg.chat_id, save_user_rt,
                          save_asst_rt);
            }

            mimi_msg_t out_msg = {0};
            strncpy(out_msg.channel, in_msg.channel, sizeof(out_msg.channel) - 1);
            strncpy(out_msg.chat_id, in_msg.chat_id, sizeof(out_msg.chat_id) - 1);
            out_msg.content = final_text;
            if (message_bus_push_outbound(&out_msg) != OPRT_OK) {
                MIMI_LOGW(TAG, "drop final response: outbound queue full");
                free(out_msg.content);
            } else {
                final_text = NULL;
            }
        } else {
            mimi_msg_t out_msg = {0};
            strncpy(out_msg.channel, in_msg.channel, sizeof(out_msg.channel) - 1);
            strncpy(out_msg.chat_id, in_msg.chat_id, sizeof(out_msg.chat_id) - 1);
            out_msg.content = strdup("Sorry, I encountered an error.");
            if (out_msg.content) {
                if (message_bus_push_outbound(&out_msg) != OPRT_OK) {
                    MIMI_LOGW(TAG, "drop error response: outbound queue full");
                    free(out_msg.content);
                }
            }
        }

        free(final_text);
        free(in_msg.content);
        MIMI_LOGI(TAG, "free heap=%d", tal_system_get_free_heap_size());
        MIMI_LOGI(TAG, "================ AGENT TURN END ================");
    }
}

OPERATE_RET agent_loop_init(void)
{
    return OPRT_OK;
}

OPERATE_RET agent_loop_start(void)
{
    if (s_agent_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = MIMI_AGENT_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "mimi_agent";

    OPERATE_RET rt = tal_thread_create_and_start(&s_agent_thread, NULL, NULL, agent_loop_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "create agent thread failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}
