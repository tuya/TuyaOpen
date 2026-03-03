#ifndef __IM_UTILS_H__
#define __IM_UTILS_H__

#include "im_platform.h"
#include "cJSON.h"

void        im_safe_copy(char *dst, size_t dst_size, const char *src);
uint16_t    im_parse_http_status(const char *raw_resp);
int         im_find_header_end(const char *buf, int len);
uint64_t    im_fnv1a64(const char *s);

const char *im_json_str(cJSON *obj, const char *key, const char *fallback);
int         im_json_int(cJSON *obj, const char *key, int fallback);
uint32_t    im_json_uint(cJSON *obj, const char *key, uint32_t fallback);

#endif /* __IM_UTILS_H__ */
