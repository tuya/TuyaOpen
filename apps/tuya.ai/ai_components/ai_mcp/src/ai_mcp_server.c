/**
 * @file ai_mcp_server.c
 * @brief MCP (Model Context Protocol) Server implementation for TuyaOS
 * @version 1.0.0
 * @date 2025-10-10
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#include <stdio.h>
#include <string.h>

#include "tal_api.h"

#include "tuya_ai_agent.h"
#include "uni_base64.h"
#include "mix_method.h"

#include "ai_mcp_server.h"

/***********************************************************
************************macro define************************
***********************************************************/
#ifdef ENABLE_EXT_RAM
#define AI_MCP_MALLOC    tal_psram_malloc
#define AI_MCP_FREE      tal_psram_free
#else
#define AI_MCP_MALLOC    tal_malloc
#define AI_MCP_FREE      tal_free
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * typedef MCP_SEND_MESSAGE_CB - Message sending callback
 * @message: JSON-RPC message to send
 * 
 * This callback is called when the MCP server needs to send a message
 * back to the client (e.g., response or notification).
 */
typedef void (*MCP_SEND_MESSAGE_CB)(const char *message);

/**
 * MCP server instance
 * @tools: Linked list of registered tools
 * @tool_count: Number of registered tools
 * @send_message: Message sending callback, use default if NULL
 * @server_name: Server name (board name)
 * @server_version: Server version
 */
typedef struct {
    bool initialized;
    char *name;
    char *version;
    MCP_TOOL_T *tools;
    int tool_count;
    MCP_SEND_MESSAGE_CB send_message;
} MCP_SERVER_CTX_T;

typedef struct {
    char *id;
    MCP_PROPERTY_LIST_T *arguments;
    MCP_TOOL_T *tool;
} TOOL_CALL_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static MCP_SERVER_CTX_T s_server_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/
MCP_PROPERTY_T *ai_mcp_property_create(const char *name, MCP_PROPERTY_TYPE_E type, const char *description)
{
    if (!name)
        return NULL;

    MCP_PROPERTY_T *prop = (MCP_PROPERTY_T *)AI_MCP_MALLOC(sizeof(MCP_PROPERTY_T));
    if (!prop)
        return NULL;

    memset(prop, 0, sizeof(MCP_PROPERTY_T));
    prop->name = mm_strdup(name);
    if (!prop->name) {
        AI_MCP_FREE(prop);
        return NULL;
    }

    prop->description = mm_strdup(description);
    if (!prop->description) {
        AI_MCP_FREE(prop->name);
        AI_MCP_FREE(prop);
        return NULL;
    }

    prop->type = type;
    return prop;
}

MCP_PROPERTY_T *ai_mcp_property_create_from_def(const MCP_PROPERTY_DEF_T *src)
{
    MCP_PROPERTY_T *dest;

    if (!src)
        return NULL;

    dest = ai_mcp_property_create(src->name, src->type, src->description);
    if (!dest)
        return NULL;

    if (src->has_default) {
        switch (src->type) {
        case MCP_PROPERTY_TYPE_BOOLEAN:
            ai_mcp_property_set_default_bool(dest, src->default_val.bool_val);
            break;
        case MCP_PROPERTY_TYPE_INTEGER:
            ai_mcp_property_set_default_int(dest, src->default_val.int_val);
            break;
        case MCP_PROPERTY_TYPE_STRING:
            ai_mcp_property_set_default_str(dest, src->default_val.str_val);
            break;
        default:
            break;
        }
    }

    if (src->has_range && src->type == MCP_PROPERTY_TYPE_INTEGER) {
        ai_mcp_property_set_range(dest, src->min_val, src->max_val);
    }

    if (src->type == MCP_PROPERTY_TYPE_STRING && src->has_default) {
        dest->default_val.str_val = mm_strdup(src->default_val.str_val);
        if (!dest->default_val.str_val) {
            ai_mcp_property_destroy(dest);
            return NULL;
        }
    }

    return dest;
}

VOID ai_mcp_property_destroy(MCP_PROPERTY_T *prop)
{
    if (!prop)
        return;

    if (prop->name)
        AI_MCP_FREE(prop->name);
    if (prop->description)
        AI_MCP_FREE(prop->description);
    if (prop->type == MCP_PROPERTY_TYPE_STRING && prop->has_default) {
        AI_MCP_FREE(prop->default_val.str_val);
    }
    AI_MCP_FREE(prop);
}

OPERATE_RET ai_mcp_property_set_default_bool(MCP_PROPERTY_T *prop, bool value)
{
    if (!prop || prop->type != MCP_PROPERTY_TYPE_BOOLEAN)
        return OPRT_INVALID_PARM;

    prop->has_default = true;
    prop->default_val.type = MCP_PROPERTY_TYPE_BOOLEAN;
    prop->default_val.bool_val = value;

    return OPRT_OK;
}

