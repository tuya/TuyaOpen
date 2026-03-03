#ifndef __HTTP_PROXY_H__
#define __HTTP_PROXY_H__

#include "im_platform.h"

typedef struct proxy_conn proxy_conn_t;

OPERATE_RET http_proxy_init(void);
bool        http_proxy_is_enabled(void);
OPERATE_RET http_proxy_set(const char *host, uint16_t port, const char *type);
OPERATE_RET http_proxy_clear(void);

proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout_ms);
int           proxy_conn_write(proxy_conn_t *conn, const char *data, int len);
int           proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms);
void          proxy_conn_close(proxy_conn_t *conn);

#endif /* __HTTP_PROXY_H__ */
