/**
 * @file tal_kv.h
 * @brief 交付仓 tal_kv 组件对外头（src/tuya_service/tal_kv）：上游风格的 KV 门面。
 *
 * 说明（交付仓分层，C2）：
 *  - KV 的 4 个公开 API（tal_kv_init/set/get/del）由 core 持有并分发
 *    （tuyaopen-core iot-core/tal_system/tal_kv.c → tal_kv_ops_platform），
 *    此处保留其声明：本头在 include 路径上先于 core 的 tal_kv.h 被命中，
 *    上游风格代码（#include "tal_kv.h"）据此拿到完整 API 面；
 *  - tal_kv_cfg_t / TAL_LV_KEY_LEN 来自契约层 tal_kv_ops.h（平台
 *    port/interface/，core 与组件共用的同一份定义，不再重复定义）；
 *  - 本头另导出组件侧扩展：kv_db_t 序列化（kv_serialize.c）、tal_kv_free、
 *    tal_kv_cmd（tal_cli 的 kv 命令）、tal_lfs_get（audio_player 等直接用
 *    lfs 的调用方）。
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TAL_KV_H__
#define __TAL_KV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "lfs.h"
/* 契约层（平台 port/interface/tal_kv_ops.h）：tal_kv_cfg_t / TAL_LV_KEY_LEN /
 * TAL_KV_OPS_T（core 与 port 共用类型，只此一处定义） */
#include "tal_kv_ops.h"

/**
 * @brief tuya key-value database type define, used for serialize/deserialize
 * data to json format
 *
 */
typedef uint8_t kv_tp_t;
#define KV_CHAR   0 // char, same as int, need 11+6 byte
#define KV_BYTE   1 // byte, same as int, need 11+6 byte
#define KV_SHORT  2 // short, same as int, need 11+6 byte
#define KV_USHORT 3 // unsigned short, same as int, need 11+6 byte
#define KV_INT    4 // int, need 11+6 byte when serialize to json format
#define KV_BOOL   5 // bool, need 6+6 byte when serialize to json format
#define KV_STRING 6 // string, need len+6 byte when serialize to json format
#define KV_RAW                                                                                                         \
    7 // raw, need len/3*4+len%3?4:0+6 (base64 encode) when serialize to json
      // format

/**
 * @brief tuya key-value database, used for serialize/deserialize data to json
 * format
 *
 */
typedef struct {
    char    *key; // property name
    kv_tp_t  tp;  // property type
    void    *val; // property value
    uint16_t len; // property length
} kv_db_t;

/* ---------------------------------------------------------------------------
 * KV 公开 API（core 的 tal_kv.h 为准，此处重复声明是为本头在 include 路径上
 * 遮蔽 core 头时调用方仍可见；实现见 core 的 tal_kv.c，经 ops 卡分发到本组件）
 * --------------------------------------------------------------------------- */

/**
 * @brief Initializes the TAL Key-Value (KV) module.
 *
 * @param kv_cfg A pointer to the TAL KV configuration structure.
 * @return An integer value indicating the status of the initialization process.
 *         Returns 0 on success, and a negative value on failure.
 */
int tal_kv_init(tal_kv_cfg_t *kv_cfg);

/**
 * @brief Sets the value of a key in the TAL Key-Value store.
 *
 * @param key The key to set the value for.
 * @param value A pointer to the value to be set.
 * @param length The length of the value in bytes.
 * @return 0 if the value was successfully set, or a negative error code if an
 * error occurred.
 */
int tal_kv_set(const char *key, const uint8_t *value, size_t length);

/**
 * @brief Retrieves the value associated with the specified key from the
 * key-value store.
 *
 * @param key The key to retrieve the value for.
 * @param value A pointer to a pointer that will store the retrieved value.
 * @param length A pointer to a variable that will store the length of the
 * retrieved value.
 *
 * @return 0 if the value was successfully retrieved, or a negative error code
 * if an error occurred.
 */
int tal_kv_get(const char *key, uint8_t **value, size_t *length);

/**
 * @brief Deletes the specified key from the TAL Key-Value store.
 *
 * @param key The key to be deleted.
 * @return 0 if the key was successfully deleted, or a negative error code if an
 * error occurred.
 */
int tal_kv_del(const char *key);

/* ---------------------------------------------------------------------------
 * 组件侧扩展
 * --------------------------------------------------------------------------- */

/**
 * @brief Frees the memory allocated for a value in the TAL Key-Value store.
 *
 * @param value Pointer to the value to be freed.
 * @return 0 if the value was successfully freed, or a negative error code if an
 * error occurred.
 */
int tal_kv_free(uint8_t *value);

/**
 * @brief Serializes and sets the value of a key in the key-value database.
 *
 * @param key The key to set in the database.
 * @param db A pointer to the key-value database.
 * @param dbcnt The size of the key-value database.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tal_kv_serialize_set(const char *key, kv_db_t *db, size_t dbcnt);

/**
 * @brief Serializes a key-value pair retrieval operation.
 *
 * @param key The key to retrieve.
 * @param db The database to search for the key.
 * @param dbcnt The number of key-value pairs found in the database.
 *
 * @return The number of key-value pairs found in the database.
 */
int tal_kv_serialize_get(const char *key, kv_db_t *db, size_t dbcnt);

/**
 * @brief Executes the TAL KV command.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings containing the command-line arguments.
 */
void tal_kv_cmd(int argc, char *argv[]);

/**
 * @brief Get the LFS handle, can be used for file system opeation
 *
 * @return lfs_t *
 */
lfs_t *tal_lfs_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_KV_H__ */
