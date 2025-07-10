#include "tuya_misc.h"
#include <unistd.h>
#include <fcntl.h>
//#include "uv.h"
#include "queue.h"
#include "tuya_log.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include "mbedtls/pk.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

#define RTC_DEFAULT_TLS_CERT_ISSUER_NAME  "CN=Cert,O=WebRTC,C=US"
#define RTC_DEFAULT_TLS_CERT_SUBJECT_NAME "CN=Cert,O=WebRTC,C=US"
#define RTC_DEFAULT_TLS_CERT_VERSION      MBEDTLS_X509_CRT_VERSION_3
#define RTC_DEFAULT_TLS_CERT_MD           MBEDTLS_MD_SHA256
#define RTC_DEFAULT_TLS_CERT_NOT_BEFORE   "20180101000000"
#define RTC_DEFAULT_TLS_CERT_NOT_AFTER    "20351231235959"

// int get_ai_type(struct sockaddr* ai_addr){
//     static struct spec_ipv4_t{
// 	    uint32_t addr;
// 	    uint32_t mask;
// 	    int	    type;
//     } spec_ipv4[] = {
// 	    { 0x7f000000, 0xFF000000, AI_LOOPBACK },
// 	    { 0x00000000, 0xFF000000, AI_DISABLE },
// 	    { 0xa9fe0000, 0xFFFF0000, AI_LINKLOCAL }
//     };

//     static struct spec_ipv6_t{
// 	    uint8_t addr[16];
// 	    uint8_t mask[16];
// 	    int	   type;
//     } spec_ipv6[] = {
// 	    { {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
// 	      {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
// 	      0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
// 	      AI_LOOPBACK
// 	    },
// 	    { {0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
// 	      {0xff,0xc0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
// 	      AI_LINKLOCAL
// 	    },
// 	    { {0x0,0x0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
// 	      { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
// 	       0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
// 	      AI_DISABLE
// 	    }
//     };

//     if(ai_addr->sa_family == AF_INET){
//         struct sockaddr_in* ai_addr_v4 = (struct sockaddr_in*)ai_addr;
//         int i;
//         for(i = 0; i < 3; ++i){
//             uint32_t a = ntohl(ai_addr_v4->sin_addr.s_addr);
//             uint32_t pa = spec_ipv4[i].addr;
//             uint32_t pm = spec_ipv4[i].mask;
//             if( (a & pm) == pa){
//                 return spec_ipv4[i].type;
//             }
//         }
//     }else{
//         struct sockaddr_in6* ai_addr_v6 = (struct sockaddr_in6*)ai_addr;
//         int i;
//         for(i = 0; i < 3; ++i){
//             uint8_t* a = ai_addr_v6->sin6_addr.s6_addr;
//             uint8_t am[16];
//             uint8_t *pa = spec_ipv6[i].addr;
//             uint8_t *pm = spec_ipv6[i].mask;
//             int k;
//             for(k = 0; k < 16; ++k){
//                 am[k] = (uint8_t)((a[k] & pm[k]) & 0xff);
//             }
//             if(memcmp(am, pa, 16) == 0){
//                 return spec_ipv6[i].type;
//             }
//         }
//     }
//     return AI_NORMAL;
// }

// int tuya_p2p_check_system_ipv4_ipv6(int *has_ipv4, int *has_ipv6){
//     tuya_p2p_log_debug("try check system support of ipv4/ipv6\n");
//     *has_ipv4 = 0;
//     *has_ipv6 = 0;

//     tuya_uv_interface_address_t *addr = NULL;
//     int count = 0;
//     tuya_uv_interface_addresses(&addr, &count);
//     int i;
//     for(i=0; i<count; i++){
//         tuya_uv_interface_address_t *paddr = &addr[i];
//         struct sockaddr *p = (struct sockaddr *)&paddr->address;
//         int ai_type = get_ai_type(p);
//         if(ai_type == AI_NORMAL){
//             if(p->sa_family == AF_INET){
//                 *has_ipv4 = 1;
//             }else if(p->sa_family == AF_INET6){
//                 *has_ipv6 = 1;
//             }
//         }
//     }

//     tuya_p2p_log_info("system has ipv4(%s), has ipv6(%s)\n", *has_ipv4 ? "yes" : "no", *has_ipv6 ? "yes" : "no");
//     tuya_uv_free_interface_addresses(addr, count);
//     return 0;
// }

