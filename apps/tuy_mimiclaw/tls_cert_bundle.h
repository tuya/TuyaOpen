#pragma once

#include "mimi_base.h"

OPERATE_RET mimi_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, size_t *cacert_len);
