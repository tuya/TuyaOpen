#include "inflate_stream.h"

#include <string.h>

#include "tal_api.h"
#include "tal_memory.h"

static void *big_alloc(size_t sz)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    return tal_psram_malloc(sz);
#else
    return tal_malloc(sz);
#endif
}

static void big_free(void *p)
{
    if (!p)
        return;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    tal_psram_free(p);
#else
    tal_free(p);
#endif
}

static uint32_t revbits(uint32_t v, uint32_t n)
{
    uint32_t r = 0;
    for (uint32_t i = 0; i < n; i++) {
        r = (r << 1) | (v & 1u);
        v >>= 1;
    }
    return r;
}

static int need_bits(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *pos, uint32_t n)
{
    while (s->bitcount < n) {
        if (*pos >= len)
            return 0;
        s->bitbuf |= ((uint32_t)data[(*pos)++]) << s->bitcount;
        s->bitcount += 8;
    }
    return 1;
}

static uint32_t pull_bits(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *pos, uint32_t n, int *ok)
{
    if (!need_bits(s, data, len, pos, n)) {
        if (ok)
            *ok = 0;
        return 0;
    }
    uint32_t v = s->bitbuf & ((n == 32) ? 0xFFFFFFFFu : ((1u << n) - 1u));
    s->bitbuf >>= n;
    s->bitcount -= n;
    if (ok)
        *ok = 1;
    return v;
}

static void align_byte(inflate_stream_t *s)
{
    uint32_t drop = s->bitcount & 7u;
    s->bitbuf >>= drop;
    s->bitcount -= drop;
}

static int build_table(uint16_t *tbl, const uint8_t *lens, size_t num_syms)
{
    uint16_t count[16];
    uint16_t next_code[16];
    memset(count, 0, sizeof(count));
    for (size_t i = 0; i < num_syms; i++) {
        uint8_t l = lens[i];
        if (l > 15)
            return -1;
        if (l)
            count[l]++;
    }
    uint16_t code = 0;
    count[0]      = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code            = (uint16_t)((code + count[bits - 1]) << 1);
        next_code[bits] = code;
    }
    memset(tbl, 0, 32768u * sizeof(uint16_t));
    for (size_t sym = 0; sym < num_syms; sym++) {
        uint8_t l = lens[sym];
        if (!l)
            continue;
        uint16_t c    = next_code[l]++;
        uint32_t r    = revbits(c, l);
        uint32_t step = 1u << l;
        for (uint32_t idx = r; idx < 32768u; idx += step) {
            tbl[idx] = (uint16_t)((l << 9) | (uint16_t)sym);
        }
    }
    return 0;
}

static int decode_sym(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *pos, const uint16_t *tbl,
                      uint32_t *out_sym)
{
    if (!need_bits(s, data, len, pos, 15))
        return 0;
    uint16_t entry = tbl[s->bitbuf & 0x7FFFu];
    uint32_t l     = (uint32_t)(entry >> 9);
    if (l == 0 || l > 15) {
        s->last_err = -2;
        return -1;
    }
    s->bitbuf >>= l;
    s->bitcount -= l;
    if (out_sym)
        *out_sym = (uint32_t)(entry & 0x01FFu);
    return 1;
}

static int fixed_tables(inflate_stream_t *s)
{
    if (!s->litlen_len) {
        s->litlen_len = (uint8_t *)big_alloc(288);
        if (!s->litlen_len)
            return -1;
        for (int i = 0; i <= 143; i++)
            s->litlen_len[i] = 8;
        for (int i = 144; i <= 255; i++)
            s->litlen_len[i] = 9;
        for (int i = 256; i <= 279; i++)
            s->litlen_len[i] = 7;
        for (int i = 280; i <= 287; i++)
            s->litlen_len[i] = 8;
    }
    if (!s->dist_len) {
        s->dist_len = (uint8_t *)big_alloc(32);
        if (!s->dist_len)
            return -1;
        for (int i = 0; i < 32; i++)
            s->dist_len[i] = 5;
    }
    if (!s->litlen_tbl) {
        s->litlen_tbl = (uint16_t *)big_alloc(32768u * sizeof(uint16_t));
        if (!s->litlen_tbl)
            return -1;
        if (build_table(s->litlen_tbl, s->litlen_len, 288) != 0)
            return -1;
    }
    if (!s->dist_tbl) {
        s->dist_tbl = (uint16_t *)big_alloc(32768u * sizeof(uint16_t));
        if (!s->dist_tbl)
            return -1;
        if (build_table(s->dist_tbl, s->dist_len, 32) != 0)
            return -1;
    }
    return 0;
}