// int tuya_p2p_misc_is_ipv4(tuya_uv_buf_t *host) {
//     char ip[64];
//     snprintf(ip, sizeof(ip), "%.*s", (int)host->len, host->base);
//     struct sockaddr_storage sa;
//     return tuya_uv_inet_pton(AF_INET, ip, &sa) == 0;
// }

// int tuya_p2p_misc_is_ipv6(tuya_uv_buf_t *host){
//     char ip[64];
//     if (host->len > 2 && host->base[0] == '[' && host->base[host->len - 1] == ']') {
//         snprintf(ip, sizeof(ip), "%.*s", (int)host->len - 2, host->base + 1);
//     } else {
//         snprintf(ip, sizeof(ip), "%.*s", (int)host->len, host->base);
//     }
//     struct sockaddr_storage sa;
//     return tuya_uv_inet_pton(AF_INET6, ip, &sa) == 0;
// }

// int tuya_p2p_misc_is_ip(tuya_uv_buf_t *host){
//     return tuya_p2p_misc_is_ipv4(host) || tuya_p2p_misc_is_ipv6(host);
// }

// void tuya_p2p_misc_release_tuya_uv_handle(tuya_uv_handle_t *handle){
//     //TUYA_P2P_POOL_RELEASE(handle);
//     free(handle);
// }

uint64_t tuya_uv_hrtime2(void)
{
#define NANOSEC ((uint64_t)1e9)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (((uint64_t)ts.tv_sec) * NANOSEC + ts.tv_nsec);
}

uint64_t tuya_p2p_misc_get_current_time_ms()
{
    return tuya_uv_hrtime2() / 1000 / 1000;
}

uint64_t tuya_p2p_misc_get_timestamp_ms()
{
    static uint64_t t = 0;
    static uint64_t ms = 0;
    if (ms == 0) {
        t = time(NULL);
        ms = tuya_p2p_misc_get_current_time_ms();
    }
    uint64_t delta_ms = tuya_p2p_misc_get_current_time_ms() - ms;
    return t * 1000 + delta_ms;
}

int32_t tuya_p2p_misc_check_timeout(uint64_t tbegin, uint32_t timeout)
{
    uint64_t tnow = tuya_p2p_misc_get_timestamp_ms();
    if (tnow - tbegin >= timeout) {
        return 1;
    } else {
        return 0;
    }
}

void tuya_p2p_misc_set_blocking(int fd, int blocking)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        tuya_p2p_log_error("get nonblock failed\n");
        return;
    }
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    int ret = fcntl(fd, F_SETFL, flags);
    if (ret < 0) {
        tuya_p2p_log_error("set nonblock failed\n");
    }
    return;
}

char *tuya_p2p_misc_dump_buf(char *buf, int len)
{
    static char print_buf[8092];
    memset(print_buf, 0, sizeof(print_buf));
    int already = 0;
    int remain = sizeof(print_buf) - 1;
    int i;
    for (i = 0; i < len; i++) {
        int ret = snprintf(print_buf + already, remain, " %02hhx", buf[i]);
        if (ret < 0) {
            return print_buf;
        }
        already += ret;
        remain -= ret;
    }
    return print_buf;
}

void tuya_p2p_misc_rand_string(char *buf, uint32_t size)
{
    uint32_t i;
    memset(buf, 0, size);
    for (i = 0; i < size - 1; i++) {
        uint8_t index = rand() % 62; // (62 = 26 + 26 + 10)
        if (index < 10) {
            buf[i] = '0' + index;
        } else if (index < 36) {
            buf[i] = 'A' + index - 10;
        } else {
            buf[i] = 'a' + index - 10 - 26;
        }
    }
}

void tuya_p2p_misc_rand_string_dec(char *buf, uint32_t size)
{
    uint32_t i;
    memset(buf, 0, size);
    for (i = 0; i < size - 1; i++) {
        uint8_t index = rand() % 10; // (62 = 26 + 26 + 10)
        buf[i] = '0' + index;
    }
}

void tuya_p2p_misc_rand_hex(char *buf, uint32_t size)
{
    if (size == 0) {
        return;
    }
    uint32_t i;
    memset(buf, 0, size);
    for (i = 0; i < size; i++) {
        buf[i] = rand() % 0xff;
    }
}

