/**
 * @file tal_kv.c
 * @brief Thread-safe encrypted key-value storage.
 *
 * The public KV flow is independent from the storage implementation.  The
 * platform file-system and LittleFS differences are contained in the
 * kv_storage_* helpers below.
 */

#include <limits.h>
#include "tal_kv.h"
#include "tal_api.h"
#include "tal_security.h"

#include "tal_fs.h"

#if !(defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1))
#include "lfs_config.h"
#include "tkl_flash.h"

static lfs_t lfs;
static lfs_size_t lfs_flash_addr;
#endif

static tal_kv_cfg_t lfs_kv_cfg;
static MUTEX_HANDLE lfs_mutex;

extern int kv_serialize(const kv_db_t *db, const uint32_t dbcnt, char **out, uint32_t *out_len);
extern int kv_deserialize(const char *in, kv_db_t *db, const uint32_t dbcnt);

#if !(defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1))
static int user_provided_block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer,
                                           lfs_size_t size)
{
    OPERATE_RET ret = tkl_flash_read(lfs_flash_addr + c->block_size * block + off, buffer, size);
    return (OPRT_OK == ret) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int user_provided_block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                                           const void *buffer, lfs_size_t size)
{
    OPERATE_RET ret = tkl_flash_write(lfs_flash_addr + c->block_size * block + off, buffer, size);
    return (OPRT_OK == ret) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int user_provided_block_device_erase(const struct lfs_config *c, lfs_block_t block)
{
    OPERATE_RET ret = tkl_flash_erase(lfs_flash_addr + c->block_size * block, c->block_size);
    return (OPRT_OK == ret) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int user_provided_block_device_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}
#endif

/* Storage helpers: callers hold lfs_mutex for read, write, and remove. */
static OPERATE_RET kv_storage_init(void)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return OPRT_OK;
#else
    TUYA_FLASH_BASE_INFO_T info;
    static struct lfs_config lfs_cfg;
    int result;

    if (OPRT_OK != tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_UF, &info) || 0 == info.partition[0].block_size) {
        return OPRT_COM_ERROR;
    }

    lfs_flash_addr = info.partition[0].start_addr;
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

    result = lfs_mount(&lfs, &lfs_cfg);
    if (LFS_ERR_OK != result) {
        result = lfs_format(&lfs, &lfs_cfg);
        if (LFS_ERR_OK == result) {
            result = lfs_mount(&lfs, &lfs_cfg);
        }
    }

    return (LFS_ERR_OK == result) ? OPRT_OK : OPRT_COM_ERROR;
#endif
}

static OPERATE_RET kv_storage_write(const char *key, const uint8_t *data, uint32_t length)
{
    TUYA_FILE file;
    int result;
    int sync_result;
    int close_result;

    if (length > INT_MAX) {
        return OPRT_INVALID_PARM;
    }

    file = tal_fopen(key, "wb+");
    if (NULL == file) {
        PR_ERR("fs open %s failed", key);
        return OPRT_FILE_OPEN_FAILED;
    }

    result = tal_fwrite((void *)data, (int)length, file);
    sync_result = tal_fsync(file);
    close_result = tal_fclose(file);
    if (result != (int)length || OPRT_OK != sync_result || OPRT_OK != close_result) {
        PR_ERR("kv write failed write:%d sync:%d close:%d", result, sync_result, close_result);
        return OPRT_KVS_WR_FAIL;
    }

    return OPRT_OK;
}