static int dynamic_tables(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *pos)
{
    int      ok   = 0;
    uint32_t HLIT = pull_bits(s, data, len, pos, 5, &ok);
    if (!ok)
        return 0;
    uint32_t HDIST = pull_bits(s, data, len, pos, 5, &ok);
    if (!ok)
        return 0;
    uint32_t HCLEN = pull_bits(s, data, len, pos, 4, &ok);
    if (!ok)
        return 0;
    HLIT += 257;
    HDIST += 1;
    HCLEN += 4;
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t              clen[19];
    memset(clen, 0, sizeof(clen));
    for (uint32_t i = 0; i < HCLEN; i++) {
        uint32_t v = pull_bits(s, data, len, pos, 3, &ok);
        if (!ok)
            return 0;
        clen[order[i]] = (uint8_t)v;
    }

    uint16_t *cltbl = (uint16_t *)big_alloc(32768u * sizeof(uint16_t));
    if (!cltbl) {
        s->last_err = -1;
        return -1;
    }
    if (build_table(cltbl, clen, 19) != 0) {
        big_free(cltbl);
        s->last_err = -1;
        return -1;
    }

    size_t   ll_count = (size_t)(HLIT + HDIST);
    uint8_t *all_len  = (uint8_t *)big_alloc(ll_count);
    if (!all_len) {
        big_free(cltbl);
        s->last_err = -1;
        return -1;
    }
    size_t idx = 0;
    while (idx < ll_count) {
        uint32_t sym = 0;
        int      r   = decode_sym(s, data, len, pos, cltbl, &sym);
        if (r == 0) {
            big_free(cltbl);
            big_free(all_len);
            return 0;
        }
        if (r < 0) {
            big_free(cltbl);
            big_free(all_len);
            return -1;
        }
        if (sym <= 15) {
            all_len[idx++] = (uint8_t)sym;
        } else if (sym == 16) {
            if (idx == 0) {
                big_free(cltbl);
                big_free(all_len);
                s->last_err = -2;
                return -1;
            }
            uint32_t rep = pull_bits(s, data, len, pos, 2, &ok);
            if (!ok) {
                big_free(cltbl);
                big_free(all_len);
                return 0;
            }
            rep += 3;
            uint8_t val = all_len[idx - 1];
            while (rep-- && idx < ll_count)
                all_len[idx++] = val;
        } else if (sym == 17) {
            uint32_t rep = pull_bits(s, data, len, pos, 3, &ok);
            if (!ok) {
                big_free(cltbl);
                big_free(all_len);
                return 0;
            }
            rep += 3;
            while (rep-- && idx < ll_count)
                all_len[idx++] = 0;
        } else if (sym == 18) {
            uint32_t rep = pull_bits(s, data, len, pos, 7, &ok);
            if (!ok) {
                big_free(cltbl);
                big_free(all_len);
                return 0;
            }
            rep += 11;
            while (rep-- && idx < ll_count)
                all_len[idx++] = 0;
        } else {
            big_free(cltbl);
            big_free(all_len);
            s->last_err = -2;
            return -1;
        }
    }
    big_free(cltbl);

    if (s->litlen_len)
        big_free(s->litlen_len);
    if (s->dist_len)
        big_free(s->dist_len);
    s->litlen_len = (uint8_t *)big_alloc(HLIT);
    s->dist_len   = (uint8_t *)big_alloc(HDIST);
    if (!s->litlen_len || !s->dist_len) {
        big_free(all_len);
        s->last_err = -1;
        return -1;
    }
    memcpy(s->litlen_len, all_len, HLIT);
    memcpy(s->dist_len, all_len + HLIT, HDIST);
    big_free(all_len);

    if (s->litlen_tbl)
        big_free(s->litlen_tbl);
    if (s->dist_tbl)
        big_free(s->dist_tbl);
    s->litlen_tbl = (uint16_t *)big_alloc(32768u * sizeof(uint16_t));
    s->dist_tbl   = (uint16_t *)big_alloc(32768u * sizeof(uint16_t));
    if (!s->litlen_tbl || !s->dist_tbl) {
        s->last_err = -1;
        return -1;
    }
    if (build_table(s->litlen_tbl, s->litlen_len, HLIT) != 0) {
        s->last_err = -1;
        return -1;
    }
    if (build_table(s->dist_tbl, s->dist_len, HDIST) != 0) {
        s->last_err = -1;
        return -1;
    }
    return 1;
}

