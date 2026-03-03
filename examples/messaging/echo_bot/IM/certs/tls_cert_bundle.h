#ifndef __TLS_CERT_BUNDLE_H__
#define __TLS_CERT_BUNDLE_H__

#include "im_platform.h"

OPERATE_RET im_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, size_t *cacert_len);

#endif /* __TLS_CERT_BUNDLE_H__ */