OPERATE_RET ai_mcp_property_set_default_int(MCP_PROPERTY_T *prop, int value)
{
    if (!prop || prop->type != MCP_PROPERTY_TYPE_INTEGER)
        return OPRT_INVALID_PARM;

    prop->has_default = true;
    prop->default_val.type = MCP_PROPERTY_TYPE_INTEGER;
    prop->default_val.int_val = value;

    return OPRT_OK;
}

OPERATE_RET ai_mcp_property_set_default_str(MCP_PROPERTY_T *prop, const char *value)
{
    if (!prop || !value || prop->type != MCP_PROPERTY_TYPE_STRING)
        return OPRT_INVALID_PARM;

    prop->has_default = true;
    prop->default_val.type = MCP_PROPERTY_TYPE_STRING;
    prop->default_val.str_val = mm_strdup(value);
    if (!prop->default_val.str_val)
        return OPRT_MALLOC_FAILED;

    return OPRT_OK;
}

OPERATE_RET ai_mcp_property_set_range(MCP_PROPERTY_T *prop, int min_val, int max_val)
{
    if (!prop || prop->type != MCP_PROPERTY_TYPE_INTEGER)
        return OPRT_INVALID_PARM;

    if (min_val > max_val)
        return OPRT_INVALID_PARM;

    prop->has_range = true;
    prop->min_val = min_val;
    prop->max_val = max_val;

    return OPRT_OK;
}

MCP_PROPERTY_T *ai_mcp_property_dup(const MCP_PROPERTY_T *src)
{
    MCP_PROPERTY_T *dest;

    if (!src)
        return NULL;

    dest = ai_mcp_property_create(src->name, src->type, src->description);
    if (!dest)
        return NULL;

    memcpy(&dest->has_default, &src->has_default, sizeof(MCP_PROPERTY_T) - offsetof(MCP_PROPERTY_T, has_default));
    if (src->type == MCP_PROPERTY_TYPE_STRING && src->has_default) {
        dest->default_val.str_val = mm_strdup(src->default_val.str_val);
        if (!dest->default_val.str_val) {
            ai_mcp_property_destroy(dest);
            return NULL;
        }
    }

    return dest;
}

cJSON *ai_mcp_property_to_json(const MCP_PROPERTY_T *prop)
{
    cJSON *json;

    if (!prop)
        return NULL;

    json = cJSON_CreateObject();
    if (!json)
        return NULL;

    switch (prop->type) {
    case MCP_PROPERTY_TYPE_BOOLEAN:
        cJSON_AddStringToObject(json, "type", "boolean");
        if (prop->has_default)
            cJSON_AddBoolToObject(json, "default", prop->default_val.bool_val);
        break;

    case MCP_PROPERTY_TYPE_INTEGER:
        cJSON_AddStringToObject(json, "type", "integer");
        if (prop->has_default)
            cJSON_AddNumberToObject(json, "default", prop->default_val.int_val);
        if (prop->has_range) {
            cJSON_AddNumberToObject(json, "minimum", prop->min_val);
            cJSON_AddNumberToObject(json, "maximum", prop->max_val);
        }
        break;

    case MCP_PROPERTY_TYPE_STRING:
        cJSON_AddStringToObject(json, "type", "string");
        if (prop->has_default)
            cJSON_AddStringToObject(json, "default", prop->default_val.str_val);
        break;

    default:
        cJSON_Delete(json);
        return NULL;
    }

    if (strlen(prop->description) > 0)
        cJSON_AddStringToObject(json, "description", prop->description);

    return json;
}

MCP_PROPERTY_LIST_T *ai_mcp_property_list_create(VOID)
{
    MCP_PROPERTY_LIST_T *list = (MCP_PROPERTY_LIST_T *)AI_MCP_MALLOC(sizeof(MCP_PROPERTY_LIST_T));
    if (!list)
        return NULL;

    ai_mcp_property_list_init(list);
    return list;
}

VOID ai_mcp_property_list_init(MCP_PROPERTY_LIST_T *list)
{
    if (!list)
        return;

    memset(list, 0, sizeof(*list));
}

VOID ai_mcp_property_list_destroy(MCP_PROPERTY_LIST_T *list)
{
    int i;

    if (!list)
        return;

    for (i = 0; i < list->count; i++) {
        ai_mcp_property_destroy(list->properties[i]);
    }
    AI_MCP_FREE(list);
}


OPERATE_RET ai_mcp_property_list_add(MCP_PROPERTY_LIST_T *list, MCP_PROPERTY_T *prop)
{
    if (!list || !prop)
        return OPRT_INVALID_PARM;

    if (list->count >= MCP_MAX_PROPERTIES)
        return OPRT_COM_ERROR;

    list->properties[list->count++] = prop;

    return OPRT_OK;
}