static OPERATE_RET kv_storage_read(const char *key, uint8_t **data, uint32_t *length)
{
    TUYA_FILE file;
    int stored_length;
    int result;
    int close_result;

    stored_length = tal_fgetsize(key);
    if (stored_length <= 0) {
        return OPRT_NOT_FOUND;
    }
    if (stored_length % 16 != 0 || stored_length == INT_MAX) {
        PR_ERR("invalid encrypted KV length %d for %s", stored_length, key);
        return OPRT_KVS_RD_FAIL;
    }

    file = tal_fopen(key, "rb");
    if (NULL == file) {
        PR_ERR("fs open %s failed", key);
        return OPRT_FILE_OPEN_FAILED;
    }

    *data = tal_malloc((size_t)stored_length + 1U);
    if (NULL == *data) {
        tal_fclose(file);
        return OPRT_MALLOC_FAILED;
    }

    result = tal_fread(*data, stored_length, file);
    close_result = tal_fclose(file);
    if (result != stored_length || OPRT_OK != close_result) {
        PR_ERR("kv read failed read:%d close:%d", result, close_result);
        tal_free(*data);
        *data = NULL;
        return OPRT_KVS_RD_FAIL;
    }

    *length = (uint32_t)stored_length;
    return OPRT_OK;
}

static OPERATE_RET kv_storage_remove(const char *key)
{
    return (OPRT_OK == tal_fs_remove(key)) ? OPRT_OK : OPRT_COM_ERROR;
}

static OPERATE_RET kv_encrypt(const uint8_t *value, size_t length, uint8_t **encrypted, uint32_t *encrypted_length)
{
    uint8_t iv[16];

    if (length > UINT32_MAX - 16U) {
        return OPRT_INVALID_PARM;
    }

    memcpy(iv, lfs_kv_cfg.seed, sizeof(iv));
    return tal_aes128_cbc_encode((uint8_t *)value, (uint32_t)length, (uint8_t *)lfs_kv_cfg.key, iv, encrypted,
                                 encrypted_length);
}

static OPERATE_RET kv_decrypt(uint8_t *encrypted, uint32_t encrypted_length, uint8_t **value, size_t *value_length)
{
    uint8_t iv[16];
    uint8_t *decrypted = NULL;
    uint32_t decrypted_length = 0;
    uint8_t padding_length;
    uint32_t i;
    OPERATE_RET result;

    if (0 == encrypted_length || encrypted_length % 16U != 0U) {
        return OPRT_KVS_RD_FAIL;
    }

    memcpy(iv, lfs_kv_cfg.seed, sizeof(iv));
    result = tal_aes128_cbc_decode(encrypted, encrypted_length, (uint8_t *)lfs_kv_cfg.key, iv, &decrypted,
                                   &decrypted_length);
    if (OPRT_OK != result) {
        return result;
    }

    if (0 == decrypted_length || decrypted_length % 16U != 0U) {
        tal_aes_free_data(decrypted);
        return OPRT_KVS_RD_FAIL;
    }

    padding_length = decrypted[decrypted_length - 1U];
    if (0 == padding_length || padding_length > 16U || padding_length >= decrypted_length) {
        tal_aes_free_data(decrypted);
        return OPRT_KVS_RD_FAIL;
    }
    for (i = 0; i < padding_length; ++i) {
        if (decrypted[decrypted_length - 1U - i] != padding_length) {
            tal_aes_free_data(decrypted);
            return OPRT_KVS_RD_FAIL;
        }
    }

    *value_length = decrypted_length - padding_length;
    decrypted[*value_length] = 0;
    *value = decrypted;
    return OPRT_OK;
}

int tal_kv_init(tal_kv_cfg_t *kv_cfg)
{
    uint8_t sha256_ret[32];
    OPERATE_RET result;

    if (NULL == kv_cfg) {
        return OPRT_INVALID_PARM;
    }

    memset(&lfs_kv_cfg, 0, sizeof(lfs_kv_cfg));
    tal_sha256_ret((const uint8_t *)kv_cfg->seed, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.seed, sha256_ret, TAL_LV_KEY_LEN);
    tal_sha256_ret((const uint8_t *)kv_cfg->key, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.key, sha256_ret, TAL_LV_KEY_LEN);

    result = tal_mutex_create_init(&lfs_mutex);
    if (OPRT_OK != result) {
        return result;
    }

    return kv_storage_init();
}

