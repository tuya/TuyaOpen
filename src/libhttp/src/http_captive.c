/**
 * @file http_captive.c
 * @brief Captive portal HTTP redirect helpers (HTTP-only, no DNS hijack).
 * @version 1.1
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#include "http_captive.h"

#include "tal_api.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define HTTP_CAPTIVE_PORTAL_URL_LEN 64

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    BOOL_T running;
    BOOL_T redirect_unmatched_get;
    char portal_url[HTTP_CAPTIVE_PORTAL_URL_LEN];
    const char *const *probe_paths;
} HTTP_CAPTIVE_CTX_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static HTTP_CAPTIVE_CTX_T s_captive_ctx;

static const char *s_default_probe_paths[] = {
    "/generate_204",
    "/gen_204",
    "/connecttest.txt",
    "/ncsi.txt",
    "/hotspot-detect.html",
    "/canonical.html",
    "/success.txt",
    "/mobile/status.php",
    NULL,
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Check whether path is a common captive portal probe URL.
 * @param[in] path request path.
 * @return TRUE when path is a probe URL.
 */
static BOOL_T __http_captive_is_probe_path(const char *path)
{
    const char *const *probe_paths = s_captive_ctx.probe_paths;
    uint32_t i = 0;

    if (path == NULL) {
        return FALSE;
    }

    if (probe_paths == NULL) {
        probe_paths = s_default_probe_paths;
    }

    for (i = 0; probe_paths[i] != NULL; i++) {
        if (strcmp(path, probe_paths[i]) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Send HTTP redirect to the configured portal URL.
 * @param[in] req HTTP request context.
 * @return OPRT_OK on success.
 */
static OPERATE_RET __http_captive_reply_redirect(const HTTP_HOST_REQUEST_T *req)
{
    if ((req == NULL) || (s_captive_ctx.portal_url[0] == '\0')) {
        return OPRT_INVALID_PARM;
    }

    return http_host_reply_redirect(req->client_fd, s_captive_ctx.portal_url);
}

/**
 * @brief Initialize captive portal HTTP redirect state.
 * @param[in] cfg captive portal configuration.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_captive_start(const HTTP_CAPTIVE_CFG_T *cfg)
{
    if ((cfg == NULL) || (cfg->ap_ip == NULL)) {
        return OPRT_INVALID_PARM;
    }

    if (s_captive_ctx.running == TRUE) {
        return OPRT_OK;
    }

    if ((cfg->portal_url != NULL) && (cfg->portal_url[0] != '\0')) {
        (void)snprintf(s_captive_ctx.portal_url, sizeof(s_captive_ctx.portal_url), "%s", cfg->portal_url);
    } else {
        (void)snprintf(s_captive_ctx.portal_url, sizeof(s_captive_ctx.portal_url), "http://%s/", cfg->ap_ip);
    }

    s_captive_ctx.probe_paths = cfg->probe_paths;
    s_captive_ctx.redirect_unmatched_get = cfg->redirect_unmatched_get;
    s_captive_ctx.running = TRUE;

    PR_NOTICE("http_captive started, portal %s (no DNS hijack)", s_captive_ctx.portal_url);
    return OPRT_OK;
}

/**
 * @brief Reset captive portal HTTP redirect state.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_captive_stop(void)
{
    memset(&s_captive_ctx, 0, sizeof(s_captive_ctx));
    return OPRT_OK;
}

/**
 * @brief Handle captive portal probe requests before site dispatch.
 * @param[in] req HTTP request context.
 * @return OPRT_OK when handled, OPRT_NOT_FOUND when not a probe.
 */
OPERATE_RET http_captive_handle_http(const HTTP_HOST_REQUEST_T *req)
{
    if ((req == NULL) || (strcmp(req->method, "GET") != 0)) {
        return OPRT_NOT_FOUND;
    }

    if (__http_captive_is_probe_path(req->path) != TRUE) {
        return OPRT_NOT_FOUND;
    }

    return __http_captive_reply_redirect(req);
}

/**
 * @brief Redirect a GET request to the configured portal URL.
 * @param[in] req HTTP request context.
 * @return OPRT_OK on success, OPRT_NOT_FOUND when redirect is disabled.
 */
OPERATE_RET http_captive_redirect_to_portal(const HTTP_HOST_REQUEST_T *req)
{
    if (s_captive_ctx.redirect_unmatched_get != TRUE) {
        return OPRT_NOT_FOUND;
    }

    return __http_captive_reply_redirect(req);
}
