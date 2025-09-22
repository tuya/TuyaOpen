/**
 * @file tal_kv.h
 * @brief Provides key-value storage functionality for Tuya applications.
 *
 * This header file defines the interface for Tuya's key-value (KV) storage
 * system, which is designed to serialize and deserialize data to and from JSON
 * format for efficient storage and retrieval. It supports various data types
 * including integers, booleans, strings, and raw binary data. The API
 * facilitates the initialization of the KV storage system, setting and getting
 * key-value pairs, deleting keys, and performing serialization and
 * deserialization of complex data structures. This is essential for
 * applications requiring persistent storage of configuration parameters or
 * other data across reboots or power cycles.
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
    char *key;    // property name
    kv_tp_t tp;   // property type
    void *val;    // property value
    uint16_t len; // property length
} kv_db_t;

#define TAL_LV_KEY_LEN 16

typedef struct {
    char seed[TAL_LV_KEY_LEN + 1];
    char key[TAL_LV_KEY_LEN + 1];
} tal_kv_cfg_t;

/**
 * @brief Initializes the TAL Key-Value (KV) module.
 *
 * This function initializes the TAL KV module with the provided configuration.
 *
 * @param kv_cfg A pointer to the TAL KV configuration structure.
 * @return An integer value indicating the status of the initialization process.
 *         Returns 0 on success, and a negative value on failure.
 */
int tal_kv_init(tal_kv_cfg_t *kv_cfg);

/**
 * @brief Sets the value of a key in the TAL Key-Value store.
 *
 * This function sets the value of a specified key in the TAL Key-Value store.
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
 * This function retrieves the value associated with the specified key from the
 * key-value store. The retrieved value is stored in the `value` parameter, and
 * its length is stored in the `length` parameter.
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
 * @brief Frees the memory allocated for a value in the TAL Key-Value store.
 *
 * This function frees the memory allocated for a value in the TAL Key-Value
 * store.
 *
 * @param value Pointer to the value to be freed.
 * @return 0 if the value was successfully freed, or a negative error code if an
 * error occurred.
 */
int tal_kv_free(uint8_t *value);

/**
 * @brief Deletes the specified key from the TAL Key-Value store.
 *
 * This function deletes the specified key from the TAL Key-Value store.
 *
 * @param key The key to be deleted.
 * @return 0 if the key was successfully deleted, or a negative error code if an
 * error occurred.
 */
int tal_kv_del(const char *key);

/**
 * @brief Serializes and sets the value of a key in the key-value database.
 *
 * This function serializes the provided key-value pair and sets it in the
 * specified key-value database.
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
 * This function serializes a key-value pair retrieval operation by taking the
 * specified key and database, and returns the number of key-value pairs found
 * in the database.
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
 * This function is responsible for executing the TAL KV command based on the
 * provided arguments.
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
lfs_t *tal_lfs_get();

#ifdef __cplusplus
}
#endif

#endif //__TIMER_INTERFACE_H_