int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    uint8_t *encrypted = NULL;
    uint32_t encrypted_length = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == value || 0 == length) {
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("key:%s, len %u", key, (unsigned int)length);
    result = kv_encrypt(value, length, &encrypted, &encrypted_length);
    if (OPRT_OK != result) {
        PR_ERR("key %s encrypt failed: %d", key, result);
        return result;
    }

    tal_mutex_lock(lfs_mutex);
    result = kv_storage_write(key, encrypted, encrypted_length);
    tal_mutex_unlock(lfs_mutex);
    tal_aes_free_data(encrypted);
    return result;
}

int tal_kv_get(const char *key, uint8_t **value, size_t *length)
{
    uint8_t *encrypted = NULL;
    uint32_t encrypted_length = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == value || NULL == length) {
        return OPRT_INVALID_PARM;
    }
    *value = NULL;
    *length = 0;

    tal_mutex_lock(lfs_mutex);
    result = kv_storage_read(key, &encrypted, &encrypted_length);
    tal_mutex_unlock(lfs_mutex);
    if (OPRT_OK != result) {
        return result;
    }

    result = kv_decrypt(encrypted, encrypted_length, value, length);
    tal_free(encrypted);
    if (OPRT_OK != result) {
        PR_ERR("key %s decrypt failed: %d", key, result);
    }

    return result;
}

int tal_kv_del(const char *key)
{
    OPERATE_RET result;

    if (NULL == key) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(lfs_mutex);
    result = kv_storage_remove(key);
    tal_mutex_unlock(lfs_mutex);
    return result;
}

int tal_kv_free(uint8_t *value)
{
    if (NULL == value) {
        return OPRT_INVALID_PARM;
    }

    tal_free(value);
    return OPRT_OK;
}

void tal_kv_cmd(int argc, char *argv[])
{
    if (argc < 3) {
        return;
    }

    if (0 == strcmp("set", argv[1])) {
        if (argc < 4) {
            return;
        }
        tal_kv_set(argv[2], (const uint8_t *)argv[3], strlen(argv[3]));
    } else if (0 == strcmp("get", argv[1])) {
        uint8_t *buffer = NULL;
        size_t length = 0;

        if (OPRT_OK == tal_kv_get(argv[2], &buffer, &length)) {
            PR_DEBUG("buffer %s", buffer);
            tal_kv_free(buffer);
        }
    } else if (0 == strcmp("del", argv[1])) {
        tal_kv_del(argv[2]);
    } else if (0 == strcmp("list", argv[1])) {
        TUYA_DIR dir = NULL;
        TUYA_FILEINFO info = NULL;
        const char *name = NULL;

        if (OPRT_OK != tal_dir_open(argv[2], &dir)) {
            return;
        }
        while (OPRT_OK == tal_dir_read(dir, &info) && info) {
            if (OPRT_OK == tal_dir_name(info, &name) && name) {
                PR_DEBUG_RAW("%s  ", name);
            }
        }
        PR_DEBUG_RAW("\r\n");
        tal_dir_close(dir);
    }
}

int tal_kv_serialize_set(const char *key, kv_db_t *db, size_t dbcnt)
{
    char *buf = NULL;
    uint32_t len = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == db || 0 == dbcnt) {
        return OPRT_INVALID_PARM;
    }

    result = kv_serialize(db, dbcnt, &buf, &len);
    if (OPRT_OK != result) {
        PR_ERR("kv_serialize failed: %d", result);
        return result;
    }

    result = tal_kv_set(key, (const uint8_t *)buf, len);
    tal_free(buf);
    return result;
}

int tal_kv_serialize_get(const char *key, kv_db_t *db, size_t dbcnt)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == db || 0 == dbcnt) {
        return OPRT_INVALID_PARM;
    }

    result = tal_kv_get(key, &buf, &len);
    if (OPRT_OK != result) {
        PR_ERR("kv_get failed %s: %d", key, result);
        return result;
    }

    result = kv_deserialize((char *)buf, db, dbcnt);
    tal_kv_free(buf);
    if (OPRT_OK != result) {
        PR_ERR("kv_deserialize failed: %d", result);
    }

    return result;
}

#if !(defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1))
lfs_t *tal_lfs_get(void)
{
    return &lfs;
}
#endif