const MCP_PROPERTY_T *ai_mcp_property_list_find(const MCP_PROPERTY_LIST_T *list, const char *name)
{
    int i;

    if (!list || !name)
        return NULL;

    for (i = 0; i < list->count; i++) {
        if (strcmp(list->properties[i]->name, name) == 0)
            return list->properties[i];
    }

    return NULL;
}

MCP_PROPERTY_LIST_T *ai_mcp_property_list_dup(const MCP_PROPERTY_LIST_T *src)
{
    MCP_PROPERTY_LIST_T *dest;
    int i;

    if (!src)
        return NULL;

    dest = ai_mcp_property_list_create();
    if (!dest)
        return NULL;

    for (i = 0; i < src->count; i++) {
        MCP_PROPERTY_T *prop_dup = ai_mcp_property_dup(src->properties[i]);
        if (!prop_dup) {
            ai_mcp_property_list_destroy(dest);
            return NULL;
        }
        dest->properties[dest->count++] = prop_dup;
    }

    return dest;
}

cJSON *ai_mcp_property_list_to_json(const MCP_PROPERTY_LIST_T *list)
{
    cJSON *json;
    int i;

    if (!list)
        return NULL;

    json = cJSON_CreateObject();
    if (!json)
        return NULL;

    for (i = 0; i < list->count; i++) {
        cJSON *prop_json = ai_mcp_property_to_json(list->properties[i]);
        if (prop_json)
            cJSON_AddItemToObject(json, list->properties[i]->name, prop_json);
    }

    return json;
}

/* === Return Value Management Functions === */

VOID ai_mcp_return_value_init(MCP_RETURN_VALUE_T *ret_val, MCP_RETURN_TYPE_E type)
{
    if (!ret_val)
        return;

    memset(ret_val, 0, sizeof(*ret_val));
    ret_val->type = type;
}

VOID ai_mcp_return_value_set_bool(MCP_RETURN_VALUE_T *ret_val, bool value)
{
    if (!ret_val)
        return;

    ret_val->type = MCP_RETURN_TYPE_BOOLEAN;
    ret_val->bool_val = value;
}

VOID ai_mcp_return_value_set_int(MCP_RETURN_VALUE_T *ret_val, int value)
{
    if (!ret_val)
        return;

    ret_val->type = MCP_RETURN_TYPE_INTEGER;
    ret_val->int_val = value;
}

OPERATE_RET ai_mcp_return_value_set_str(MCP_RETURN_VALUE_T *ret_val, const char *value)
{
    if (!ret_val || !value)
        return OPRT_INVALID_PARM;

    /* Clean up any existing string */
    if (ret_val->type == MCP_RETURN_TYPE_STRING && ret_val->str_val) {
        AI_MCP_FREE(ret_val->str_val);
        ret_val->str_val = NULL;
    }

    ret_val->type = MCP_RETURN_TYPE_STRING;
    ret_val->str_val = mm_strdup(value);
    
    return ret_val->str_val ? OPRT_OK : OPRT_MALLOC_FAILED;
}

VOID ai_mcp_return_value_set_json(MCP_RETURN_VALUE_T *ret_val, cJSON *json)
{
    if (!ret_val || !json)
        return;

    /* Clean up any existing JSON */
    if (ret_val->type == MCP_RETURN_TYPE_JSON && ret_val->json_val) {
        cJSON_Delete(ret_val->json_val);
        ret_val->json_val = NULL;
    }

    ret_val->type = MCP_RETURN_TYPE_JSON;
    ret_val->json_val = json;
}

OPERATE_RET ai_mcp_return_value_set_image(MCP_RETURN_VALUE_T *ret_val,
                                            const char *mime_type,
                                            const VOID *data, uint32_t data_len)
{
    uint32_t encoded_len;
    int ret;

    if (!ret_val || !mime_type || !data || data_len == 0)
        return OPRT_INVALID_PARM;

    /* Clean up any existing image data */
    if (ret_val->type == MCP_RETURN_TYPE_IMAGE) {
        AI_MCP_FREE(ret_val->image_val.mime_type);
        AI_MCP_FREE(ret_val->image_val.data);
        memset(&ret_val->image_val, 0, sizeof(ret_val->image_val));
    }

    ret_val->type = MCP_RETURN_TYPE_IMAGE;

    /* Allocate and copy MIME type */
    ret_val->image_val.mime_type = mm_strdup(mime_type);
    if (!ret_val->image_val.mime_type)
        return OPRT_MALLOC_FAILED;

    /* Calculate base64 encoded length */
    encoded_len = ((data_len + 2) / 3) * 4 + 1;  /* +1 for null terminator */
    
    ret_val->image_val.data = AI_MCP_MALLOC(encoded_len);
    if (!ret_val->image_val.data) {
        AI_MCP_FREE(ret_val->image_val.mime_type);
        ret_val->image_val.mime_type = NULL;
        return OPRT_MALLOC_FAILED;
    }

    /* Base64 encode the data */
    ret = ai_mcp_base64_encode(data, data_len, ret_val->image_val.data, encoded_len);
    if (ret != 0) {
        AI_MCP_FREE(ret_val->image_val.mime_type);
        AI_MCP_FREE(ret_val->image_val.data);
        memset(&ret_val->image_val, 0, sizeof(ret_val->image_val));
        return ret;
    }

    ret_val->image_val.data_len = strlen(ret_val->image_val.data);
    return OPRT_OK;
}

