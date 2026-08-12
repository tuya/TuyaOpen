/**
 * @file tal_local_ip.h
 * @brief Local IPv4 helper for pj on TuyaOpen (TAL WiFi / wired)
 * @note Used when PJ_TUYAOS=1 (LINUX and MCU).
 */
#ifndef __TAL_LOCAL_IP_H__
#define __TAL_LOCAL_IP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get local IPv4 in network byte order (WiFi STA or wired)
 * @param[out] addr_nbo IPv4 network byte order
 * @return 0 on success, -1 on failure
 */
int tal_compat_get_sta_ipv4_nbo(unsigned int *addr_nbo);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_LOCAL_IP_H__ */
