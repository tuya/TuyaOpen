/**
 * @file http_site.h
 * @brief HTTP site route registry and request dispatch.
 * @version 1.0
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#ifndef __HTTP_SITE_H__
#define __HTTP_SITE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "http_host.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define HTTP_SITE_DEFAULT_MAX_ROUTES 16

/***********************************************************
*********************typedef define*************************
***********************************************************/
typedef void *HTTP_SITE_HANDLE_T;

typedef OPERATE_RET (*HTTP_SITE_HANDLER_CB)(const HTTP_HOST_REQUEST_T *req, void *user_ctx);

typedef struct {
    const char *method;
    const char *path;
    const char *status;
    const char *content_type;
    const char *body;
} HTTP_SITE_STATIC_ROUTE_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Create an HTTP site route table.
 * @param[out] site output site handle.
 * @param[in] max_routes maximum route entries.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_create(HTTP_SITE_HANDLE_T *site, uint16_t max_routes);

/**
 * @brief Destroy an HTTP site route table.
 * @param[in,out] site site handle pointer.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_destroy(HTTP_SITE_HANDLE_T *site);

/**
 * @brief Register a static response route.
 * @param[in] site site handle.
 * @param[in] method HTTP method string.
 * @param[in] path request path string.
 * @param[in] status HTTP status line text.
 * @param[in] content_type Content-Type header value.
 * @param[in] body response body.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_add_static(HTTP_SITE_HANDLE_T site, const char *method, const char *path, const char *status,
                                 const char *content_type, const char *body);

/**
 * @brief Register multiple static response routes.
 * @param[in] site site handle.
 * @param[in] routes static route table.
 * @param[in] route_count number of routes in the table.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_add_static_routes(HTTP_SITE_HANDLE_T site, const HTTP_SITE_STATIC_ROUTE_T *routes,
                                        uint16_t route_count);

/**
 * @brief Register a dynamic handler route.
 * @param[in] site site handle.
 * @param[in] method HTTP method string.
 * @param[in] path request path string.
 * @param[in] handler route handler callback.
 * @param[in] user_ctx user context passed to handler.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_add_handler(HTTP_SITE_HANDLE_T site, const char *method, const char *path,
                                  HTTP_SITE_HANDLER_CB handler, void *user_ctx);

/**
 * @brief Dispatch one HTTP host request through the site route table.
 * @param[in] site site handle.
 * @param[in] req HTTP host request context.
 * @return OPRT_OK when a route handled the request, OPRT_NOT_FOUND when unmatched.
 */
OPERATE_RET http_site_dispatch(HTTP_SITE_HANDLE_T site, const HTTP_HOST_REQUEST_T *req);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_SITE_H__ */
