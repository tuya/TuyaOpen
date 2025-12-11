#ifndef __HTTP_PORT_INTERNAL_H__
#define __HTTP_PORT_INTERNAL_H__

#include <stdbool.h>
#include <stdint.h>

#include "http_manager.h"
#include "http_client_interface.h"
#include "transport_interface.h"
#include "core_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *url;
    char *host;
    char *path;
    uint16_t port;
    bool use_tls;
    http_client_response_t response;
    http_resp_t resp_info;
    size_t read_offset;
    bool response_ready;

    /* Streaming mode fields */
    bool streaming_mode;
    NetworkContext_t network;
    TransportInterface_t transport;
    HTTPResponse_t http_response;
    HTTPRequestHeaders_t request_headers;
    size_t total_body_length;
    size_t bytes_read;
    uint8_t *header_buffer;
} http_session_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_PORT_INTERNAL_H__ */
