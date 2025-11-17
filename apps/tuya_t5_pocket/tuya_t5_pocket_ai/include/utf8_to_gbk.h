#ifndef UTF8TOGBK_H
#define UTF8TOGBK_H

#include <stdint.h>
#include <stddef.h>

/* Return values */
#define UTF8TOGBK_OK      0
#define UTF8TOGBK_ILSEQ  -1  /* Illegal UTF-8 sequence */
#define UTF8TOGBK_NOMEM  -2  /* Output buffer full */

/* Callback: Read UTF-8 bytes, return actual bytes read (<=buf_size) */
typedef int (*utf8_read_fn)(void *ctx, uint8_t *buf, size_t buf_size);

/* Callback: Write GBK bytes, return actual bytes written (<=buf_size) */
typedef int (*gbk_write_fn)(void *ctx, const uint8_t *buf, size_t buf_size);

/* Main interface: Complete conversion in one call */
int utf8_to_gbk_stream(utf8_read_fn readfn,  void *read_ctx,
                       gbk_write_fn writefn, void *write_ctx);

/* Array-based conversion: Returns actual bytes written to out, <0 indicates error */
int utf8_to_gbk_buf(const uint8_t *in,  size_t in_len,
                    uint8_t       *out, size_t out_max);
#endif