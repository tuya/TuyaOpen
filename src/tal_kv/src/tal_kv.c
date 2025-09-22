/**
 * @file tal_kv.c
 * @brief Key-Value storage implementation using LittleFS for Tuya devices.
 *
 * This file contains the implementation of the key-value storage system for
 * Tuya devices, utilizing the LittleFS filesystem. It provides low-level block
 * device operations for reading and programming (writing) data to the flash
 * memory. The implementation includes initialization of the filesystem,
 * configuration of the key-value storage, and thread-safe access through mutex
 * locking. This allows for reliable storage and retrieval of configuration
 * parameters and other data in a structured and efficient manner.
 *
 * The functions `user_provided_block_device_read` and
 * `user_provided_block_device_prog` are specifically designed to interface with
 * the underlying flash storage hardware, using Tuya's hardware abstraction
 * layer (HAL) for flash operations. This ensures compatibility and optimal
 * performance across different Tuya devices and platforms.
 *
 * @note This file is part of the Tuya SDK and is intended for use in Tuya-based
 * applications. It requires the LittleFS library and Tuya's hardware
 * abstraction libraries for proper functionality.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_kv.h"
#include "lfs_config.h"
#include "tkl_flash.h"
#include "tal_api.h"
#include "tal_security.h"

// variables used by the filesystem
static lfs_t lfs;
static lfs_size_t lfs_flash_addr;
static tal_kv_cfg_t lfs_kv_cfg;
static MUTEX_HANDLE lfs_mutex;

extern int kv_serialize(const kv_db_t *db, const uint32_t dbcnt, char **out, uint32_t *out_len);
extern int kv_deserialize(const char *in, kv_db_t *db, const uint32_t dbcnt);

/**
 * Reads data from a user-provided block device.
 *
 * This function reads data from a block device specified by the provided
 * configuration.
 *
 * @param c The configuration of the block device.
 * @param block The block number to read from.
 * @param off The offset within the block to start reading from.
 * @param buffer The buffer to store the read data.
 * @param size The number of bytes to read.
 * @return Returns LFS_ERR_OK if the read operation is successful, otherwise
 * returns LFS_ERR_IO.
 */
