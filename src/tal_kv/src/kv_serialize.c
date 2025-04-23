/**
 * @file kv_serialize.c
 * @brief Implements serialization of key-value pairs into a JSON string.
 *
 * This file contains the implementation of the kv_serialize function, which is
 * responsible for converting a database of key-value pairs into a JSON
 * formatted string. It supports various data types including integers,
 * booleans, strings, and raw binary data. The serialization process calculates
 * the necessary buffer size, iterates through the key-value pairs, and formats
 * them according to their type into a JSON string. This functionality is
 * crucial for storing configuration data in a human-readable and easily
 * transmittable format.
 *
 * The implementation utilizes the cJSON library for JSON serialization and
 * includes optimizations for memory usage and processing time, making it
 * suitable for resource-constrained environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include "tuya_cloud_types.h"
#include "tal_kv.h"
#include "tal_api.h"
#include "cJSON.h"
#include "mix_method.h"

/**
 * Serializes the key-value pairs in the given database into a JSON-formatted
 * string.
 *
 * @param db The pointer to the database containing the key-value pairs.
 * @param dbcnt The number of key-value pairs in the database.
 * @param out The pointer to store the serialized JSON string.
 * @param out_len The pointer to store the length of the serialized JSON string.
 * @return Returns OPRT_OK if serialization is successful, otherwise returns an
 * error code.
 */
int kv_serialize(const kv_db_t *db, const uint32_t dbcnt, char **out, uint32_t *out_len)
{
    int i = 0;
    // conut need buf size
    uint32_t len = 0;
    for (i = 0; i < dbcnt; i++) {
        // value len
        if (db[i].tp <= KV_INT) {
            len += 11 + 6;
        } else if (db[i].tp == KV_BOOL) {
            len += 6 + 6;
        } else if (db[i].tp == KV_STRING) {
            char *tmp = (char *)db[i].val;
            if (0 == tmp[0]) {
                len += db[i].len + 4 + 4; //"id":null,
            } else {
                len += db[i].len + 6; //"swv":"4.1.16",
            }

        } else if (db[i].tp == KV_RAW) {
            // len += (db[i].len / 3) * 4 + ((db[i].len % 3) ? 4 : 0) + 6;
            len += (db[i].len / 3) * 4 + ((db[i].len % 3) ? 4 : 0);
            len += (db[i].len ? 6 : 4 + 4);
        }

        // key len
        len += strlen(db[i].key) + 2; // 2 == ""
    }
    len += 3; // 3 == {}\0

    uint32_t offset = 0;
    char *buf = tal_malloc(len);
    if (NULL == buf) {
        PR_ERR("maloc fails %d", len);
        return OPRT_MALLOC_FAILED;
    }
    offset = sprintf(buf, "{");

    for (i = 0; i < dbcnt; i++) {
        // add key
        offset += sprintf(buf + offset, "\"%s\":", db[i].key);

        // add value
        switch (db[i].tp) {
        case KV_CHAR: {
            offset += sprintf(buf + offset, "%d", *((char *)(db[i].val)));
        } break;
        case KV_BYTE: {
            offset += sprintf(buf + offset, "%d", *((uint8_t *)(db[i].val)));
        } break;
        case KV_SHORT: {
            offset += sprintf(buf + offset, "%d", *((int16_t *)(db[i].val)));
        } break;
        case KV_USHORT: {
            offset += sprintf(buf + offset, "%d", *((uint16_t *)(db[i].val)));
        } break;
        case KV_INT: {
            offset += sprintf(buf + offset, "%d", *((int32_t *)(db[i].val)));
        } break;
        case KV_BOOL: {
            if (FALSE == *((BOOL_T *)(db[i].val))) {
                offset += sprintf(buf + offset, "false");
            } else {
                offset += sprintf(buf + offset, "true");
            }
        } break;

        case KV_STRING: {
            char *tmp = (char *)db[i].val;
            if (0 == tmp[0]) {
                offset += sprintf(buf + offset, "null");
            } else {
                offset += sprintf(buf + offset, "\"%s\"", (char *)db[i].val);
            }
        } break;

        case KV_RAW: {
            if (0 == db[i].len) {
                offset += sprintf(buf + offset, "null");
            } else {
                offset += sprintf(buf + offset, "\"");
                tuya_base64_encode((unsigned char *)db[i].val, (buf + offset), db[i].len);
                offset += strlen((buf + offset));
                buf[offset] = '\"';
                offset++;
            }
        } break;

        default: {
            PR_ERR("type invalid %d", db[i].tp);
            tal_free(buf);
            return OPRT_COM_ERROR;
        }
        }

        offset += sprintf(buf + offset, ",");
    }

    if (i != 0) {
        /* When exiting the loop, an extra ',' will be added at the end, so
         * reset it here */
        buf[offset - 1] = ' ';
    }

    buf[offset++] = '}';
    buf[offset] = 0;

    *out = buf;
    *out_len = offset;

    return OPRT_OK;
}

