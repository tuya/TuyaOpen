#ifndef __UTF8TOGBK_H__
#define __UTF8TOGBK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/* Array-based conversion: Returns actual bytes written to out, <0 indicates error */
int utf8_to_gbk_buf(const uint8_t *in,  size_t in_len,
                    uint8_t       *out, size_t out_max);

/* Streaming conversion: Returns actual bytes written to out, <0 indicates error */
uint8_t utf8_full_char_len(uint8_t b);

#ifdef __cplusplus
}
#endif

#endif