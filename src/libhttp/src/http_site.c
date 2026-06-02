/**
 * @file http_site.c
 * @brief HTTP site route registry and request dispatch.
 * @version 1.0
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#include "http_site.h"

#include "tal_api.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    const char *method;
    const char *path;
    BOOL_T is_static;
    const char *status;
    const char *content_type;
    const char *body;
    HTTP_SITE_HANDLER_CB handler;
    void *handler_ctx;
} HTTP_SITE_ROUTE_T;

typedef struct {
    HTTP_SITE_ROUTE_T *routes;
    uint16_t max_routes;
    uint16_t route_count;
} HTTP_SITE_CTX_T;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Create an HTTP site route table.
 * @param[out] site output site handle.
 * @param[in] max_routes maximum route entries.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_create(HTTP_SITE_HANDLE_T *site, uint16_t max_routes)
{
    HTTP_SITE_CTX_T *ctx = NULL;

    if (site == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (max_routes == 0) {
        max_routes = HTTP_SITE_DEFAULT_MAX_ROUTES;
    }

    ctx = (HTTP_SITE_CTX_T *)tal_malloc(sizeof(HTTP_SITE_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(HTTP_SITE_CTX_T));

    ctx->routes = (HTTP_SITE_ROUTE_T *)tal_malloc(sizeof(HTTP_SITE_ROUTE_T) * max_routes);
    if (ctx->routes == NULL) {
        tal_free(ctx);
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx->routes, 0, sizeof(HTTP_SITE_ROUTE_T) * max_routes);

    ctx->max_routes = max_routes;
    *site = ctx;
    return OPRT_OK;
}

/**
 * @brief Destroy an HTTP site route table.
 * @param[in,out] site site handle pointer.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_destroy(HTTP_SITE_HANDLE_T *site)
{
    HTTP_SITE_CTX_T *ctx = NULL;

    if ((site == NULL) || (*site == NULL)) {
        return OPRT_INVALID_PARM;
    }

    ctx = (HTTP_SITE_CTX_T *)(*site);
    if (ctx->routes != NULL) {
        tal_free(ctx->routes);
    }
    tal_free(ctx);
    *site = NULL;
    return OPRT_OK;
}

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
                                 const char *content_type, const char *body)
{
    HTTP_SITE_CTX_T *ctx = (HTTP_SITE_CTX_T *)site;
    HTTP_SITE_ROUTE_T *route = NULL;

    if ((ctx == NULL) || (method == NULL) || (path == NULL) || (status == NULL) || (content_type == NULL) ||
        (body == NULL)) {
        return OPRT_INVALID_PARM;
    }

    if (ctx->route_count >= ctx->max_routes) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    route = &ctx->routes[ctx->route_count];
    route->method = method;
    route->path = path;
    route->is_static = TRUE;
    route->status = status;
    route->content_type = content_type;
    route->body = body;
    route->handler = NULL;
    route->handler_ctx = NULL;
    ctx->route_count++;
    return OPRT_OK;
}

/**
 * @brief Register multiple static response routes.
 * @param[in] site site handle.
 * @param[in] routes static route table.
 * @param[in] route_count number of routes in the table.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_site_add_static_routes(HTTP_SITE_HANDLE_T site, const HTTP_SITE_STATIC_ROUTE_T *routes,
                                        uint16_t route_count)
{
    uint16_t i = 0;
    OPERATE_RET rt = OPRT_OK;

    if ((routes == NULL) && (route_count > 0)) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < route_count; i++) {
        rt = http_site_add_static(site, routes[i].method, routes[i].path, routes[i].status, routes[i].content_type,
                                  routes[i].body);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    return OPRT_OK;
}

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
                                  HTTP_SITE_HANDLER_CB handler, void *user_ctx)
{
    HTTP_SITE_CTX_T *ctx = (HTTP_SITE_CTX_T *)site;
    HTTP_SITE_ROUTE_T *route = NULL;

    if ((ctx == NULL) || (method == NULL) || (path == NULL) || (handler == NULL)) {
        return OPRT_INVALID_PARM;
    }

    if (ctx->route_count >= ctx->max_routes) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    route = &ctx->routes[ctx->route_count];
    route->method = method;
    route->path = path;
    route->is_static = FALSE;
    route->status = NULL;
    route->content_type = NULL;
    route->body = NULL;
    route->handler = handler;
    route->handler_ctx = user_ctx;
    ctx->route_count++;
    return OPRT_OK;
}

/**
 * @brief Dispatch one HTTP host request through the site route table.
 * @param[in] site site handle.
 * @param[in] req HTTP host request context.
 * @return OPRT_OK when a route handled the request, OPRT_NOT_FOUND when unmatched.
 */
OPERATE_RET http_site_dispatch(HTTP_SITE_HANDLE_T site, const HTTP_HOST_REQUEST_T *req)
{
    HTTP_SITE_CTX_T *ctx = (HTTP_SITE_CTX_T *)site;
    uint16_t i = 0;

    if ((ctx == NULL) || (req == NULL)) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < ctx->route_count; i++) {
        HTTP_SITE_ROUTE_T *route = &ctx->routes[i];

        if (strcmp(req->method, route->method) != 0) {
            continue;
        }
        if (strcmp(req->path, route->path) != 0) {
            continue;
        }

        if (route->is_static == TRUE) {
            return http_host_reply(req->client_fd, route->status, route->content_type, route->body);
        }

        return route->handler(req, route->handler_ctx);
    }

    return OPRT_NOT_FOUND;
}
