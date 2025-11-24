/**
 * @file utf8_to_gbk.c
 * @brief Implementation of UTF-8 to GBK conversion functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "utf8_to_gbk.h"

/* -------------------- Built-in UTF-8 → GBK lookup table -------------------- */
/* Format: Unicode codepoint (3 byte BE) + GBK high + GBK low, sorted by Unicode in ascending order */
#include "u2g_tbl.inc"   /* See generation script below, each record is 5 bytes */
#include "string.h"

#define U2G_COUNT  (sizeof(u2g_tbl)/5)  /* Each record: 3 bytes Unicode + 2 bytes GBK */

/* Return values */
#define UTF8TOGBK_OK      0
#define UTF8TOGBK_ILSEQ  -1  /* Illegal UTF-8 sequence */
#define UTF8TOGBK_NOMEM  -2  /* Output buffer full */

/* Callback: Read UTF-8 bytes, return actual bytes read (<=buf_size) */
typedef int (*utf8_read_fn)(void *ctx, uint8_t *buf, size_t buf_size);

/* Callback: Write GBK bytes, return actual bytes written (<=buf_size) */
typedef int (*gbk_write_fn)(void *ctx, const uint8_t *buf, size_t buf_size);

/* Internal static context */
struct buf_ctx {
    const uint8_t *in;
    size_t        in_len;
    size_t        in_pos;
    uint8_t       *out;
    size_t        out_max;
    size_t        out_pos;
};

/**
 * @brief Get the full byte length of a UTF8 character
 * @param b First byte of the UTF8 character
 * @return Total bytes of the character, 0 means illegal
 */
uint8_t utf8_full_char_len(uint8_t b)
{
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 0;   /* Illegal header */
}

/* Read callback: Get data from array */
static int read_buf(void *ctx, uint8_t *buf, size_t n)
{
    struct buf_ctx *c = (struct buf_ctx *)ctx;
    size_t left = c->in_len - c->in_pos;
    size_t give = (left > n) ? n : left;
    memcpy(buf, c->in + c->in_pos, give);
    c->in_pos += give;
    return (int)give;
}

/* Write callback: Write to array */
static int write_buf(void *ctx, const uint8_t *buf, size_t n)
{
    struct buf_ctx *c = (struct buf_ctx *)ctx;
    size_t left = c->out_max - c->out_pos;
    size_t give = (left > n) ? n : left;
    memcpy(c->out + c->out_pos, buf, give);
    c->out_pos += give;
    return (give == n) ? (int)n : -1;   /* Unable to write fully means buffer insufficient */
}

/* Binary search: unicode(3B) → return array index, -1 means not found */
static int find_gb2312(const uint8_t u[3])
{
    int lo = 0, hi = U2G_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const uint8_t *p = u2g_tbl + mid * 5;  /* Each record is 5 bytes */
        int cmp = (p[0] != u[0]) ? (p[0] - u[0]) :
                  (p[1] != u[1]) ? (p[1] - u[1]) : (p[2] - u[2]);
        if (cmp == 0) return mid;
        else if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* -------------------- UTF-8 decoding -------------------- */
/* Returns: >0 bytes consumed, 0 needs more bytes, -1 illegal sequence */
static int utf8_decode(const uint8_t *buf, size_t n, uint32_t *out)
{
    if (n == 0) return 0;
    uint8_t c = buf[0];
    if (c < 0x80) { *out = c; return 1; }
    if ((c & 0xE0) == 0xC0) {          /* 110xxxxx 10xxxxxx */
        if (n < 2) return 0;
        if ((buf[1] & 0xC0) != 0x80) return -1;
        *out = ((c & 0x1F) << 6) | (buf[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {          /* 1110xxxx 10xxxxxx 10xxxxxx */
        if (n < 3) return 0;
        if ((buf[1] & 0xC0) != 0x80 || (buf[2] & 0xC0) != 0x80) return -1;
        *out = ((c & 0x0F) << 12) | ((buf[1] & 0x3F) << 6) | (buf[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {          /* 4-byte sequence, this implementation discards or extends */
        return -1;
    }
    return -1;
}

/* -------------------- Main conversion loop -------------------- */
int utf8_to_gbk_stream(utf8_read_fn readfn,  void *read_ctx,
                       gbk_write_fn writefn, void *write_ctx)
{
    uint8_t in[4];
    size_t  in_len = 0;
    uint8_t out[2];
    int eof = 0;  // EOF flag

    for (;;) {
        // Only try to read more data if we haven't hit EOF and buffer is not full
        if (!eof && in_len < 4) {
            int rc = readfn(read_ctx, in + in_len, 4 - in_len);
            if (rc < 0) return UTF8TOGBK_ILSEQ;
            if (rc == 0) {
                eof = 1;  // Mark EOF, but continue processing remaining bytes
            } else {
                in_len += rc;
            }
        }

        // If no data left to process, exit
        if (in_len == 0) break;

        uint32_t u;
        int consumed = utf8_decode(in, in_len, &u);
        
        // If we need more bytes but hit EOF, it's an incomplete sequence
        if (consumed == 0) {
            if (eof) {
                // Incomplete UTF-8 sequence at end of input, output replacement character
                out[0] = 0x3F;  // '?'
                if (writefn(write_ctx, out, 1) != 1) return UTF8TOGBK_NOMEM;
                break;  // Exit after handling incomplete sequence
            }
            continue;  // Need more bytes, continue reading
        }
        
        if (consumed < 0) return UTF8TOGBK_ILSEQ;

        /* ASCII passthrough */
        if (u < 0x80) {
            out[0] = (uint8_t)u;
            if (writefn(write_ctx, out, 1) != 1) return UTF8TOGBK_NOMEM;
        } else {
            /* Convert to big-endian 3 bytes */
            uint8_t ube[3] = { (uint8_t)(u >> 16), (uint8_t)(u >> 8), (uint8_t)u };
            int idx = find_gb2312(ube);
            if (idx < 0) {                    /* Not found, use ? as replacement */
                out[0] = 0x3F;
                if (writefn(write_ctx, out, 1) != 1) return UTF8TOGBK_NOMEM;
            } else {
                const uint8_t *g = u2g_tbl + idx * 5 + 3; /* Skip 3 bytes Unicode, point to GBK */
                out[0] = g[0];   /* GBK high byte */
                out[1] = g[1];   /* GBK low byte */
                if (writefn(write_ctx, out, 2) != 2) return UTF8TOGBK_NOMEM;
            }
        }
        /* Sliding window */
        memmove(in, in + consumed, in_len - consumed);
        in_len -= consumed;
    }
    return UTF8TOGBK_OK;
}

/* ---------- Array-based entry point ---------- */
int utf8_to_gbk_buf(const uint8_t *in,  size_t in_len,
                    uint8_t       *out, size_t out_max)
{
    struct buf_ctx c = { .in      = in,
                         .in_len  = in_len,
                         .in_pos  = 0,
                         .out     = out,
                         .out_max = out_max,
                         .out_pos = 0 };
    int rc = utf8_to_gbk_stream(read_buf, &c, write_buf, &c);
    if (rc < 0) return rc;            /* Conversion error */
    return (int)c.out_pos;            /* Actual output byte count */
}