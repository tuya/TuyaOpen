/**
 * @file tal_x509.c
 * @brief Implementation of X.509 certificate utilities for Tuya SDK.
 *
 * This source file provides the implementation of functionalities related to
 * X.509 certificates within the Tuya SDK. It includes functions for checking if
 * a buffer contains a CA certificate in PEM format, converting PEM format
 * certificates to DER format, and other utilities that facilitate the handling
 * and manipulation of X.509 certificates. These functionalities are crucial for
 * secure communication and authentication in Tuya-based applications, ensuring
 * that devices and services can verify each other's identities through trusted
 * certificates.
 *
 * The implementation leverages the mbedtls library for parsing and handling
 * X.509 certificates, providing a robust and secure foundation for certificate
 * operations. This file is part of the Tuya IoT Development Platform and is
 * intended for use in Tuya-based applications, enhancing the security posture
 * of applications and devices within the Tuya ecosystem.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tal_x509.h"
#include "mbedtls/pem.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"

#define TUYA_X509_SELF_TEST 0

/**
 * @brief Checks if the given buffer contains a CA (Certificate Authority) in
 * PEM format.
 *
 * This function checks whether the provided buffer contains a CA certificate in
 * PEM (Privacy-Enhanced Mail) format. The buffer should contain the certificate
 * data, and the length of the buffer should be specified in the 'buflen'
 * parameter.
 *
 * @param buf Pointer to the buffer containing the certificate data.
 * @param buflen Length of the buffer in bytes.
 * @return BOOL_T Returns 'TRUE' if the buffer contains a CA certificate in PEM
 * format, 'FALSE' otherwise.
 */