VOID ai_mcp_return_value_cleanup(MCP_RETURN_VALUE_T *ret_val)
{
    if (!ret_val)
        return;

    switch (ret_val->type) {
    case MCP_RETURN_TYPE_STRING:
        AI_MCP_FREE(ret_val->str_val);
        break;
    case MCP_RETURN_TYPE_JSON:
        if (ret_val->json_val)
            cJSON_Delete(ret_val->json_val);
        break;
    case MCP_RETURN_TYPE_IMAGE:
        AI_MCP_FREE(ret_val->image_val.mime_type);
        AI_MCP_FREE(ret_val->image_val.data);
        break;
    default:
        break;
    }

    memset(ret_val, 0, sizeof(*ret_val));
}

cJSON *ai_mcp_return_value_to_json(const MCP_RETURN_VALUE_T *ret_val)
{
    cJSON *json, *content, *item;
    char *json_str;

    if (!ret_val)
        return NULL;

    json = cJSON_CreateObject();
    if (!json)
        return NULL;

    content = cJSON_CreateArray();
    if (!content) {
        cJSON_Delete(json);
        return NULL;
    }

    switch (ret_val->type) {
    case MCP_RETURN_TYPE_BOOLEAN:
        item = cJSON_CreateObject();
        if (item) {
            cJSON_AddStringToObject(item, "type", "text");
            cJSON_AddStringToObject(item, "text", ret_val->bool_val ? "true" : "false");
            cJSON_AddItemToArray(content, item);
        }
        break;

    case MCP_RETURN_TYPE_INTEGER:
        item = cJSON_CreateObject();
        if (item) {
            char int_str[32];
            snprintf(int_str, sizeof(int_str), "%d", ret_val->int_val);
            cJSON_AddStringToObject(item, "type", "text");
            cJSON_AddStringToObject(item, "text", int_str);
            cJSON_AddItemToArray(content, item);
        }
        break;

    case MCP_RETURN_TYPE_STRING:
        item = cJSON_CreateObject();
        if (item && ret_val->str_val) {
            cJSON_AddStringToObject(item, "type", "text");
            cJSON_AddStringToObject(item, "text", ret_val->str_val);
            cJSON_AddItemToArray(content, item);
        }
        break;

    case MCP_RETURN_TYPE_JSON:
        item = cJSON_CreateObject();
        if (item && ret_val->json_val) {
            json_str = cJSON_PrintUnformatted(ret_val->json_val);
            if (json_str) {
                cJSON_AddStringToObject(item, "type", "text");
                cJSON_AddStringToObject(item, "text", json_str);
                cJSON_free(json_str);
                cJSON_AddItemToArray(content, item);
            } else {
                cJSON_Delete(item);
            }
        }
        break;

    case MCP_RETURN_TYPE_IMAGE:
        item = cJSON_CreateObject();
        if (item && ret_val->image_val.mime_type && ret_val->image_val.data) {
            cJSON_AddStringToObject(item, "type", "image");
            cJSON_AddStringToObject(item, "mimeType", ret_val->image_val.mime_type);
            cJSON_AddStringToObject(item, "data", ret_val->image_val.data);
            cJSON_AddItemToArray(content, item);
        }
        break;

    default:
        break;
    }

    cJSON_AddItemToObject(json, "content", content);
    cJSON_AddBoolToObject(json, "isError", false);

    return json;
}

/* === Tool Management Functions === */

MCP_TOOL_T *ai_mcp_tool_create(const char *name, const char *description,
                                 MCP_TOOL_CALLBACK callback, VOID *user_data)
{
    MCP_TOOL_T *tool;

    if (!name || !description || !callback)
        return NULL;

    tool = tal_calloc(1, sizeof(*tool));
    if (!tool)
        return NULL;

    tool->name = mm_strdup(name);
    tool->description = mm_strdup(description);
    if (!tool->name || !tool->description) {
        if (tool->name)
            AI_MCP_FREE(tool->name);
        if (tool->description)
            AI_MCP_FREE(tool->description);
        AI_MCP_FREE(tool);
        return NULL;
    }
    tool->callback = callback;
    tool->user_data = user_data;
    ai_mcp_property_list_init(&tool->properties);

    return tool;
}