unsigned char tuya_p2p_misc_hex_to_char(unsigned char hex)
{
    if (hex >= 0 && hex <= 9) {
        return '0' + hex;
    } else if (hex >= 10 && hex <= 15) {
        return 'a' + (hex - 10);
    }
    return '0';
}

int tuya_p2p_misc_hex_to_string(char *dst_str, int dst_str_size, unsigned char *src_hex, int src_hex_size, char *sep)
{
    int i;
    int already = 0;
    for (i = 0; i < src_hex_size; i++) {
        if (already + 2 > dst_str_size) {
            return -1;
        }
        dst_str[already++] = tuya_p2p_misc_hex_to_char(src_hex[i] >> 4);
        dst_str[already++] = tuya_p2p_misc_hex_to_char(src_hex[i] & 0x0F);
        if (i != src_hex_size - 1 && sep) {
            if (already + 1 > dst_str_size) {
                return -1;
            }
            dst_str[already++] = *sep;
        }
    }

    if (already + 1 > dst_str_size) {
        return -1;
    }
    dst_str[already++] = '\0';
    return 0;
}

unsigned char tuya_p2p_misc_char_to_hex(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    } else if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0;
}

char tuya_p2p_misc_char_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        c = 'a' + (c - 'A');
    }
    return c;
}

// case insensitive compare
int tuya_p2p_misc_strncicmp(char *a, char *b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int d = tuya_p2p_misc_char_to_lower(a[i]) - tuya_p2p_misc_char_to_lower(b[i]);
        if (d != 0) {
            return d;
        }
        if (a[i] == 0) {
            return 0;
        }
    }
    return 0;
}

int tuya_p2p_misc_generate_pkey(unsigned char *output_buf, size_t *len)
{
    if (output_buf == NULL || len == NULL) {
        return -1;
    }

    int rc = -1;
    mbedtls_pk_context key;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_pk_init(&key);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    int ret;

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) {
        tuya_p2p_log_error(" failed\n  ! mbedtls_ctr_drbg_seed returned -0x%04x\n", -ret);
        goto exit;
    }
    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        tuya_p2p_log_error(" failed\n  !  mbedtls_pk_setup returned -0x%04x", -ret);
        goto exit;
    }
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        tuya_p2p_log_error(" failed\n  !  mbedtls_ecp_gen_key returned -0x%04x", -ret);
        goto exit;
    }

    memset(output_buf, 0, *len);
    ret = mbedtls_pk_write_key_pem(&key, output_buf, *len);
    if (ret != 0) {
        tuya_p2p_log_error(" failed\n  !  mbedtls_pk_write_key_pem returned -0x%04x", -ret);
        goto exit;
    }
    *len = strlen((char *)output_buf) + 1;
    tuya_p2p_log_debug("pkey:\n%s\n", output_buf);
    rc = 0;

exit:
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return rc;
}

int mbedtls_test_rnd_zero_rand(void *rng_state, unsigned char *output, size_t len)
{
    if (rng_state != NULL)
        rng_state = NULL;

    memset(output, 0, len);

    return (0);
}

int tuya_p2p_misc_generate_cert(unsigned char *pkey, size_t pkey_len, unsigned char *output_buf, size_t *output_buf_len)
{
    int rc = -1;
    mbedtls_pk_context *issuer_key = NULL;
    mbedtls_pk_context *subject_key = NULL;
    mbedtls_pk_context loaded_issuer_key;
    mbedtls_x509write_cert crt;
    mbedtls_mpi serial;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int ret;
    char buf[1024];
    memset(buf, 0, 1024);

    mbedtls_x509write_crt_init(&crt);
    mbedtls_pk_init(&loaded_issuer_key);
    mbedtls_mpi_init(&serial);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_ctr_drbg_seed returned %d - %s\n", ret, buf);
        goto exit;
    }

    char str_serial[20];
    tuya_p2p_misc_rand_string_dec(str_serial, sizeof(str_serial));
    ret = mbedtls_mpi_read_string(&serial, 10, str_serial);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_mpi_read_string "
                           "returned -0x%04x - %s\n\n",
                           -ret, buf);
        goto exit;
    }
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    ret = mbedtls_pk_parse_key(&loaded_issuer_key, pkey, pkey_len, NULL, 0, mbedtls_test_rnd_zero_rand, NULL);
#else
    ret = mbedtls_pk_parse_key(&loaded_issuer_key, pkey, pkey_len, NULL, 0);
