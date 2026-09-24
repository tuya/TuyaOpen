#ifndef JIELI_MBEDTLS_CONFIG_H
#define JIELI_MBEDTLS_CONFIG_H

/* Keep the upstream feature defaults, but disable the POSIX/Windows-only
 * timing implementation for the embedded PI32V2 target. */
#include "mbedtls/mbedtls_config.h"

#undef MBEDTLS_TIMING_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_NET_C
/* The bundled Mbed TLS 3.1 tree does not include the native PSA ITS headers;
 * TuyaOpen uses the traditional Mbed TLS APIs on this platform. */
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C

#endif /* JIELI_MBEDTLS_CONFIG_H */
