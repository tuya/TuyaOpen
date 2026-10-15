/**
 * @file tal_kv.c
 * @brief tal_kv 组件实现：LittleFS 介质 + AES-128-CBC 加密的键值存储，
 *        并为 core 提供 tal_kv 平台 ops 卡（tal_kv_ops_platform）
 *
 * 交付仓分层定位（C2）：
 *  - core（tuyaopen-core iot-core/tal_system）只持有 tal_kv 的 4 接口分发器
 *    （init/set/get/del → 透传 ops 卡），契约见平台 port/interface/tal_kv_ops.h；
 *    本文件是该卡的实现（此前由 platform/LINUX/port/tkl_kv.c 承担，现收编到本组件）。
 *  - 本组件属应用侧服务组件，可直接依赖 core：加密走 core 的 tal_security
 *    （tal_aes128_cbc_encode/decode + tal_sha256_ret，mbedtls 后端），
 *    平台侧不再需要 OpenSSL。
 *
 * 介质：平台 tkl_flash（linux 实现为文件模拟 ./tuyadb/tuyadb）的 UF 分区挂
 * LittleFS；每条记录 = 明文经 AES-128-CBC 加密后的密文，IV/密钥在 init 时由
 * seed/key 各经 SHA256 派生 16 字节，读回时逐字节校验 PKCS#7 填充。
 * 密文格式与 lfs 几何（block 4KB / UF 分区 96K）与原平台卡一致，字节兼容既有数据。
 *
 * 与上游（原 src/tal_kv）的差异：
 *  - 只保留 LittleFS 直连路径（本仓平台没有 tal_fs 卡，原 tal_fopen 门面不可用），
 *    kv_storage_* 改为直调 lfs_file_*；
 *  - 4 个公开 API 收为静态实现 + 导出 ops 卡（避免与 core 的同名符号冲突）；
 *  - 沿用原平台卡的安全加固：未挂载拒绝读写、重复 init 幂等（不重复 mount，
 *    避免失败路径上的 format 抹掉既有数据）。
 *  - tal_kv_free / tal_kv_cmd / tal_kv_serialize_* / tal_lfs_get 保持导出
 *    （tal_cli / authorize / audio_player 在用）。
 *
 * 注意：lfs_util.h 经 -DLFS_CONFIG=lfs_config.h（组件 CMake 注入）取 lfs_config.h。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include <limits.h>
#include <string.h>

#include "tal_kv.h"
#include "tal_api.h"
#include "tal_security.h"

#include "lfs.h"
#include "lfs_config.h"
#include "tkl_flash.h"

static lfs_t      lfs;
static lfs_size_t lfs_flash_addr;

static tal_kv_cfg_t lfs_kv_cfg;
static MUTEX_HANDLE lfs_mutex;
static BOOL_T       lfs_mounted = FALSE;

extern int kv_serialize(const kv_db_t *db, const uint32_t dbcnt, char **out, uint32_t *out_len);
extern int kv_deserialize(const char *in, kv_db_t *db, const uint32_t dbcnt);

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

/* Storage helpers: callers hold lfs_mutex for read, write, and remove. */
static OPERATE_RET kv_storage_init(void)
{
    /* lfs 内部持有 cfg 指针（lfs->cfg），配置结构须比挂载活得久 */
    static struct lfs_config lfs_cfg;
    TUYA_FLASH_BASE_INFO_T   info;
    int                      result;

    if (OPRT_OK != tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_UF, &info) || 0 == info.partition[0].block_size) {
        return OPRT_COM_ERROR;
    }

    lfs_flash_addr = info.partition[0].start_addr;
    memset(&lfs_cfg, 0, sizeof(lfs_cfg));
    lfs_cfg.read           = user_provided_block_device_read;
    lfs_cfg.prog           = user_provided_block_device_prog;
    lfs_cfg.erase          = user_provided_block_device_erase;
    lfs_cfg.sync           = user_provided_block_device_sync;
    lfs_cfg.read_size      = info.partition[0].block_size;
    lfs_cfg.prog_size      = info.partition[0].block_size;
    lfs_cfg.block_size     = info.partition[0].block_size;
    lfs_cfg.block_count    = info.partition[0].size / info.partition[0].block_size;
    lfs_cfg.cache_size     = info.partition[0].block_size;
    lfs_cfg.lookahead_size = lfs_cfg.block_count / 8 + (8 - (lfs_cfg.block_count / 8));
    lfs_cfg.block_cycles   = 500;

    result = lfs_mount(&lfs, &lfs_cfg);
    if (LFS_ERR_OK != result) {
        result = lfs_format(&lfs, &lfs_cfg);
        if (LFS_ERR_OK == result) {
            result = lfs_mount(&lfs, &lfs_cfg);
        }
    }

    return (LFS_ERR_OK == result) ? OPRT_OK : OPRT_COM_ERROR;
}