static int out_byte(inflate_stream_t *s, uint8_t b)
{
    s->window[s->win_pos & 32767u] = b;
    s->win_pos                     = (s->win_pos + 1) & 32767u;
    if (s->out_cb)
        return s->out_cb(s->out_user, &b, 1);
    return 0;
}

static int copy_match(inflate_stream_t *s, uint32_t dist, uint32_t length)
{
    if (dist == 0 || dist > 32768u) {
        s->last_err = -2;
        return -1;
    }
    for (uint32_t i = 0; i < length; i++) {
        uint8_t b = s->window[(s->win_pos - dist) & 32767u];
        if (out_byte(s, b) != 0) {
            s->last_err = -3;
            return -1;
        }
    }
    return 0;
}

static int inflate_deflate(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *pos)
{
    static const uint16_t len_base[29]   = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                            31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t  len_extra[29]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                            2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t dist_base[30]  = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                            33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                            1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t  dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    while (1) {
        int      ok     = 0;
        uint32_t bfinal = pull_bits(s, data, len, pos, 1, &ok);
        if (!ok)
            return 0;
        uint32_t btype = pull_bits(s, data, len, pos, 2, &ok);
        if (!ok)
            return 0;

        if (btype == 0) {
            align_byte(s);
            if (*pos + 4 > len)
                return 0;
            size_t   p0   = *pos;
            uint16_t LEN  = (uint16_t)data[p0] | ((uint16_t)data[p0 + 1] << 8);
            uint16_t NLEN = (uint16_t)data[p0 + 2] | ((uint16_t)data[p0 + 3] << 8);
            *pos          = p0 + 4;
            if ((uint16_t)(LEN ^ 0xFFFFu) != NLEN) {
                s->last_err = -2;
                return -1;
            }
            if (*pos + LEN > len)
                return 0;
            for (uint16_t i = 0; i < LEN; i++) {
                if (out_byte(s, data[(*pos)++]) != 0) {
                    s->last_err = -3;
                    return -1;
                }
            }
        } else if (btype == 1 || btype == 2) {
            if (btype == 1) {
                if (fixed_tables(s) != 0) {
                    s->last_err = -1;
                    return -1;
                }
            } else {
                int tr = dynamic_tables(s, data, len, pos);
                if (tr == 0)
                    return 0;
                if (tr < 0)
                    return -1;
            }

            while (1) {
                uint32_t sym = 0;
                int      r   = decode_sym(s, data, len, pos, s->litlen_tbl, &sym);
                if (r == 0)
                    return 0;
                if (r < 0)
                    return -1;

                if (sym < 256) {
                    if (out_byte(s, (uint8_t)sym) != 0) {
                        s->last_err = -3;
                        return -1;
                    }
                } else if (sym == 256) {
                    break;
                } else if (sym <= 285) {
                    uint32_t li    = sym - 257;
                    uint32_t extra = len_extra[li];
                    uint32_t lval  = len_base[li];
                    if (extra) {
                        uint32_t ex = pull_bits(s, data, len, pos, extra, &ok);
                        if (!ok)
                            return 0;
                        lval += ex;
                    }

                    uint32_t dsym = 0;
                    r             = decode_sym(s, data, len, pos, s->dist_tbl, &dsym);
                    if (r == 0)
                        return 0;
                    if (r < 0)
                        return -1;
                    if (dsym >= 30) {
                        s->last_err = -2;
                        return -1;
                    }
                    uint32_t de   = dist_extra[dsym];
                    uint32_t dval = dist_base[dsym];
                    if (de) {
                        uint32_t exd = pull_bits(s, data, len, pos, de, &ok);
                        if (!ok)
                            return 0;
                        dval += exd;
                    }
                    if (copy_match(s, dval, lval) != 0)
                        return -1;
                } else {
                    s->last_err = -2;
                    return -1;
                }
            }
        } else {
            s->last_err = -2;
            return -1;
        }

        if (bfinal)
            break;
    }
    return 1;
}

