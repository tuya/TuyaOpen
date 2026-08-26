/**
 * @file tuya_entropy_poll.c
 * @brief mbedTLS hardware entropy source, backed by the platform RNG.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"

/* Platforms using the SDK-provided mbedtls bring their own mbedtls_hardware_poll. */
#if !defined(ENABLE_PLATFORM_MBEDTLS)

#include "tal_system.h"
#include "mbedtls/entropy.h"

// Filling one accumulator block clears the entropy_func threshold; asking for more is waste.
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    size_t want = (len < MBEDTLS_ENTROPY_BLOCK_SIZE) ? len : MBEDTLS_ENTROPY_BLOCK_SIZE;
    size_t done = 0;

    (void)data;

    while (done < want) {
        uint32_t word = (uint32_t)tal_system_get_random(0xFFFFFFFF);
        size_t   n    = ((want - done) < sizeof(word)) ? (want - done) : sizeof(word);

        memcpy(output + done, &word, n);
        done += n;
    }

    *olen = done;

    return 0;
}

#endif /* !ENABLE_PLATFORM_MBEDTLS */