#endif
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_pk_parse_keyfile "
                           "returned -x%02x - %s\n\n",
                           -ret, buf);
        goto exit;
    }

    issuer_key = &loaded_issuer_key;
    subject_key = &loaded_issuer_key;
    mbedtls_x509write_crt_set_subject_key(&crt, subject_key);
    mbedtls_x509write_crt_set_issuer_key(&crt, issuer_key);

    ret = mbedtls_x509write_crt_set_subject_name(&crt, RTC_DEFAULT_TLS_CERT_SUBJECT_NAME);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_x509write_crt_set_subject_name "
                           "returned -0x%04x - %s\n\n",
                           -ret, buf);
        goto exit;
    }

    ret = mbedtls_x509write_crt_set_issuer_name(&crt, RTC_DEFAULT_TLS_CERT_ISSUER_NAME);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_x509write_crt_set_issuer_name "
                           "returned -0x%04x - %s\n\n",
                           -ret, buf);
        goto exit;
    }

    mbedtls_x509write_crt_set_version(&crt, RTC_DEFAULT_TLS_CERT_VERSION);
    mbedtls_x509write_crt_set_md_alg(&crt, RTC_DEFAULT_TLS_CERT_MD);

    ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_x509write_crt_set_serial "
                           "returned -0x%04x - %s\n\n",
                           -ret, buf);
        goto exit;
    }

    ret = mbedtls_x509write_crt_set_validity(&crt, RTC_DEFAULT_TLS_CERT_NOT_BEFORE, RTC_DEFAULT_TLS_CERT_NOT_AFTER);
    if (ret != 0) {
        mbedtls_strerror(ret, buf, 1024);
        tuya_p2p_log_error(" failed\n  !  mbedtls_x509write_crt_set_validity "
                           "returned -0x%04x - %s\n\n",
                           -ret, buf);
        goto exit;
    }

    memset(output_buf, 0, *output_buf_len);
    ret = mbedtls_x509write_crt_pem(&crt, output_buf, *output_buf_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret < 0) {
        goto exit;
    }

    *output_buf_len = strlen((char *)output_buf) + 1;
    tuya_p2p_log_debug("cert:\n%s\n", output_buf);
    rc = 0;

exit:
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&loaded_issuer_key);
    mbedtls_mpi_free(&serial);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return rc;
}

int tuya_p2p_misc_calculate_cert_fingerprint(char *md_type, unsigned char *cert, int cert_len, char *fingerprint,
                                             int fingerprint_len)
{
    const mbedtls_md_info_t *md_info = NULL;
    if (strcmp(md_type, "sha-1") == 0) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    } else if (strcmp(md_type, "sha-224") == 0) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA224);
    } else if (strcmp(md_type, "sha-256") == 0) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    } else if (strcmp(md_type, "sha-384") == 0) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
    } else if (strcmp(md_type, "sha-512") == 0) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
    }

    if (md_info == NULL) {
        tuya_p2p_log_error("calculate cert fingerprint: invalid md type\n");
        //*flags |= MBEDTLS_X509_BADCERT_OTHER;
        // tuya_p2p_log_error("on dtls verify cert failed: invalid md type\n");
        return -1;
    }

    mbedtls_x509_crt x509_crt;
    mbedtls_x509_crt_init(&x509_crt);
    int ret = mbedtls_x509_crt_parse(&x509_crt, cert, cert_len);
    if (ret != 0) {
        tuya_p2p_log_error("calculate cert fingerprint: parse crt\n");
        return -1;
    }

    unsigned char md[1024];
    mbedtls_md(md_info, x509_crt.raw.p, x509_crt.raw.len, (unsigned char *)md);
    mbedtls_x509_crt_free(&x509_crt);

    snprintf(fingerprint, fingerprint_len, "%s ", md_type);
    int already = strlen(fingerprint);

    char sep = ':';
    ret = tuya_p2p_misc_hex_to_string(fingerprint + already, fingerprint_len - already, md,
                                      mbedtls_md_get_size(md_info), &sep);
    if (ret < 0) {
        tuya_p2p_log_error("calculate cert fingerprint: hex to string\n");
        return -1;
    }

    return 0;
}