BOOL_T tuya_x509_is_ca_pem_format(uint8_t *buf, uint32_t buflen)
{
    if (buflen != 0 && buf[buflen - 1] == '\0' && strstr((const char *)buf, "-----BEGIN CERTIFICATE-----") != NULL) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Converts a PEM-encoded X.509 certificate to DER format.
 *
 * This function takes a buffer containing a PEM-encoded X.509 certificate and
 * converts it to DER format. The converted DER certificate is stored in the
 * output buffer provided by the caller.
 *
 * @param buf Pointer to the buffer containing the PEM-encoded certificate.
 * @param buflen Length of the buffer containing the PEM-encoded certificate.
 * @param out Pointer to the output buffer where the converted DER certificate
 * will be stored.
 * @param outlen Pointer to the variable that will hold the length of the
 * converted DER certificate.
 *
 * @return OPERATE_RET Returns OPRT_OK if the conversion is successful, or an
 * error code if it fails.
 */
OPERATE_RET tuya_x509_pem2der(uint8_t *buf, uint32_t buflen, uint8_t **out, uint32_t *outlen)
{
    OPERATE_RET rt = OPRT_OK;
#if defined(MBEDTLS_PEM_PARSE_C)
    int32_t ret;
    mbedtls_pem_context pem;
    if (buf == NULL || out == NULL || buflen < 64) {
        return OPRT_INVALID_PARM;
    }

    size_t use_len;
    mbedtls_pem_init(&pem);

    /* If we get there, we know the string is null-terminated */
    ret = mbedtls_pem_read_buffer(&pem, "-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----", buf, NULL, 0,
                                  &use_len);
    if (ret == 0) {
        /* Was PEM encoded */
        *out = tal_malloc(pem.MBEDTLS_PRIVATE(buflen));
        if (NULL == *out) {
            PR_ERR("tal_malloc err");
            goto EXIT;
        }
        memset(*out, 0, pem.MBEDTLS_PRIVATE(buflen));
        memcpy(*out, pem.MBEDTLS_PRIVATE(buf), pem.MBEDTLS_PRIVATE(buflen));
        *outlen = pem.MBEDTLS_PRIVATE(buflen);
    } else if (ret == MBEDTLS_ERR_PEM_BAD_INPUT_DATA) {
        rt = OPRT_COM_ERROR;
        goto EXIT;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        rt = OPRT_COM_ERROR;
        goto EXIT;
    }

EXIT:
    mbedtls_pem_free(&pem);
#endif

    return rt;
}

/**
 * @brief Retrieves the serial number from an X.509 certificate.
 *
 * This function retrieves the serial number from an X.509 certificate and
 * stores it in the provided buffer.
 *
 * @param buf Pointer to the buffer where the serial number will be stored.
 * @param buflen The length of the buffer.
 * @param out Pointer to the output buffer where the serial number will be
 * copied.
 * @param outlen Pointer to the variable that will store the length of the
 * serial number.
 *
 * @return OPERATE_RET Returns the status of the operation.
 */
OPERATE_RET tuya_x509_get_serial(uint8_t *buf, uint32_t buflen, uint8_t out[32], uint32_t *outlen)
{
    OPERATE_RET rt = OPRT_OK;

#if (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
    int ret;
    mbedtls_x509_crt cert;
    uint32_t nr;
    if (buf == NULL || out == NULL) {
        return (OPRT_INVALID_PARM);
    }

    mbedtls_x509_crt_init(&cert);
    ret = mbedtls_x509_crt_parse(&cert, buf, buflen);
    if (ret != 0) {
        PR_ERR("x509_crt_parse fail ret = %d", ret);
        rt = OPRT_COM_ERROR;
        goto EXIT;
    }

    nr = (cert.serial.len <= 32) ? cert.serial.len : 28;
    memcpy(out, cert.serial.p, nr);
    *outlen = nr;

EXIT:

    mbedtls_x509_crt_free(&cert);
#endif

    return rt;
}

/**
 * @brief Retrieves the fingerprint of an X509 certificate.
 *
 * This function calculates the fingerprint of an X509 certificate and stores it
 * in the provided output buffer.
 *
 * @param buf Pointer to the X509 certificate buffer.
 * @param buflen Length of the X509 certificate buffer.
 * @param type The type of fingerprint to calculate.
 * @param out Pointer to the output buffer where the fingerprint will be stored.
 * @param outlen Pointer to the variable that will store the length of the
 * calculated fingerprint.
 *
 * @return OPERATE_RET Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tuya_x509_get_fingerprint(uint8_t *buf, uint32_t buflen, X509_fingerprint type, uint8_t out[64],
                                      uint32_t *outlen)
{
    OPERATE_RET rt = OPRT_OK;
    int32_t ret = OPRT_OK;
    BOOL_T pem_format = FALSE;
    uint8_t *p = NULL;
    uint32_t size;

    if (buf == NULL || out == NULL || buflen < 64) {
        return (OPRT_INVALID_PARM);
    }

    p = buf;
    size = buflen;

    pem_format = tuya_x509_is_ca_pem_format(buf, buflen);
    if (pem_format) {
        ret = tuya_x509_pem2der(buf, buflen, &p, &size);
        if (OPRT_OK != ret) {
            PR_ERR("pem to der err,ret:%d", ret);
            return ret;
        }
    }

    if (type == X509_fingerprint_SHA256) {
        ret = mbedtls_sha256(p, size, out, 0);
        *outlen = 32;
    } else if (type == X509_fingerprint_SHA1) {
        ret = mbedtls_sha1(p, size, out);
        *outlen = 16;
    } else {
        rt = OPRT_INVALID_PARM;
    }

    if (ret != 0) {
        rt = OPRT_COM_ERROR;
    }

    if (pem_format) {
        tal_free(p);
    }

    return rt;
}
#if defined(TUYA_X509_SELF_TEST) && (TUYA_X509_SELF_TEST == 1)
static int8_t iot_pem[] = "-----BEGIN "
                          "CERTIFICATE-----\nMIIB+DCCAZ+gAwIBAgIRAec/"
                          "XlHx+"
                          "NGcZ85xtEk1Q24wCgYIKoZIzj0EAwIwYDEPMA0GA1UEAwwG\nVGVzdENBMQ0wCwYDVQQKDARUd"
                          "XlhMREwDwYDVQQHDAhIYW5nemhvdTELMAkGA1UECAwCWmoxCzAJ\nBgNVBAYTAkNOMREwDwYDV"
                          "QQLDAhTZWN1cml0eTAeFw0yMjAzMDgxMDAzMDZaFw0yMzAzMDgxMDAz\nMDZaMG8xFTATBgNVB"
                          "AMMDCouaW90ZG5zLm5ldDENMAsGA1UECgwEVHV5YTEVMBMGA1UEBwwMc2Fu\nZnJhbmNpc2NvM"
                          "RMwEQYDVQQIDApDYWxpZm9ybmlhMQswCQYDVQQGEwJVUzEOMAwGA1UECwwFQ2xv\ndWQwWTATB"
                          "gcqhkjOPQIBBggqhkjOPQMBBwNCAASnqFBG4T2Pzu7r3UZz3xU8KNwN711q4Ln6HEyi\nqoRnR"
                          "R87lpUrJVSoPRsLiN6aYD8wwpWy+"
                          "WC0yizo7ShalQdYoyswKTAOBgNVHQ8BAf8EBAMCAYow\nFwYDVR0RBBAwDoIMKi5pb3RkbnMub"
                          "mV0MAoGCCqGSM49BAMCA0cAMEQCIHNz6FHF6Jwnt79/"
                          "nsUo\nxgYcrb4S4YZuhKaXJiQoINJLAiB11BvOTm8zTgrtfq7vWsDkkEzWX7m48sbT9WuieQ7c"
                          "dg==\n-----END CERTIFICATE-----";
static uint8_t iot_der[508] = {
    0x30, 0x82, 0x01, 0xF8, 0x30, 0x82, 0x01, 0x9F, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x11, 0x01, 0xE7, 0x3F, 0x5E,
    0x51, 0xF1, 0xF8, 0xD1, 0x9C, 0x67, 0xCE, 0x71, 0xB4, 0x49, 0x35, 0x43, 0x6E, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86,
    0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30, 0x60, 0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x06,
    0x54, 0x65, 0x73, 0x74, 0x43, 0x41, 0x31, 0x0D, 0x30, 0x0B, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x04, 0x54, 0x75,
    0x79, 0x61, 0x31, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0C, 0x08, 0x48, 0x61, 0x6E, 0x67, 0x7A, 0x68,
    0x6F, 0x75, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0C, 0x02, 0x5A, 0x6A, 0x31, 0x0B, 0x30, 0x09,
    0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x43, 0x4E, 0x31, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C,
    0x08, 0x53, 0x65, 0x63, 0x75, 0x72, 0x69, 0x74, 0x79, 0x30, 0x1E, 0x17, 0x0D, 0x32, 0x32, 0x30, 0x33, 0x30, 0x38,
    0x31, 0x30, 0x30, 0x33, 0x30, 0x36, 0x5A, 0x17, 0x0D, 0x32, 0x33, 0x30, 0x33, 0x30, 0x38, 0x31, 0x30, 0x30, 0x33,
    0x30, 0x36, 0x5A, 0x30, 0x6F, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0C, 0x2A, 0x2E, 0x69,
    0x6F, 0x74, 0x64, 0x6E, 0x73, 0x2E, 0x6E, 0x65, 0x74, 0x31, 0x0D, 0x30, 0x0B, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C,
    0x04, 0x54, 0x75, 0x79, 0x61, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0C, 0x0C, 0x73, 0x61, 0x6E,
    0x66, 0x72, 0x61, 0x6E, 0x63, 0x69, 0x73, 0x63, 0x6F, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0C,
    0x0A, 0x43, 0x61, 0x6C, 0x69, 0x66, 0x6F, 0x72, 0x6E, 0x69, 0x61, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04,
    0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x0E, 0x30, 0x0C, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C, 0x05, 0x43, 0x6C, 0x6F,
    0x75, 0x64, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86,
    0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xA7, 0xA8, 0x50, 0x46, 0xE1, 0x3D, 0x8F, 0xCE, 0xEE,
    0xEB, 0xDD, 0x46, 0x73, 0xDF, 0x15, 0x3C, 0x28, 0xDC, 0x0D, 0xEF, 0x5D, 0x6A, 0xE0, 0xB9, 0xFA, 0x1C, 0x4C, 0xA2,
    0xAA, 0x84, 0x67, 0x45, 0x1F, 0x3B, 0x96, 0x95, 0x2B, 0x25, 0x54, 0xA8, 0x3D, 0x1B, 0x0B, 0x88, 0xDE, 0x9A, 0x60,
    0x3F, 0x30, 0xC2, 0x95, 0xB2, 0xF9, 0x60, 0xB4, 0xCA, 0x2C, 0xE8, 0xED, 0x28, 0x5A, 0x95, 0x07, 0x58, 0xA3, 0x2B,
    0x30, 0x29, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02, 0x01, 0x8A, 0x30,
    0x17, 0x06, 0x03, 0x55, 0x1D, 0x11, 0x04, 0x10, 0x30, 0x0E, 0x82, 0x0C, 0x2A, 0x2E, 0x69, 0x6F, 0x74, 0x64, 0x6E,
    0x73, 0x2E, 0x6E, 0x65, 0x74, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x03, 0x47,
    0x00, 0x30, 0x44, 0x02, 0x20, 0x73, 0x73, 0xE8, 0x51, 0xC5, 0xE8, 0x9C, 0x27, 0xB7, 0xBF, 0x7F, 0x9E, 0xC5, 0x28,
    0xC6, 0x06, 0x1C, 0xAD, 0xBE, 0x12, 0xE1, 0x86, 0x6E, 0x84, 0xA6, 0x97, 0x26, 0x24, 0x28, 0x20, 0xD2, 0x4B, 0x02,
    0x20, 0x75, 0xD4, 0x1B, 0xCE, 0x4E, 0x6F, 0x33, 0x4E, 0x0A, 0xED, 0x7E, 0xAE, 0xEF, 0x5A, 0xC0, 0xE4, 0x90, 0x4C,
    0xD6, 0x5F, 0xB9, 0xB8, 0xF2, 0xC6, 0xD3, 0xF5, 0x6B, 0xA2, 0x79, 0x0E, 0xDC, 0x76};

/**
 * @brief Performs a self-test for the X509 module.
 *
 * This function is used to perform a self-test for the X509 module in the Tuya
 * Open SDK for Device. It returns an OPERATE_RET value indicating the success
 * or failure of the self-test.
 *
 * @return OPERATE_RET - The result of the self-test.
 */
OPERATE_RET tuya_x509_self_test(void)
{
    OPERATE_RET ret;
    uint8_t serial[32];
    uint8_t fingerprint[64];
    uint32_t outlen;
    uint32_t i;

    tuya_tls_set_calloc_and_free();
    outlen = sizeof(serial);
    ret = tuya_x509_get_serial(iot_der, sizeof(iot_der), serial, &outlen);
    if (ret != OPRT_OK) {
        PR_ERR("get cert serial fail ret = %d", ret);
        goto EXIT;
    }
    PR_DEBUG("get cert serial is ok:");
    for (i = 0; i < outlen; i++) {
        PR_DEBUG_RAW("0x%02x,", serial[i]);
    }

    outlen = sizeof(fingerprint);
    //  ret = tuya_x509_get_fingerprint( iot_der, sizeof(iot_der), X509_fingerprint_SHA1,fingerprint , &outlen );
    ret = tuya_x509_get_fingerprint(iot_pem, sizeof(iot_pem), X509_fingerprint_SHA1, fingerprint, &outlen);
    if (ret != OPRT_OK) {
        PR_ERR("get cert fingerprint fail ret = %d", ret);
        goto EXIT;
    }
    PR_DEBUG("get cert fingerpring is ok:");
    for (i = 0; i < outlen; i++) {
        PR_DEBUG_RAW("0x%02x,", fingerprint[i]);
    }

EXIT:

    return ret;
}
#endif