void inflate_stream_init(inflate_stream_t *s, inflate_out_cb_t out_cb, void *out_user)
{
    memset(s, 0, sizeof(*s));
    s->out_cb   = out_cb;
    s->out_user = out_user;
    s->window   = (uint8_t *)big_alloc(32768u);
    s->win_pos  = 0;
    s->last_err = 0;
}

void inflate_stream_deinit(inflate_stream_t *s)
{
    if (!s)
        return;
    if (s->litlen_len)
        big_free(s->litlen_len);
    if (s->dist_len)
        big_free(s->dist_len);
    if (s->litlen_tbl)
        big_free(s->litlen_tbl);
    if (s->dist_tbl)
        big_free(s->dist_tbl);
    if (s->window)
        big_free(s->window);
    memset(s, 0, sizeof(*s));
}

static int zlib_parse_header(inflate_stream_t *s)
{
    uint8_t cmf = s->zhdr[0];
    uint8_t flg = s->zhdr[1];
    if ((cmf & 0x0Fu) != 8u)
        return -2;
    if (((uint16_t)cmf << 8 | flg) % 31u)
        return -2;
    if (flg & 0x20u)
        return -2;
    return 0;
}

int inflate_stream_push(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *consumed)
{
    if (consumed)
        *consumed = 0;
    if (!s || !data)
        return -1;
    if (!s->window)
        return -1;
    if (s->finished) {
        if (consumed)
            *consumed = len;
        return 1;
    }

    size_t pos = 0;
    while (!s->zhdr_done && pos < len) {
        s->zhdr[s->zhdr_used++] = data[pos++];
        if (s->zhdr_used == 2) {
            int hr = zlib_parse_header(s);
            if (hr != 0) {
                s->last_err = hr;
                if (consumed)
                    *consumed = pos;
                return -1;
            }
            s->zhdr_done = 1;
        }
    }
    if (!s->zhdr_done) {
        if (consumed)
            *consumed = pos;
        return 0;
    }

    int ir = inflate_deflate(s, data, len, &pos);
    if (ir == 0) {
        if (consumed)
            *consumed = pos;
        return 0;
    }
    if (ir < 0) {
        if (consumed)
            *consumed = pos;
        return -1;
    }

    if (s->ztrail_left == 0)
        s->ztrail_left = 4;
    while (s->ztrail_left && pos < len) {
        pos++;
        s->ztrail_left--;
    }
    if (s->ztrail_left == 0)
        s->finished = 1;

    if (consumed)
        *consumed = pos;
    return s->finished ? 1 : 0;
}

int inflate_stream_finish(inflate_stream_t *s)
{
    if (!s)
        return -1;
    if (!s->zhdr_done)
        return -1;
    if (!s->finished)
        return -1;
    return 0;
}

int inflate_stream_is_done(const inflate_stream_t *s)
{
    return s ? (int)s->finished : 0;
}
