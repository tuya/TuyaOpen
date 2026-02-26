#pragma once

#include "mimi_base.h"
#include "cJSON.h"

#include "mimi_config.h"

typedef struct {
    char   id[64];
    char   name[32];
    char  *input;
    size_t input_len;
} llm_tool_call_t;

typedef struct {
    char           *text;
    size_t          text_len;
    llm_tool_call_t calls[MIMI_MAX_TOOL_CALLS];
    int             call_count;
    bool            tool_use;
} llm_response_t;

OPERATE_RET llm_proxy_init(void);
OPERATE_RET llm_set_api_key(const char *api_key);
OPERATE_RET llm_set_api_url(const char *api_url);
OPERATE_RET llm_set_provider(const char *provider);
OPERATE_RET llm_set_model(const char *model);
OPERATE_RET llm_chat(const char *system_prompt, const char *messages_json, char *response_buf, size_t buf_size);
OPERATE_RET llm_chat_tools(const char *system_prompt, cJSON *messages, const char *tools_json, llm_response_t *resp);
void        llm_response_free(llm_response_t *resp);