int user_provided_block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer,
                                    lfs_size_t size)
{

    OPERATE_RET ret = tkl_flash_read(lfs_flash_addr + c->block_size * block + off, buffer, size);
    if (OPRT_OK != ret) {
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

/**
 * @brief Writes data to a block device at a specific location.
 *
 * This function is a user-provided implementation of the block device program
 * operation for the Little File System (LFS). It writes the specified data
 * buffer to the block device at the given block and offset.
 *
 * @param c         Pointer to the LFS configuration struct.
 * @param block     The block number to write to.
 * @param off       The offset within the block to start writing at.
 * @param buffer    Pointer to the data buffer to write.
 * @param size      The size of the data to write.
 * @return          Returns LFS_ERR_OK if the write operation is successful,
 *                  or LFS_ERR_IO if an error occurs during the write operation.
 */
int user_provided_block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                                    lfs_size_t size)
{

    OPERATE_RET ret = tkl_flash_write(lfs_flash_addr + c->block_size * block + off, buffer, size);
    if (OPRT_OK != ret) {
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

/**
 * Erases a block on the user-provided block device.
 *
 * This function erases a block on the user-provided block device using the
 * specified configuration.
 *
 * @param c The configuration of the block device.
 * @param block The block number to erase.
 * @return Returns LFS_ERR_OK if the block erase operation is successful,
 * otherwise returns LFS_ERR_IO.
 */
int user_provided_block_device_erase(const struct lfs_config *c, lfs_block_t block)
{

    OPERATE_RET ret = tkl_flash_erase(lfs_flash_addr + c->block_size * block, c->block_size);
    if (OPRT_OK != ret) {
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

/**
 * @brief User-provided block device sync function.
 *
 * This function is called by the LittleFS library to synchronize the block
 * device. It should be implemented by the user according to their specific
 * block device implementation.
 *
 * @param c Pointer to the LittleFS configuration struct.
 * @return Returns LFS_ERR_OK if the synchronization is successful.
 */
int user_provided_block_device_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}

/**
 * @brief Initializes the TAL Key-Value (KV) module.
 *
 * This function initializes the TAL KV module with the provided configuration.
 *
 * @param kv_cfg A pointer to the TAL KV configuration structure.
 * @return An integer value indicating the status of the initialization process.
 *         Returns 0 on success, and a negative value on failure.
 */
int tal_kv_init(tal_kv_cfg_t *kv_cfg)
{
    uint8_t sha256_ret[32];

    //! init flash key
    memset(&lfs_kv_cfg, 0, sizeof(lfs_kv_cfg));
    tal_sha256_ret((const uint8_t *)kv_cfg->seed, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.seed, sha256_ret, TAL_LV_KEY_LEN);
    tal_sha256_ret((const uint8_t *)kv_cfg->key, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.key, sha256_ret, TAL_LV_KEY_LEN);

    tal_mutex_create_init(&lfs_mutex);

    TUYA_FLASH_BASE_INFO_T info;
    tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_UF, &info);
    lfs_flash_addr = info.partition[0].start_addr;

    static struct lfs_config lfs_cfg = {0};

    memset(&lfs_cfg, 0, sizeof(lfs_cfg));

    lfs_cfg.read = user_provided_block_device_read;
    lfs_cfg.prog = user_provided_block_device_prog;
    lfs_cfg.erase = user_provided_block_device_erase;
    lfs_cfg.sync = user_provided_block_device_sync;
    lfs_cfg.read_size = info.partition[0].block_size;
    lfs_cfg.prog_size = info.partition[0].block_size;
    lfs_cfg.block_size = info.partition[0].block_size;
    lfs_cfg.block_count = info.partition[0].size / info.partition[0].block_size;
    lfs_cfg.cache_size = info.partition[0].block_size;
    lfs_cfg.lookahead_size = lfs_cfg.block_count / 8 + (8 - (lfs_cfg.block_count / 8));
    lfs_cfg.block_cycles = 500;

    // mount the filesystem
    int err = lfs_mount(&lfs, &lfs_cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        lfs_format(&lfs, &lfs_cfg);
        err = lfs_mount(&lfs, &lfs_cfg);
    }

    return err;
}

/**
 * @brief Sets a key-value pair in the key-value store.
 *
 * This function sets a key-value pair in the key-value store. The key is a
 * string, the value is a byte array, and the length specifies the number of
 * bytes in the value.
 *
 * @param key The key to set in the key-value store.
 * @param value The value to associate with the key.
 * @param length The length of the value in bytes.
 * @return Returns OPRT_OK if the key-value pair is set successfully, or an
 * error code if an error occurs.
 */
int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    int result;
    lfs_file_t file;

    PR_DEBUG("key:%s, len %d", key, length);

    if (NULL == key || NULL == value || 0 == length) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(lfs_mutex);
    result = lfs_file_open(&lfs, &file, key, LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC);
    if (LFS_ERR_OK != result) {
        tal_mutex_unlock(lfs_mutex);
        PR_ERR("lfs open %s err", key);
        return result;
    }
    uint8_t *ec_data = NULL;
    uint32_t ec_len = 0;
    uint8_t iv[16];

    memcpy(iv, lfs_kv_cfg.seed, 16);
    result =
        tal_aes128_cbc_encode((uint8_t *)value, length, (uint8_t *)lfs_kv_cfg.key, iv, &ec_data, (uint32_t *)&ec_len);
    if (OPRT_OK != result) {
        lfs_file_close(&lfs, &file);
        tal_mutex_unlock(lfs_mutex);
        PR_DEBUG("key %s encrypt failed", key);
        return result;
    }
    lfs_file_rewind(&lfs, &file);
    result = lfs_file_write(&lfs, &file, ec_data, ec_len);
    lfs_file_close(&lfs, &file);
    tal_aes_free_data(ec_data);
    tal_mutex_unlock(lfs_mutex);
    if (result != ec_len) {
        PR_ERR("kv write fail %d", result);
        return OPRT_KVS_WR_FAIL;
    }

    return OPRT_OK;
}

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
int tal_kv_get(const char *key, uint8_t **value, size_t *length)
{
    int result;
    lfs_file_t file;

    if (NULL == key || NULL == value || NULL == length) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(lfs_mutex);
    result = lfs_file_open(&lfs, &file, key, LFS_O_RDONLY);
    if (LFS_ERR_OK != result) {
        PR_ERR("lfs open %s %d err", key, result);
        tal_mutex_unlock(lfs_mutex);
        return result;
    }
    uint8_t *ec_data = NULL;
    uint32_t ec_len = lfs_file_size(&lfs, &file);

    ec_data = tal_malloc(ec_len + 1);
    if (NULL == ec_data) {
        result = OPRT_MALLOC_FAILED;
        tal_mutex_unlock(lfs_mutex);
        return result;
    }
    PR_DEBUG("key:%s, len:%d", key, ec_len);
    result = lfs_file_read(&lfs, &file, ec_data, ec_len);
    lfs_file_close(&lfs, &file);
    tal_mutex_unlock(lfs_mutex);
    if (result <= 0) {
        *length = 0;
        tal_free(ec_data);
        PR_ERR("kv read error %d", result);
        return OPRT_KVS_RD_FAIL;
    }
    uint8_t *dec_data = NULL;
    uint32_t dec_len = 0;
    uint8_t iv[16];

    memcpy(iv, lfs_kv_cfg.seed, 16);
    result = tal_aes128_cbc_decode(ec_data, ec_len, (uint8_t *)lfs_kv_cfg.key, iv, &dec_data, (uint32_t *)&dec_len);
    dec_len = tal_aes_get_actual_length(dec_data, dec_len);
    tal_free(ec_data);
    if (OPRT_OK != result || dec_len > ec_len) {
        PR_ERR("key %s decrypt failed %d, %d-%d", key, result, dec_len, ec_len);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    *value = dec_data;
    *length = (size_t)dec_len;
    dec_data[dec_len] = 0;

    return OPRT_OK;
}

/**
 * @brief Deletes the specified key from the TAL Key-Value store.
 *
 * This function deletes the specified key from the TAL Key-Value store.
 *
 * @param key The key to be deleted.
 * @return 0 if the key was successfully deleted, or a negative error code if an
 * error occurred.
 */
int tal_kv_del(const char *key)
{
    PR_DEBUG("key:%s", key);

    tal_mutex_lock(lfs_mutex);
    int result = lfs_remove(&lfs, key);
    tal_mutex_unlock(lfs_mutex);
    if (LFS_ERR_OK == result) {
        PR_DEBUG("Deleted successfully");
        return OPRT_OK;
    }

    PR_DEBUG("Deleted failed %d", result);

    return OPRT_COM_ERROR;
}

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
int tal_kv_free(uint8_t *value)
{
    if (NULL == value) {
        return OPRT_INVALID_PARM;
    }
    tal_free(value);

    return OPRT_OK;
}

/**
 * @brief Executes the TAL KV command.
 *
 * This function is responsible for executing the TAL KV command based on the
 * provided arguments.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings containing the command-line arguments.
 */
void tal_kv_cmd(int argc, char *argv[])
{
    if (argc < 3) {
        return;
    }

    if (0 == strcmp("set", argv[1])) {
        tal_kv_set(argv[2], (const uint8_t *)argv[3], strlen(argv[3]));
    } else if (0 == strcmp("get", argv[1])) {
        uint8_t *buffer;
        size_t length;
        if (OPRT_OK == tal_kv_get(argv[2], &buffer, &length)) {
            PR_DEBUG("buffer %s", buffer);
            tal_kv_free(buffer);
        }
    } else if (0 == strcmp("del", argv[1])) {
        tal_kv_del(argv[2]);
    } else if (0 == strcmp("list", argv[1])) {
        lfs_dir_t dir;
        lfs_dir_open(&lfs, &dir, argv[2]);
        struct lfs_info info;
        while (lfs_dir_read(&lfs, &dir, &info) > 0) {
            PR_DEBUG_RAW("%s  ", info.name);
        }
        PR_DEBUG_RAW("\r\n", info.name);
        lfs_dir_close(&lfs, &dir);
    }
}

/**
 * @brief Serializes and sets key-value data in the key-value database.
 *
 * This function serializes the key-value data in the provided database and sets
 * it using the specified key.
 *
 * @param key The key to set the serialized data.
 * @param db Pointer to the key-value database.
 * @param dbcnt The number of key-value pairs in the database.
 * @return Returns OPRT_OK if the operation is successful, otherwise returns an
 * error code.
 */
int tal_kv_serialize_set(const char *key, kv_db_t *db, size_t dbcnt)
{
    if (NULL == db || 0 == dbcnt) {
        return OPRT_INVALID_PARM;
    }

    char *buf = NULL;
    uint32_t len = 0;
    int ret = OPRT_OK;

    ret = kv_serialize(db, dbcnt, &buf, &len);
    if (OPRT_OK != ret) {
        PR_ERR("kv_serialize  fail. %d", ret);
        return ret;
    }
    PR_TRACE("write buf:%s", buf);
    ret = tal_kv_set(key, (const uint8_t *)buf, len);
    tal_free(buf);
    if (OPRT_OK != ret) {
        PR_ERR("kv_set fails %s %d", key, ret);
    }

    return ret;
}

/**
 * @brief Serializes and retrieves the value associated with a given key from
 * the key-value database.
 *
 * This function serializes the value associated with the specified key and
 * retrieves it from the key-value database. The serialized value is then
 * deserialized and stored in the provided `db` array.
 *
 * @param key The key for which to retrieve the value.
 * @param db Pointer to the array where the deserialized value will be stored.
 * @param dbcnt The size of the `db` array.
 * @return Returns `OPRT_INVALID_PARM` if any of the input parameters are
 * invalid, or an error code indicating the success or failure of the operation.
 */
int tal_kv_serialize_get(const char *key, kv_db_t *db, size_t dbcnt)
{
    if (NULL == db || 0 == dbcnt || NULL == key) {
        return OPRT_INVALID_PARM;
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    int ret = OPRT_OK;

    ret = tal_kv_get(key, &buf, &len);
    if (OPRT_OK != ret) {
        PR_ERR("kv_get fails %s %d", key, ret);
        return ret;
    }
    ret = kv_deserialize((char *)buf, db, dbcnt);
    tal_free(buf);
    if (OPRT_OK != ret) {
        PR_ERR("kv_deserialize fail. %d", ret);
    }

    return ret;
}

/**
 * @brief Get the LFS handle, can be used for file system opeation
 *
 * @return lfs_t *
 */
lfs_t *tal_lfs_get()
{
    return &lfs;
}