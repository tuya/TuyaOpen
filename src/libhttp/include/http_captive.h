/**
 * @file http_captive.h
 * @brief Captive portal HTTP redirect helpers (HTTP-only, no DNS hijack).
 * @version 1.1
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#ifndef __HTTP_CAPTIVE_H__
#define __HTTP_CAPTIVE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "http_host.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    const char *ap_ip;              /* AP IPv4 string, used to compose default portal URL */
    const char *portal_url;         /* explicit portal URL; if NULL/empty, "http://<ap_ip>/" is used */
    const char *const *probe_paths; /* NULL-terminated probe path list; NULL uses built-in defaults */
    BOOL_T redirect_unmatched_get;  /* whether to redirect any unmatched GET to portal_url */
} HTTP_CAPTIVE_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialize captive portal HTTP redirect state.
 * @param[in] cfg captive portal configuration.
 * @return OPRT_OK on success.
 * @note No DNS hijack is performed. Auto-popup of the captive portal will not
 *       be reliably triggered on phones; users must open a browser and access
 *       the portal URL manually after associating with the SoftAP.
 */
OPERATE_RET http_captive_start(const HTTP_CAPTIVE_CFG_T *cfg);

/**
 * @brief Reset captive portal HTTP redirect state.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_captive_stop(void);

/**
 * @brief Handle captive portal probe requests before site dispatch.
 * @param[in] req HTTP request context.
 * @return OPRT_OK when handled, OPRT_NOT_FOUND when not a probe.
 */
OPERATE_RET http_captive_handle_http(const HTTP_HOST_REQUEST_T *req);

/**
 * @brief Redirect a GET request to the configured portal URL.
 * @param[in] req HTTP request context.
 * @return OPRT_OK on success, OPRT_NOT_FOUND when redirect is disabled.
 */
OPERATE_RET http_captive_redirect_to_portal(const HTTP_HOST_REQUEST_T *req);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_CAPTIVE_H__ */