OPERATE_RET ai_mcp_tool_register(const char *name, const char *description,
                                 MCP_TOOL_CALLBACK callback, VOID *user_data, ...)
{
    OPERATE_RET rt;
    MCP_TOOL_T *tool = ai_mcp_tool_create(name, description, callback, user_data);
    if (!tool)
        return OPRT_MALLOC_FAILED;

    va_list args;
    va_start(args, user_data);
    MCP_PROPERTY_DEF_T *prop_def;
    while ((prop_def = va_arg(args, MCP_PROPERTY_DEF_T *))) {
        // dup property to avoid ownership issues
        MCP_PROPERTY_T *prop = ai_mcp_property_create_from_def(prop_def);
        if (prop) {
            rt = ai_mcp_tool_add_property(tool, prop);
            if (rt != OPRT_OK) {
                ai_mcp_property_destroy(prop);
                ai_mcp_tool_destroy(tool);
                va_end(args);
                return rt;
            }
        }
    }
    va_end(args);
    rt = ai_mcp_server_add_tool(tool);
    if (rt != OPRT_OK) {
        ai_mcp_tool_destroy(tool);
        return rt;
    }
    return OPRT_OK;
}

VOID ai_mcp_tool_destroy(MCP_TOOL_T *tool)
{
    if (!tool)
        return;

    /* Free properties */
    for (int i = 0; i < tool->properties.count; i++) {
        ai_mcp_property_destroy(tool->properties.properties[i]);
    }
    AI_MCP_FREE(tool->name);
    AI_MCP_FREE(tool->description);
    AI_MCP_FREE(tool);
}

OPERATE_RET ai_mcp_tool_add_property(MCP_TOOL_T *tool, MCP_PROPERTY_T *prop)
{
    if (!tool || !prop)
        return OPRT_INVALID_PARM;

    return ai_mcp_property_list_add(&tool->properties, prop);
}

cJSON *ai_mcp_tool_to_json(const MCP_TOOL_T *tool)
{
    cJSON *json, *input_schema, *properties, *required;
    int i;

    if (!tool)
        return NULL;

    json = cJSON_CreateObject();
    if (!json)
        return NULL;

    cJSON_AddStringToObject(json, "name", tool->name);
    cJSON_AddStringToObject(json, "description", tool->description);

    /* Create input schema */
    input_schema = cJSON_CreateObject();
    if (!input_schema) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON_AddStringToObject(input_schema, "type", "object");

    properties = ai_mcp_property_list_to_json(&tool->properties);
    if (properties)
        cJSON_AddItemToObject(input_schema, "properties", properties);

    /* Add required properties */
    required = cJSON_CreateArray();
    if (required) {
        for (i = 0; i < tool->properties.count; i++) {
            if (!tool->properties.properties[i]->has_default) {
                cJSON_AddItemToArray(required, 
                    cJSON_CreateString(tool->properties.properties[i]->name));
            }
        }
        if (cJSON_GetArraySize(required) > 0)
            cJSON_AddItemToObject(input_schema, "required", required);
        else
            cJSON_Delete(required);
    }

    cJSON_AddItemToObject(json, "inputSchema", input_schema);

    return json;
}

/* === Server Management Functions === */

VOID __send_message_default(const char *message)
{
    tuya_ai_agent_mcp_response((char *)message);
}

OPERATE_RET ai_mcp_server_init(const char *name, const char *version)
{
    if (s_server_ctx.initialized) {
        PR_WARN("MCP server already initialized");
        return OPRT_OK;
    }

    if (!name || !version)
        return OPRT_INVALID_PARM;

    memset(&s_server_ctx, 0, sizeof(s_server_ctx));
    s_server_ctx.name = mm_strdup(name);
    if (!s_server_ctx.name) {
        return OPRT_MALLOC_FAILED;
    }
    s_server_ctx.version = mm_strdup(version);
    if (!s_server_ctx.version) {
        AI_MCP_FREE(s_server_ctx.name);
        return OPRT_MALLOC_FAILED;
    }
    s_server_ctx.send_message = __send_message_default;
    tuya_ai_agent_mcp_set_cb(ai_mcp_server_parse_message, NULL);
    s_server_ctx.initialized = TRUE;
    return OPRT_OK;
}

VOID ai_mcp_server_destroy(VOID)
{
    MCP_TOOL_T *tool, *tmp;

    if (!s_server_ctx.initialized)
        return;

    s_server_ctx.initialized = FALSE;
    tuya_ai_agent_mcp_set_cb(NULL, NULL);

    /* Free all tools */
    tool = s_server_ctx.tools;
    while (tool) {
        tmp = tool->next;
        ai_mcp_tool_destroy(tool);
        tool = tmp;
    }
    AI_MCP_FREE(s_server_ctx.name);
    AI_MCP_FREE(s_server_ctx.version);
    memset(&s_server_ctx, 0, sizeof(s_server_ctx));
}

