/**
 * @file ai_mcp_server.h
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
 * This is a pure C implementation of the MCP server. It provides JSON-RPC 2.0 
 * based communication for tool discovery and execution.
 * 
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */
#ifndef __AI_MCP_SERVER_H__
#define __AI_MCP_SERVER_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Maximum limits */
#define MCP_MAX_PROPERTIES      16
#define MCP_MAX_PAYLOAD_SIZE    8192

/* MCP protocol version */
#define MCP_PROTOCOL_VERSION    "2024-11-05"

/* Error codes following JSON-RPC 2.0 */
#define MCP_ERROR_PARSE         -32700
#define MCP_ERROR_INVALID_REQ   -32600
#define MCP_ERROR_METHOD_NOT_FOUND  -32601
#define MCP_ERROR_INVALID_PARAMS    -32602
#define MCP_ERROR_INTERNAL      -32603

#define MCP_IMAGE_MIME_TYPE_JPEG    "image/jpeg"
#define MCP_IMAGE_MIME_TYPE_PNG     "image/png"

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * MCP tool property types
 */
typedef enum {
    MCP_PROPERTY_TYPE_BOOLEAN = 0,
    MCP_PROPERTY_TYPE_INTEGER,
    MCP_PROPERTY_TYPE_STRING,
    MCP_PROPERTY_TYPE_MAX
} MCP_PROPERTY_TYPE_E;

/**
 * MCP tool return value types
 */
typedef enum {
    MCP_RETURN_TYPE_BOOLEAN = 0,
    MCP_RETURN_TYPE_INTEGER,
    MCP_RETURN_TYPE_STRING,
    MCP_RETURN_TYPE_JSON,
    MCP_RETURN_TYPE_IMAGE,
    MCP_RETURN_TYPE_MAX
} MCP_RETURN_TYPE_E;

/**
 * Property value union
 */
typedef struct {
    MCP_PROPERTY_TYPE_E type;
    union {
        bool bool_val;
        int int_val;
        char *str_val;
    };
} MCP_PROPERTY_VALUE_T;

/**
 * Tool property definition
 * @name: Property name
 * @type: Property type
 * @description: Property description (optional)
 * @has_default: Whether property has default value
 * @default_val: Default value
 * @has_range: Whether integer property has range limits
 * @min_val: Minimum value for integer properties
 * @max_val: Maximum value for integer properties
 */
typedef struct {
    char *name;
    MCP_PROPERTY_TYPE_E type;
    char *description;
    bool has_default;
    MCP_PROPERTY_VALUE_T default_val;
    bool has_range;
    int min_val;
    int max_val;
} MCP_PROPERTY_T;

/**
 * MCP_PROPERTY_LIST_T - List of tool properties
 * @properties: Array of properties
 * @count: Number of properties
 */
typedef struct {
    MCP_PROPERTY_T *properties[MCP_MAX_PROPERTIES];
    int count;
} MCP_PROPERTY_LIST_T;

/**
 * Tool return value
 * @type: Return value type
 * @data: Return value data (depends on type)
 */
typedef struct {
    MCP_RETURN_TYPE_E type;
    union {
        bool bool_val;
        int int_val;
        char *str_val;  /* Dynamically allocated */
        cJSON *json_val;  /* cJSON object */
        struct {
            char *mime_type;
            char *data;  /* Base64 encoded */
            uint32_t data_len;
        } image_val;
    };
} MCP_RETURN_VALUE_T;

/**
 * typedef MCP_TOOL_CALLBACK - Tool execution callback function
 * @properties: Input properties from client
 * @ret_val: Return value to be filled by callback
 * @user_data: User data passed during tool registration
 * 
 * Return: OPRT_OK on success, error code on failure
 */
typedef OPERATE_RET (*MCP_TOOL_CALLBACK)(const MCP_PROPERTY_LIST_T *properties,
                   MCP_RETURN_VALUE_T *ret_val,
                   void *user_data);

/**
 * MCP tool definition
 * @name: Tool name (Name is separated by dots is recommended, e.g., "device.audio.set_volume")
 * @description: Tool description
 * @properties: Tool input properties
 * @callback: Tool execution callback
 * @user_data: User data passed to callback
 * @next: Next tool in linked list
 */
typedef struct ai_mcp_tool_s {
    char *name;
    char *description;
    MCP_PROPERTY_LIST_T properties;
    MCP_TOOL_CALLBACK callback;
    void *user_data;
    struct ai_mcp_tool_s *next;
} MCP_TOOL_T;

