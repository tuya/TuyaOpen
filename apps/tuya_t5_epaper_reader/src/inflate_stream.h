#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int (*inflate_out_cb_t)(void *user, const uint8_t *data, size_t len);

typedef struct {
    uint8_t         *litlen_len;
    uint8_t         *dist_len;
    uint16_t        *litlen_tbl;
    uint16_t        *dist_tbl;
    uint8_t         *window;
    uint32_t         bitbuf;
    uint32_t         bitcount;
    uint8_t          zhdr[2];
    uint8_t          zhdr_used;
    uint8_t          zhdr_done;
    uint8_t          ztrail_left;
    uint8_t          finished;
    size_t           win_pos;
    inflate_out_cb_t out_cb;
    void            *out_user;
    int              last_err;
} inflate_stream_t;

void inflate_stream_init(inflate_stream_t *s, inflate_out_cb_t out_cb, void *out_user);
void inflate_stream_deinit(inflate_stream_t *s);

int inflate_stream_push(inflate_stream_t *s, const uint8_t *data, size_t len, size_t *consumed);
int inflate_stream_finish(inflate_stream_t *s);
int inflate_stream_is_done(const inflate_stream_t *s);