OPERATE_RET ai_mcp_server_add_tool(MCP_TOOL_T *tool)
{
    MCP_TOOL_T *existing;

    if (!s_server_ctx.initialized || !tool)
        return OPRT_INVALID_PARM;

    /* Check for duplicate tool names */
    existing = ai_mcp_server_find_tool(tool->name);
    if (existing) {
        PR_WARN("Tool %s already exists", tool->name);
        return OPRT_COM_ERROR;
    }

    /* Add to linked list */
    tool->next = s_server_ctx.tools;
    s_server_ctx.tools = tool;
    s_server_ctx.tool_count++;

    PR_INFO("Added tool: %s", tool->name);
    return OPRT_OK;
}

MCP_TOOL_T *ai_mcp_server_find_tool(const char *name)
{
    MCP_TOOL_T *tool;

    if (!s_server_ctx.initialized || !name)
        return NULL;

    for (tool = s_server_ctx.tools; tool; tool = tool->next) {
        if (strcmp(tool->name, name) == 0)
            return tool;
    }

    return NULL;
}

/* === Message Handling Functions === */

static OPERATE_RET __reply_result(const char *id, cJSON *result)
{
    cJSON *response;
    char *json_str;

    if (!result || !id)
        return OPRT_INVALID_PARM;

    response = cJSON_CreateObject();
    if (!response)
        return OPRT_MALLOC_FAILED;

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddItemToObject(response, "result", result);

    json_str = cJSON_PrintUnformatted(response);
    if (json_str) {
        PR_DEBUG("MCP Reply: %s", json_str);
        if (s_server_ctx.send_message)
            s_server_ctx.send_message(json_str);
        cJSON_free(json_str);
    }

    cJSON_Delete(response);
    return OPRT_OK;
}