typedef struct {
    const char *name;
    MCP_PROPERTY_TYPE_E type;
    const char *description;
    bool has_default;
    union {
        bool bool_val;
        int int_val;
        const char *str_val;
    } default_val;
    bool has_range;
    int min_val;
    int max_val;
} MCP_PROPERTY_DEF_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/* === Property Management Functions === */
#define MCP_PROP_END NULL

#define MCP_PROP_INT(name, desc) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = name, \
    .type = MCP_PROPERTY_TYPE_INTEGER, \
    .description = desc, \
    .has_default = FALSE, \
    .has_range = FALSE \
}

#define MCP_PROP_INT_DEF(name, desc, def_val)  \
    &(MCP_PROPERTY_DEF_T){ \
    .name = name, \
    .type = MCP_PROPERTY_TYPE_INTEGER, \
    .description = desc, \
    .has_default = TRUE, \
    .default_val.int_val = def_val, \
    .has_range = FALSE \
}

#define MCP_PROP_INT_RANGE(prop_name, desc, min, max) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_INTEGER, \
    .description = desc, \
    .has_default = FALSE, \
    .has_range = TRUE, \
    .min_val = min, \
    .max_val = max \
}

#define MCP_PROP_INT_DEF_RANGE(prop_name, desc, def_val, min, max) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_INTEGER, \
    .description = desc, \
    .has_default = TRUE, \
    .default_val.int_val = def_val, \
    .has_range = TRUE, \
    .min_val = min, \
    .max_val = max \
}

#define MCP_PROP_BOOL(prop_name, desc) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_BOOLEAN, \
    .description = desc, \
    .has_default = FALSE \
}

#define MCP_PROP_BOOL_DEF(prop_name, desc, def_val) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_BOOLEAN, \
    .description = desc, \
    .has_default = TRUE, \
    .default_val.bool_val = def_val \
}

#define MCP_PROP_STR(prop_name, desc) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_STRING, \
    .description = desc, \
    .has_default = FALSE \
}

#define MCP_PROP_STR_DEF(prop_name, desc, def_val) \
    &(MCP_PROPERTY_DEF_T){ \
    .name = prop_name, \
    .type = MCP_PROPERTY_TYPE_STRING, \
    .description = desc, \
    .has_default = TRUE, \
    .default_val.str_val = def_val \
}

/**
 * ai_mcp_property_create - Create a new property
 * @param[in] name Property name
 * @param[in] type Property type
 * @param[in] description Property description (optional)
 * 
 * Return: Pointer to the created property, or NULL on failure
 */
MCP_PROPERTY_T *ai_mcp_property_create(const char *name, MCP_PROPERTY_TYPE_E type, const char *description);

/**
 * ai_mcp_property_create_from_def - Create property from definition
 * @param[in] src Source property definition
 * 
 * Return: Pointer to the created property, or NULL on failure
 */
MCP_PROPERTY_T *ai_mcp_property_create_from_def(const MCP_PROPERTY_DEF_T *src);

/**
 * ai_mcp_property_destroy - Destroy a property
 * @param[in] prop Property to destroy
 * 
 * Frees all memory associated with the property.
 */
void ai_mcp_property_destroy(MCP_PROPERTY_T *prop);

/**
 * ai_mcp_property_set_default_bool - Set boolean default value
 * @param[in] prop Property to modify
 * @param[in] value Default boolean value
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_property_set_default_bool(MCP_PROPERTY_T *prop, bool value);

/**
 * ai_mcp_property_set_default_int - Set integer default value
 * @param[in] prop Property to modify
 * @param[in] value Default integer value
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_property_set_default_int(MCP_PROPERTY_T *prop, int value);

/**
 * ai_mcp_property_set_default_str - Set string default value
 * @param[in] prop Property to modify
 * @param[in] value Default string value
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_property_set_default_str(MCP_PROPERTY_T *prop, const char *value);

/**
 * ai_mcp_property_set_range - Set integer range limits
 * @param[in] prop Property to modify (must be integer type)
 * @param[in] min_val Minimum value
 * @param[in] max_val Maximum value
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_property_set_range(MCP_PROPERTY_T *prop, int min_val, int max_val);

/**
 * ai_mcp_property_dup - Duplicate a property
 * @param[in] src Source property to duplicate
 * 
 * Return: Pointer to the duplicated property, or NULL on failure
 */
