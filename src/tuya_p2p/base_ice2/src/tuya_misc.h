#ifndef __TUYA_MISC_H__
#define __TUYA_MISC_H__

//#include "uv.h"
//#include "queue.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#define BC_MSG_SIZE_MAX        (200 * 1024)
#define TUYA_P2P_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define TUYA_P2P_SDK_VERSION_NUMBER (0xF4030478)
#define TUYA_P2P_SDK_VERSION        "tuya_p2p_sdk_v3.4.120"

#define TUYA_P2P_SDK_SKILL_BASIC         (1)
#define TUYA_P2P_SDK_SKILL_UDP_TCP_RELAY (2)
#define TUYA_P2P_SDK_SKILL_PRECONNECT    (32)
#define TUYA_P2P_SDK_SKILL_MULTIMEDIA    (64)
#define TUYA_P2P_SDK_SKILL_IPV6          (TUYA_P2P_SDK_SKILL_BASIC << 9)
#define TUYA_P2P_SDK_SKILL_NUMBER                                                                                      \
    (TUYA_P2P_SDK_SKILL_BASIC | TUYA_P2P_SDK_SKILL_UDP_TCP_RELAY | TUYA_P2P_SDK_SKILL_PRECONNECT |                     \
     TUYA_P2P_SDK_SKILL_MULTIMEDIA | TUYA_P2P_SDK_SKILL_IPV6)

enum { AI_LOOPBACK = 1, AI_LINKLOCAL = 2, AI_DISABLE = 3, AI_NORMAL = 4 };

#ifndef TUYA_MIN
#define TUYA_MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef TUYA_MAX
#define TUYA_MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// void tuya_p2p_misc_release_tuya_uv_handle(tuya_uv_handle_t *handle);
//#define TUYA_UV_CLOSE(h, f) do { if (!tuya_uv_is_closing((tuya_uv_handle_t *)(h))) { tuya_uv_close((tuya_uv_handle_t
//*)(h), f); } } while(0)

uint64_t tuya_p2p_misc_get_timestamp_ms();
int32_t tuya_p2p_misc_check_timeout(uint64_t tbegin, uint32_t timeout);
void tuya_p2p_misc_rand_string(char *buf, uint32_t size);
void tuya_p2p_misc_rand_hex(char *buf, uint32_t size);
int tuya_p2p_misc_strncicmp(char *a, char *b, int n);

// int get_ai_type(struct sockaddr* ai_addr);
// int tuya_p2p_check_system_ipv4_ipv6(int *has_ipv4, int *has_ipv6);
// int tuya_p2p_misc_is_ipv6(tuya_uv_buf_t *host);
// int tuya_p2p_misc_is_ipv4(tuya_uv_buf_t *host);
// int tuya_p2p_misc_is_ip(tuya_uv_buf_t *host);
unsigned char tuya_p2p_misc_hex_to_char(unsigned char hex);
unsigned char tuya_p2p_misc_char_to_hex(unsigned char c);
void tuya_p2p_misc_set_blocking(int fd, int blocking);
char *tuya_p2p_misc_dump_buf(char *buf, int len);
int tuya_p2p_misc_generate_pkey(unsigned char *output_buf, size_t *len);
int tuya_p2p_misc_generate_cert(unsigned char *pkey, size_t pkey_len, unsigned char *output_buf,
                                size_t *output_buf_len);
int tuya_p2p_misc_calculate_cert_fingerprint(char *md_type, unsigned char *cert, int cert_len, char *fingerprint,
                                             int fingerprint_len);
int mbedtls_test_rnd_zero_rand(void *rng_state, unsigned char *output, size_t len);
#endif