static OPERATE_RET __reply_error(const char *id, int error_code, const char *message)
{
    cJSON *response, *error;
    char *json_str;

    if (!message || !id)
        return OPRT_INVALID_PARM;

    response = cJSON_CreateObject();
    if (!response)
        return OPRT_MALLOC_FAILED;

    error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(response);
        return OPRT_MALLOC_FAILED;
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddNumberToObject(error, "code", error_code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(response, "error", error);

    json_str = cJSON_PrintUnformatted(response);
    if (json_str) {
        PR_DEBUG("MCP Error Reply: %s", json_str);
        s_server_ctx.send_message(json_str);
        cJSON_free(json_str);
    }

    cJSON_Delete(response);
    return OPRT_OK;
}

static VOID_T __tool_call(VOID_T *data)
{
    OPERATE_RET rt;
    MCP_RETURN_VALUE_T ret_val;
    cJSON *result;

    TOOL_CALL_MSG_T *msg = (TOOL_CALL_MSG_T *)data;
    if (!msg || !msg->tool || !msg->arguments) {
        __reply_error(msg ? msg->id : NULL, MCP_ERROR_INTERNAL, "Invalid tool call message");
        goto exit;
    }

    /* Initialize return value */
    ai_mcp_return_value_init(&ret_val, MCP_RETURN_TYPE_BOOLEAN);

    rt = msg->tool->callback(msg->arguments, &ret_val, msg->tool->user_data);
    if (rt != OPRT_OK) {
        ai_mcp_return_value_cleanup(&ret_val);
        __reply_error(msg->id, MCP_ERROR_INTERNAL, "Tool execution failed");
        goto exit;
    }

    /* Convert return value to JSON result */
    result = ai_mcp_return_value_to_json(&ret_val);
    ai_mcp_return_value_cleanup(&ret_val);

    if (!result) {
        __reply_error(msg->id, MCP_ERROR_INTERNAL, "Failed to format result");
        goto exit;
    }

    __reply_result(msg->id, result);

exit:
    if (msg) {
        if (msg->id)
            AI_MCP_FREE(msg->id);
        if (msg->arguments)
            ai_mcp_property_list_destroy(msg->arguments);
        AI_MCP_FREE(msg);
    }
}

static OPERATE_RET __handle_initialize(cJSON *params, const char *id)
{
    cJSON *root, *node;

    /* Create response */
    root = cJSON_CreateObject();
    if (!root)
        return OPRT_MALLOC_FAILED;

    cJSON_AddStringToObject(root, "protocolVersion", MCP_PROTOCOL_VERSION);

    node = cJSON_CreateObject();
    if (node) {
        cJSON *tools = cJSON_CreateObject();
        if (tools)
            cJSON_AddItemToObject(node, "tools", tools);
        cJSON_AddItemToObject(root, "capabilities", node);
    }

    node = cJSON_CreateObject();
    if (node) {
        cJSON_AddStringToObject(node, "name", s_server_ctx.name);
        cJSON_AddStringToObject(node, "version", s_server_ctx.version);
        cJSON_AddItemToObject(root, "serverInfo", node);
    }

    return __reply_result(id, root);
}

static OPERATE_RET __handle_tools_list(cJSON *params, const char *id)
{
    const char *cursor_str = "";
    bool found_cursor = false;
    MCP_TOOL_T *tool;
    cJSON *result, *tools_array;
    int json_len = 0;

    /* Parse parameters */
    if (params) {
        cJSON *cursor = cJSON_GetObjectItem(params, "cursor");
        if (cJSON_IsString(cursor))
            cursor_str = cursor->valuestring;
    }

    result = cJSON_CreateObject();
    if (!result)
        return OPRT_MALLOC_FAILED;

    tools_array = cJSON_CreateArray();
    if (!tools_array) {
        cJSON_Delete(result);
        return OPRT_MALLOC_FAILED;
    }

    found_cursor = (strlen(cursor_str) == 0);

    /* Iterate through tools */
    for (tool = s_server_ctx.tools; tool; tool = tool->next) {
        cJSON *tool_json;
        char *tool_str;
        int tool_len;

        /* Skip until we find the cursor */
        if (!found_cursor) {
            if (strcmp(tool->name, cursor_str) == 0)
                found_cursor = true;
            else
                continue;
        }

        tool_json = ai_mcp_tool_to_json(tool);
        if (!tool_json)
            continue;

        /* Check payload size limit */
        tool_str = cJSON_PrintUnformatted(tool_json);
        if (tool_str) {
            tool_len = strlen(tool_str);
            if (json_len + tool_len + 100 > MCP_MAX_PAYLOAD_SIZE) {
                /* Set next cursor and break */
                cJSON_AddStringToObject(result, "nextCursor", tool->name);
                cJSON_free(tool_str);
                cJSON_Delete(tool_json);
                break;
            }
            json_len += tool_len;
            cJSON_free(tool_str);
        }

        cJSON_AddItemToArray(tools_array, tool_json);
    }

    cJSON_AddItemToObject(result, "tools", tools_array);
    return __reply_result(id, result);
}

static OPERATE_RET __parse_property_value(MCP_PROPERTY_LIST_T *prop_list,
                    const char *name, cJSON *value)
{
    const MCP_PROPERTY_T *prop_def;
    MCP_PROPERTY_T *prop;
    int i;

    /* Find property definition */
    prop_def = ai_mcp_property_list_find(prop_list, name);
    if (!prop_def)
        return OPRT_NOT_FOUND;

    /* Find mutable property in list */
    for (i = 0; i < prop_list->count; i++) {
        if (strcmp(prop_list->properties[i]->name, name) == 0) {
            prop = prop_list->properties[i];
            break;
        }
    }
    if (i >= prop_list->count)
        return OPRT_NOT_FOUND;

    /* Parse value based on type */
    switch (prop_def->type) {
    case MCP_PROPERTY_TYPE_BOOLEAN:
        if (!cJSON_IsBool(value))
            return OPRT_INVALID_PARM;
        prop->default_val.type = MCP_PROPERTY_TYPE_BOOLEAN;
        prop->default_val.bool_val = cJSON_IsTrue(value);
        prop->has_default = true;
        break;

    case MCP_PROPERTY_TYPE_INTEGER:
        if (!cJSON_IsNumber(value))
            return OPRT_INVALID_PARM;
        prop->default_val.type = MCP_PROPERTY_TYPE_INTEGER;
        prop->default_val.int_val = value->valueint;
        
        /* Validate range */
        if (prop_def->has_range) {
            if (prop->default_val.int_val < prop_def->min_val ||
                prop->default_val.int_val > prop_def->max_val)
                return OPRT_INVALID_PARM;
        }
        prop->has_default = true;
        break;

    case MCP_PROPERTY_TYPE_STRING:
        if (!cJSON_IsString(value))
            return OPRT_INVALID_PARM;
        prop->default_val.type = MCP_PROPERTY_TYPE_STRING;
        if (prop->has_default && prop->default_val.str_val)
            AI_MCP_FREE(prop->default_val.str_val);    // Free existing string
        prop->default_val.str_val = mm_strdup(value->valuestring);
        if (!prop->default_val.str_val)
            return OPRT_MALLOC_FAILED;
        prop->has_default = true;
        break;

    default:
        return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}

static OPERATE_RET __handle_tools_call(cJSON *params, const char *id)
{
    int ret, i;
    cJSON *tool_name_json, *tool_arguments;
    const char *tool_name;
    MCP_TOOL_T *tool;
    MCP_PROPERTY_LIST_T *arguments = NULL;
    TOOL_CALL_MSG_T *msg = NULL;
    const char *error_msg = NULL;
    int error_code = MCP_ERROR_INTERNAL;

    if (!cJSON_IsObject(params)) {
        error_code = MCP_ERROR_INVALID_PARAMS;
        error_msg = "Missing params";
        goto err;
    }

    tool_name_json = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(tool_name_json)) {
        error_code = MCP_ERROR_INVALID_PARAMS;
        error_msg = "Missing tool name";
        goto err;
    }
    tool_name = tool_name_json->valuestring;

    tool_arguments = cJSON_GetObjectItem(params, "arguments");
    if (tool_arguments && !cJSON_IsObject(tool_arguments)) {
        error_code = MCP_ERROR_INVALID_PARAMS;
        error_msg = "Invalid arguments";
        goto err;
    }

    /* Find the tool */
    tool = ai_mcp_server_find_tool(tool_name);
    if (!tool) {
        error_code = MCP_ERROR_METHOD_NOT_FOUND;
        error_msg = "Unknown tool";
        goto err;
    }

    msg = (TOOL_CALL_MSG_T *)AI_MCP_MALLOC(sizeof(TOOL_CALL_MSG_T));
    if (!msg) {
        error_msg = "Failed to allocate tool call message";
        goto err;
    }
    memset(msg, 0, sizeof(TOOL_CALL_MSG_T));

    /* Copy tool properties to arguments and parse values */
    arguments = ai_mcp_property_list_dup(&tool->properties);
    if (!arguments) {
        error_msg = "Failed to allocate arguments";
        goto err;
    }

    msg->id = mm_strdup(id);
    if (!msg->id) {
        error_msg = "Failed to allocate id";
        goto err;
    }
    msg->arguments = arguments;
    msg->tool = tool;

    if (tool_arguments) {
        cJSON *arg;
        cJSON_ArrayForEach(arg, tool_arguments) {
            ret = __parse_property_value(arguments, arg->string, arg);
            if (ret != OPRT_OK) {
                error_code = MCP_ERROR_INVALID_PARAMS;
                error_msg = "Failed to parse argument";
                goto err;
            }
        }
    }

    /* Check for missing required arguments */
    for (i = 0; i < arguments->count; i++) {
        if (!arguments->properties[i]->has_default) {
            error_code = MCP_ERROR_INVALID_PARAMS;
            error_msg = "Missing required argument";
            goto err;
        }
    }

    /* Call the tool in workqueue */
    ret = tal_workq_schedule(WORKQ_SYSTEM, __tool_call, msg);
    if (ret != OPRT_OK) {
        error_msg = "Failed to schedule tool call";
        error_code = MCP_ERROR_INTERNAL;
        goto err;
    }
    return OPRT_OK;

err:
    if (msg) {
        if (msg->id)
            AI_MCP_FREE(msg->id);
        if (msg->arguments)
            ai_mcp_property_list_destroy(msg->arguments);
        AI_MCP_FREE(msg);
    }
    return __reply_error(id, error_code, error_msg);
}

OPERATE_RET ai_mcp_server_parse_message(const cJSON *json, VOID *user_data)
{
    cJSON *node;
    const char *method;
    const char *id = NULL;

    if (!s_server_ctx.initialized || !json)
        return OPRT_INVALID_PARM;

    /* Check JSONRPC version */
    node = cJSON_GetObjectItem(json, "jsonrpc");
    if (!node || !cJSON_IsString(node) || strcmp(node->valuestring, "2.0") != 0) {
        PR_ERR("Invalid JSONRPC version");
        return OPRT_INVALID_PARM;
    }

    /* Check method */
    node = cJSON_GetObjectItem(json, "method");
    if (!node || !cJSON_IsString(node)) {
        PR_ERR("Missing method");
        return OPRT_INVALID_PARM;
    }
    method = node->valuestring;

    /* Skip notifications */
    if (strncmp(method, "notifications", 13) == 0)
        return OPRT_OK;

    /* Check ID */
    node = cJSON_GetObjectItem(json, "id");
    if (!node) {
        PR_ERR("Missing ID for method: %s", method);
        return OPRT_INVALID_PARM;
    }

    if (!node || !cJSON_IsString(node) || node->valuestring == NULL) {
        PR_ERR("Missing ID or Invalid ID type for method: %s", method);
        return OPRT_INVALID_PARM;
    }
    id = node->valuestring;

    /* Get params */
    node = cJSON_GetObjectItem(json, "params");
    if (node && !cJSON_IsObject(node)) {
        PR_ERR("Invalid params for method: %s", method);
        return OPRT_INVALID_PARM;
    }

    /* Handle methods */
    if (strcmp(method, "initialize") == 0) {
        return __handle_initialize(node, id);
    } else if (strcmp(method, "tools/list") == 0) {
        return __handle_tools_list(node, id);
    } else if (strcmp(method, "tools/call") == 0) {
        return __handle_tools_call(node, id);
    } else {
        PR_ERR("Method not implemented: %s", method);
        return __reply_error(id, MCP_ERROR_METHOD_NOT_FOUND, "Method not implemented");
    }
}

/* === Utility Functions === */

OPERATE_RET ai_mcp_base64_encode(const VOID *input, uint32_t input_len,
                                   char *output, uint32_t output_len)
{
    if (!input || !output || input_len == 0 || output_len == 0)
        return OPRT_INVALID_PARM;

    if (TY_BASE64_BUF_LEN_CALC(input_len) > output_len)
        return OPRT_COM_ERROR;

    tuya_base64_encode(input, output, input_len);

    return OPRT_OK;
}