MCP_PROPERTY_T *ai_mcp_property_dup(const MCP_PROPERTY_T *src);

/**
 * ai_mcp_property_list_create - Create a new property list with initialization
 * @return: Pointer to the created property list, or NULL on failure
 */
MCP_PROPERTY_LIST_T *ai_mcp_property_list_create(void);

/**
 * ai_mcp_property_list_init - Initialize property list
 * @param[in] list Property list to initialize
 */
void ai_mcp_property_list_init(MCP_PROPERTY_LIST_T *list);

/**
 * ai_mcp_property_list_destroy - Destroy property list
 * @param[in] list Property list to destroy
 * 
 * Frees all memory associated with the property list.
 */
void ai_mcp_property_list_destroy(MCP_PROPERTY_LIST_T *list);

/**
 * ai_mcp_property_list_add - Add property to list
 * @param[in] list Property list
 * @param[in] prop Property to add
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_property_list_add(MCP_PROPERTY_LIST_T *list, MCP_PROPERTY_T *prop);

/**
 * ai_mcp_property_list_find - Find property by name
 * @param[in] list Property list
 * @param[in] name Property name to find
 * 
 * Return: Pointer to property if found, NULL otherwise
 */
const MCP_PROPERTY_T *ai_mcp_property_list_find(const MCP_PROPERTY_LIST_T *list, const char *name);

/**
 * ai_mcp_property_list_dup - Duplicate a property list
 * @param[in] src Source property list to duplicate
 * 
 * Return: Pointer to the duplicated property list, or NULL on failure
 */
MCP_PROPERTY_LIST_T *ai_mcp_property_list_dup(const MCP_PROPERTY_LIST_T *src);

/* === Return Value Management Functions === */

/**
 * ai_mcp_return_value_init - Initialize return value
 * @param[in] ret_val Return value to initialize
 * @param[in] type Return value type
 */
void ai_mcp_return_value_init(MCP_RETURN_VALUE_T *ret_val, MCP_RETURN_TYPE_E type);

/**
 * ai_mcp_return_value_set_bool - Set boolean return value
 * @param[in] ret_val Return value
 * @param[in] value Boolean value
 */
void ai_mcp_return_value_set_bool(MCP_RETURN_VALUE_T *ret_val, bool value);

/**
 * ai_mcp_return_value_set_int - Set integer return value
 * @param[in] ret_val Return value
 * @param[in] value Integer value
 */
void ai_mcp_return_value_set_int(MCP_RETURN_VALUE_T *ret_val, int value);

/**
 * ai_mcp_return_value_set_str - Set string return value
 * @param[in] ret_val Return value
 * @param[in] value String value (will be duplicated)
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_return_value_set_str(MCP_RETURN_VALUE_T *ret_val, const char *value);

/**
 * ai_mcp_return_value_set_json - Set JSON return value
 * @param[in] ret_val Return value
 * @param[in] json cJSON object (ownership transferred to return value)
 */
void ai_mcp_return_value_set_json(MCP_RETURN_VALUE_T *ret_val, cJSON *json);

/**
 * ai_mcp_return_value_set_image - Set image return value
 * @param[in] ret_val Return value
 * @param[in] mime_type Image MIME type
 * @param[in] data Image data (will be base64 encoded)
 * @param[in] data_len Length of image data
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_return_value_set_image(MCP_RETURN_VALUE_T *ret_val,
                                            const char *mime_type,
                                            const void *data, uint32_t data_len);

/**
 * ai_mcp_return_value_cleanup - Clean up return value
 * @param[in] ret_val Return value to clean up
 * 
 * Frees any dynamically allocated memory in the return value.
 */
void ai_mcp_return_value_cleanup(MCP_RETURN_VALUE_T *ret_val);

/* === Tool Management Functions === */

/**
 * ai_mcp_tool_create - Create a new tool
 * @param[in] name Tool name
 * @param[in] description Tool description
 * @param[in] callback Tool execution callback
 * @param[in] user_data User data passed to callback
 * 
 * Return: Pointer to new tool, NULL on allocation failure
 */
MCP_TOOL_T *ai_mcp_tool_create(const char *name, const char *description,
                                 MCP_TOOL_CALLBACK callback, void *user_data);