static OPERATE_RET kv_storage_write(const char *key, const uint8_t *data, uint32_t length)
{
    lfs_file_t file;
    int        result;
    int        sync_result;
    int        close_result;

    if (length > INT_MAX) {
        return OPRT_INVALID_PARM;
    }

    result = lfs_file_open(&lfs, &file, key, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (LFS_ERR_OK != result) {
        PR_ERR("lfs open %s failed: %d", key, result);
        return OPRT_FILE_OPEN_FAILED;
    }

    result       = lfs_file_write(&lfs, &file, data, (lfs_size_t)length);
    sync_result  = lfs_file_sync(&lfs, &file);
    close_result = lfs_file_close(&lfs, &file);
    if (result != (int)length || LFS_ERR_OK != sync_result || LFS_ERR_OK != close_result) {
        PR_ERR("kv write failed write:%d sync:%d close:%d", result, sync_result, close_result);
        return OPRT_KVS_WR_FAIL;
    }

    return OPRT_OK;
}

static OPERATE_RET kv_storage_read(const char *key, uint8_t **data, uint32_t *length)
{
    lfs_file_t file;
    lfs_soff_t stored_length;
    int        result;
    int        close_result;

    result = lfs_file_open(&lfs, &file, key, LFS_O_RDONLY);
    if (LFS_ERR_OK != result) {
        return (LFS_ERR_NOENT == result) ? OPRT_NOT_FOUND : OPRT_FILE_OPEN_FAILED;
    }

    stored_length = lfs_file_size(&lfs, &file);
    if (stored_length <= 0) {
        lfs_file_close(&lfs, &file);
        return OPRT_NOT_FOUND;
    }
    if (stored_length % 16 != 0 || stored_length > INT_MAX) {
        PR_ERR("invalid encrypted KV length %d for %s", (int)stored_length, key);
        lfs_file_close(&lfs, &file);
        return OPRT_KVS_RD_FAIL;
    }

    *data = tal_malloc((size_t)stored_length + 1U);
    if (NULL == *data) {
        lfs_file_close(&lfs, &file);
        return OPRT_MALLOC_FAILED;
    }

    result       = lfs_file_read(&lfs, &file, *data, (lfs_size_t)stored_length);
    close_result = lfs_file_close(&lfs, &file);
    if (result != (int)stored_length || LFS_ERR_OK != close_result) {
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
    int result = lfs_remove(&lfs, key);

    /* 契约（tal_kv.h）：键不存在视为成功 */
    return (LFS_ERR_OK == result || LFS_ERR_NOENT == result) ? OPRT_OK : OPRT_COM_ERROR;
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
    uint8_t     iv[16];
    uint8_t    *decrypted        = NULL;
    uint32_t    decrypted_length = 0;
    uint8_t     padding_length;
    uint32_t    i;
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

    *value_length            = decrypted_length - padding_length;
    decrypted[*value_length] = 0;
    *value                   = decrypted;
    return OPRT_OK;
}

/* ===== 平台 ops 卡的 4 个实现（core 的 tal_kv.c 经卡分发到此处） ===== */

static OPERATE_RET __kv_init(tal_kv_cfg_t *kv_cfg)
{
    uint8_t     sha256_ret[32];
    OPERATE_RET result;

    if (NULL == kv_cfg) {
        return OPRT_INVALID_PARM;
    }

    /* 密钥派生：每次 init 都按传入配置刷新 */
    memset(&lfs_kv_cfg, 0, sizeof(lfs_kv_cfg));
    tal_sha256_ret((const uint8_t *)kv_cfg->seed, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.seed, sha256_ret, TAL_LV_KEY_LEN);
    tal_sha256_ret((const uint8_t *)kv_cfg->key, TAL_LV_KEY_LEN, sha256_ret, 0);
    memcpy(lfs_kv_cfg.key, sha256_ret, TAL_LV_KEY_LEN);

    if (NULL == lfs_mutex) {
        result = tal_mutex_create_init(&lfs_mutex);
        if (OPRT_OK != result) {
            return result;
        }
    }

    /* 重复 init 幂等：介质已挂载就不再 mount（重复 mount 会泄漏 lfs 缓冲，
     * 且 mount 失败路径上的 format 会把已有数据抹掉） */
    if (TRUE == lfs_mounted) {
        return OPRT_OK;
    }

    result = kv_storage_init();
    if (OPRT_OK != result) {
        return result;
    }
    lfs_mounted = TRUE;

    return OPRT_OK;
}

static OPERATE_RET __kv_set(const char *key, const uint8_t *value, size_t length)
{
    uint8_t    *encrypted        = NULL;
    uint32_t    encrypted_length = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == value || 0 == length) {
        return OPRT_INVALID_PARM;
    }
    /* 未 init（介质未挂载）时不得触碰 lfs：lfs->cfg 为空会崩 */
    if (FALSE == lfs_mounted) {
        PR_ERR("kv not mounted, set %s rejected", key);
        return OPRT_COM_ERROR;
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

static OPERATE_RET __kv_get(const char *key, uint8_t **value, size_t *length)
{
    uint8_t    *encrypted        = NULL;
    uint32_t    encrypted_length = 0;
    OPERATE_RET result;

    /* 契约（tal_kv.h）：失败时 *value 置 NULL、*length 置 0。先于参数校验归零 */
    if (NULL != value) {
        *value = NULL;
    }
    if (NULL != length) {
        *length = 0;
    }

    if (NULL == key || NULL == value || NULL == length) {
        return OPRT_INVALID_PARM;
    }
    /* 未 init（介质未挂载）时不得触碰 lfs：lfs->cfg 为空会崩 */
    if (FALSE == lfs_mounted) {
        PR_ERR("kv not mounted, get %s rejected", key);
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(lfs_mutex);
    result = kv_storage_read(key, &encrypted, &encrypted_length);
    tal_mutex_unlock(lfs_mutex);
    if (OPRT_OK != result) {
        return result;
    }

    /* kv_decrypt 返回的明文缓冲（末尾已补 '\0'）直接交给调用者，
     * 由调用者 tal_free 释放；密文缓冲在此处释放 */
    result = kv_decrypt(encrypted, encrypted_length, value, length);
    tal_free(encrypted);
    if (OPRT_OK != result) {
        PR_ERR("key %s decrypt failed: %d", key, result);
    }

    return result;
}

static OPERATE_RET __kv_del(const char *key)
{
    OPERATE_RET result;

    if (NULL == key) {
        return OPRT_INVALID_PARM;
    }
    /* 未 init（介质未挂载）时不得触碰 lfs：lfs->cfg 为空会崩 */
    if (FALSE == lfs_mounted) {
        PR_ERR("kv not mounted, del %s rejected", key);
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(lfs_mutex);
    result = kv_storage_remove(key);
    tal_mutex_unlock(lfs_mutex);
    return result;
}

/* ---------------------------------------------------------------------------
 * tal_kv 平台 ops 卡：core 的 tal_kv.c（分发器）经 extern 引用完成分发。
 * --------------------------------------------------------------------------- */
TAL_KV_OPS_T tal_kv_ops_platform = {
    .init = __kv_init,
    .set  = __kv_set,
    .get  = __kv_get,
    .del  = __kv_del,
};

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
        __kv_set(argv[2], (const uint8_t *)argv[3], strlen(argv[3]));
    } else if (0 == strcmp("get", argv[1])) {
        uint8_t *buffer = NULL;
        size_t   length = 0;

        if (OPRT_OK == __kv_get(argv[2], &buffer, &length)) {
            PR_DEBUG("buffer %s", buffer);
            tal_kv_free(buffer);
        }
    } else if (0 == strcmp("del", argv[1])) {
        __kv_del(argv[2]);
    } else if (0 == strcmp("list", argv[1])) {
        lfs_dir_t       dir;
        struct lfs_info info;

        if (LFS_ERR_OK != lfs_dir_open(&lfs, &dir, "/")) {
            return;
        }
        while (lfs_dir_read(&lfs, &dir, &info) > 0) {
            if (LFS_TYPE_REG == info.type) {
                PR_DEBUG_RAW("%s  ", info.name);
            }
        }
        PR_DEBUG_RAW("\r\n");
        lfs_dir_close(&lfs, &dir);
    }
}

int tal_kv_serialize_set(const char *key, kv_db_t *db, size_t dbcnt)
{
    char       *buf = NULL;
    uint32_t    len = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == db || 0 == dbcnt) {
        return OPRT_INVALID_PARM;
    }

    result = kv_serialize(db, dbcnt, &buf, &len);
    if (OPRT_OK != result) {
        PR_ERR("kv_serialize failed: %d", result);
        return result;
    }

    result = __kv_set(key, (const uint8_t *)buf, len);
    tal_free(buf);
    return result;
}

int tal_kv_serialize_get(const char *key, kv_db_t *db, size_t dbcnt)
{
    uint8_t    *buf = NULL;
    size_t      len = 0;
    OPERATE_RET result;

    if (NULL == key || NULL == db || 0 == dbcnt) {
        return OPRT_INVALID_PARM;
    }

    result = __kv_get(key, &buf, &len);
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

lfs_t *tal_lfs_get(void)
{
    return &lfs;
}