/**
 * @brief Deserialize a JSON string and populate a key-value database.
 *
 * This function takes a JSON string, parses it using cJSON library, and
 * populates a key-value database with the values extracted from the JSON. The
 * key-value database is represented by the `kv_db_t` structure array. The
 * number of elements in the `kv_db_t` array is specified by the `dbcnt`
 * parameter.
 *
 * @param[in] in The input JSON string to deserialize.
 * @param[in,out] db The key-value database to populate.
 * @param[in] dbcnt The number of elements in the key-value database.
 * @return Returns OPRT_OK if the deserialization is successful. Otherwise, it
 * returns an error code indicating the failure reason.
 */
int kv_deserialize(const char *in, kv_db_t *db, const uint32_t dbcnt)
{
    cJSON *root = cJSON_Parse(in);
    if (NULL == root) {
        PR_ERR("json parse fails %s", in);
        return OPRT_CJSON_PARSE_ERR;
    }

    int op_ret = OPRT_OK;
    int i = 0;
    cJSON *json = NULL;
    for (i = 0; i < dbcnt; i++) {
        json = cJSON_GetObjectItem(root, db[i].key);
        if (json != NULL) {
            if (db[i].tp <= KV_INT && (cJSON_Number != json->type)) {
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            } else if (db[i].tp == KV_BOOL && (json->type > cJSON_True)) {
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            } else if ((db[i].tp == KV_STRING || db[i].tp == KV_RAW) &&
                       (json->type != cJSON_String && json->type != cJSON_NULL)) {
                op_ret = OPRT_CJSON_GET_ERR;
                goto ERR_EXIT;
            }
        } else { // default set zero
            memset(db[i].val, 0, db[i].len);
            continue;
        }

        switch (db[i].tp) {
        case KV_CHAR: {
            if (json->valueint < -128 || json->valueint > 127) {
                op_ret = OPRT_COM_ERROR;
                goto ERR_EXIT;
            }
            *((char *)db[i].val) = json->valueint;
        } break;

        case KV_BYTE: {
            if (json->valueint < 0 || json->valueint > 255) {
                op_ret = OPRT_COM_ERROR;
                goto ERR_EXIT;
            }
            *((uint8_t *)db[i].val) = json->valueint;
        } break;

        case KV_SHORT: {
            if (json->valueint < -32768 || json->valueint > 32767) {
                op_ret = OPRT_COM_ERROR;
                goto ERR_EXIT;
            }
            *((int16_t *)db[i].val) = json->valueint;
        } break;

        case KV_USHORT: {
            if (json->valueint < 0 || json->valueint > 65535) {
                op_ret = OPRT_COM_ERROR;
                goto ERR_EXIT;
            }
            *((uint16_t *)db[i].val) = json->valueint;
        } break;

        case KV_INT: {
            *((int *)db[i].val) = json->valueint;
        } break;

        case KV_BOOL: {
            *((BOOL_T *)db[i].val) = (json->type == cJSON_False) ? 0 : 1;
        } break;

        case KV_STRING: {
            if (json->type == cJSON_NULL) {
                char *tmp = (char *)db[i].val;
                tmp[0] = 0;
            } else {
                uint32_t len = strlen(json->valuestring) + 1;
                if (db[i].len < len) {
                    op_ret = OPRT_COM_ERROR;
                    goto ERR_EXIT;
                }
                memcpy(db[i].val, json->valuestring, len);
            }
        } break;

        case KV_RAW: {
            if (json->type == cJSON_NULL) {
                db[i].len = 0;
            } else {
                tuya_base64_decode(json->valuestring, db[i].val);
            }
        } break;

        default: {
            PR_ERR("type invalid %d", db[i].tp);
            op_ret = OPRT_COM_ERROR;
            goto ERR_EXIT;
        }
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;

ERR_EXIT:
    cJSON_Delete(root);
    PR_ERR("deserial fails %d", op_ret);

    return op_ret;
}