/**
 * ai_mcp_tool_register - Helper function to create and register a new tool
 * @param[in] name Tool name
 * @param[in] description Tool description
 * @param[in] callback Tool execution callback
 * @param[in] user_data User data passed to callback
 * @param[in] ... Variable arguments of MCP_PROPERTY_DEF_T* ending with NULL
 *
 * Return: OPRT_OK on success, error code on failure 
 */
OPERATE_RET ai_mcp_tool_register(const char *name, const char *description,
                                 MCP_TOOL_CALLBACK callback, void *user_data, ...);

/**
 * AI_MCP_TOOL_ADD - Macro to create and register a new tool with properties
 * @param[in] name Tool name
 * @param[in] description Tool description
 * @param[in] callback Tool execution callback
 * @param[in] user_data User data passed to callback
 * @param[in] ... Variable arguments of MCP_PROPERTY_DEF_T* ending with MCP_PROP_END
 * 
 * This macro appends a NULL terminator to the variable arguments list.
 * 
 * Return: OPRT_OK on success, error code on failure
 */
#define AI_MCP_TOOL_ADD(name, description, callback, user_data, ...) \
    ai_mcp_tool_register(name, description, callback, user_data, ##__VA_ARGS__, MCP_PROP_END)

/**
 * ai_mcp_tool_destroy - Destroy a tool
 * @param[in] tool Tool to destroy
 */
void ai_mcp_tool_destroy(MCP_TOOL_T *tool);

/**
 * ai_mcp_tool_add_property - Add property to tool
 * @param[in] tool Tool to modify
 * @param[in] prop Property to add
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_tool_add_property(MCP_TOOL_T *tool, MCP_PROPERTY_T *prop);

/* === Server Management Functions === */

/**
 * ai_mcp_server_init - Initialize MCP server
 * @param[in] name Server name (board name)
 * @param[in] version Server version
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_server_init(const char *name, const char *version);

/**
 * ai_mcp_server_destroy - Destroy MCP server
 */
void ai_mcp_server_destroy(void);

/**
 * ai_mcp_server_add_tool - Add tool to server
 * @param[in] tool Tool to add (ownership transferred to server)
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_server_add_tool(MCP_TOOL_T *tool);

/**
 * ai_mcp_server_find_tool - Find tool by name
 * @param[in] name Tool name
 * 
 * Return: Pointer to tool if found, NULL otherwise
 */
MCP_TOOL_T *ai_mcp_server_find_tool(const char *name);

/**
 * ai_mcp_server_parse_message - Parse and handle MCP message
 * @param[in] json JSON-RPC message object
 * @param[in] user_data User data (currently unused)
 * 
 * This function parses the incoming JSON-RPC message and handles
 * initialize, tools/list, and tools/call methods.
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_server_parse_message(const cJSON *json, void *user_data);

/* === Utility Functions === */

/**
 * ai_mcp_base64_encode - Base64 encode data
 * @param[in] input Input data
 * @param[in] input_len Length of input data
 * @param[out] output Output buffer (must be allocated by caller)
 * @param[in] output_len Length of output buffer
 * 
 * Return: OPRT_OK on success, error code on failure
 */
OPERATE_RET ai_mcp_base64_encode(const void *input, uint32_t input_len,
                                   char *output, uint32_t output_len);

/**
 * ai_mcp_property_to_json - Convert property to JSON schema
 * @param[in] prop Property to convert
 * 
 * Return: cJSON object representing the property schema, NULL on error
 */
cJSON *ai_mcp_property_to_json(const MCP_PROPERTY_T *prop);

/**
 * ai_mcp_property_list_to_json - Convert property list to JSON schema
 * @param[in] list Property list to convert
 * 
 * Return: cJSON object representing the properties schema, NULL on error
 */
cJSON *ai_mcp_property_list_to_json(const MCP_PROPERTY_LIST_T *list);

/**
 * ai_mcp_tool_to_json - Convert tool to JSON representation
 * @param[in] tool Tool to convert
 * 
 * Return: cJSON object representing the tool, NULL on error
 */
cJSON *ai_mcp_tool_to_json(const MCP_TOOL_T *tool);

/**
 * ai_mcp_return_value_to_json - Convert return value to JSON result
 * @param[in] ret_val Return value to convert
 * 
 * Return: cJSON object representing the MCP result, NULL on error
 */
cJSON *ai_mcp_return_value_to_json(const MCP_RETURN_VALUE_T *ret_val);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MCP_SERVER_H__ */
