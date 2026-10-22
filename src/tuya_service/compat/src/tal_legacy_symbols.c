/**
 * @file tal_legacy_symbols.c
 * @brief Legacy tal_* symbols referenced by platform prebuilt libraries.
 *
 * The prebuilt libraries keep the tal_* names; each wrapper forwards to the
 * core_* implementation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_compat.h"

#include "core_log.h"
#include "core_memory.h"
#include "core_symmetry.h"

#undef tal_malloc
#undef tal_free
#undef tal_log_hex_dump
#undef tal_aes128_ecb_encode
#undef tal_aes128_ecb_decode
#undef tal_aes_get_actual_length

void *tal_malloc(size_t size)
{
    return core_malloc(size);
}

void tal_free(void *ptr)
{
    core_free(ptr);
}

void tal_log_hex_dump(const CORE_LOG_LEVEL_E level, const char *file, const int line, const char *title, uint8_t width,
                      uint8_t *buf, uint16_t size)
{
    core_log_hex_dump(level, file, line, title, width, buf, size);
}

OPERATE_RET tal_aes128_ecb_encode(uint8_t *data, uint32_t len, uint8_t **ec_data, uint32_t *ec_len, uint8_t *key)
{
    return core_aes128_ecb_encode(data, len, ec_data, ec_len, key);
}

OPERATE_RET tal_aes128_ecb_decode(uint8_t *data, uint32_t len, uint8_t **dec_data, uint32_t *dec_len, uint8_t *key)
{
    return core_aes128_ecb_decode(data, len, dec_data, dec_len, key);
}

int32_t tal_aes_get_actual_length(uint8_t *dec_data, uint32_t dec_data_len)
{
    return core_aes_get_actual_length(dec_data, dec_data_len);
}
