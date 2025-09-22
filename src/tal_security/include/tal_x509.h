/**
 * @file tal_x509.h
 * @brief X.509 certificate utilities for Tuya SDK.
 *
 * This header file provides the definitions and functions for handling X.509
 * certificates within the Tuya SDK. It includes functionalities such as
 * checking if a buffer contains a CA certificate in PEM format, converting PEM
 * format certificates to DER format, retrieving the serial number of a
 * certificate, and obtaining the fingerprint of a certificate in either SHA1 or
 * SHA256 format. These operations are crucial for secure communication and
 * authentication in Tuya-based applications, ensuring that devices and services
 * can verify each other's identities through trusted certificates.
 *
 * The file defines an enumeration for fingerprint types (SHA1 and SHA256) and a
 * type for X.509 certificate structures. It also declares functions for various
 * certificate-related operations, supporting the management and verification of
 * X.509 certificates, which are a fundamental aspect of security in IoT
 * applications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It facilitates the integration of
 * certificate handling and verification functionalities, enhancing the security
 * posture of applications and devices within the Tuya ecosystem.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_X509_H__
#define __TAL_X509_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    X509_fingerprint_SHA1 = 0,
    X509_fingerprint_SHA256 = 1,
} X509_fingerprint;

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
BOOL_T tuya_x509_is_ca_pem_format(uint8_t *buf, uint32_t buflen);

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
OPERATE_RET tuya_x509_pem2der(uint8_t *buf, uint32_t buflen, uint8_t **out, uint32_t *outlen);
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
OPERATE_RET tuya_x509_get_serial(uint8_t *buf, uint32_t buflen, uint8_t out[32], uint32_t *outlen);
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
                                      uint32_t *outlen);
/**
 * @brief Performs a self-test for the X509 module.
 *
 * This function is used to perform a self-test for the X509 module in the Tuya
 * Open SDK for Device. It returns an OPERATE_RET value indicating the success
 * or failure of the self-test.
 *
 * @return OPERATE_RET - The result of the self-test.
 */
OPERATE_RET tuya_x509_self_test(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
