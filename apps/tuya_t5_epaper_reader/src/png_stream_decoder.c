#include "png_stream_decoder.h"

#include <stdint.h>
#include <string.h>

#include "GUI_Paint.h"
#include "tal_api.h"
#include "tkl_fs.h"
#include "tkl_memory.h"

#include "inflate_stream.h"

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define big_alloc(sz) tal_psram_malloc(sz)
#define big_free(p)   tal_psram_free(p)
#else
#define big_alloc(sz) tal_malloc(sz)
#define big_free(p)   tal_free(p)
#endif

static void mem_print(const char *tag)
{
    int heap_free = tal_system_get_free_heap_size();
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    int psram_free = tkl_system_psram_get_free_heap_size();
    PR_NOTICE("%s heap_free=0x%x psram_free=0x%x", tag, heap_free, psram_free);
#else
    PR_NOTICE("%s heap_free=0x%x", tag, heap_free);
#endif
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int read_exact(TUYA_FILE f, void *buf, uint32_t n)
{
    int rd = tkl_fread(buf, (int)n, f);
    return (rd == (int)n) ? 0 : -1;
}

static int skip_exact(TUYA_FILE f, uint32_t n)
{
    uint8_t tmp[256];
    while (n) {
        uint32_t c = n > sizeof(tmp) ? (uint32_t)sizeof(tmp) : n;
        if (read_exact(f, tmp, c) != 0)
            return -1;
        n -= c;
    }
    return 0;
}

static uint8_t luma_u8(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t y = (uint32_t)r * 30u + (uint32_t)g * 59u + (uint32_t)b * 11u;
    return (uint8_t)(y / 100u);
}

static void fit_aspect(int src_w, int src_h, int dst_w, int dst_h, int *out_w, int *out_h, int *out_off_x,
                       int *out_off_y)
{
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        *out_w     = 0;
        *out_h     = 0;
        *out_off_x = 0;
        *out_off_y = 0;
        return;
    }
    int w = dst_w;
    int h = (int)((int64_t)src_h * dst_w / src_w);
    if (h > dst_h) {
        h = dst_h;
        w = (int)((int64_t)src_w * dst_h / src_h);
    }
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    *out_w     = w;
    *out_h     = h;
    *out_off_x = (dst_w - w) / 2;
    *out_off_y = (dst_h - h) / 2;
}

static uint8_t dither_thresh4(int x, int y)
{
    static const uint8_t m[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    uint8_t              v     = m[((y & 3) << 2) | (x & 3)];
    return (uint8_t)(v * 16 + 8);
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int p  = (int)a + (int)b - (int)c;
    int pa = p - (int)a;
    if (pa < 0)
        pa = -pa;
    int pb = p - (int)b;
    if (pb < 0)
        pb = -pb;
    int pc = p - (int)c;
    if (pc < 0)
        pc = -pc;
    if (pa <= pb && pa <= pc)
        return a;
    if (pb <= pc)
        return b;
    return c;
}

typedef struct {
    int base_x;
    int base_y;
    int view_w;
    int view_h;

    uint32_t src_w;
    uint32_t src_h;
    uint8_t  color_type;
    uint8_t  bpp;

    int draw_w;
    int draw_h;
    int off_x;
    int off_y;

    uint32_t row_index;
    uint32_t row_bytes;
    uint32_t row_packet_bytes;

    uint8_t *packet;
    uint32_t packet_used;

    uint8_t *prev;
    uint8_t *cur;
} png_row_ctx_t;

static void unfilter_row(uint8_t filter, uint8_t *cur, const uint8_t *prev, uint32_t row_bytes, uint8_t bpp)
{
    if (filter == 0)
        return;
    if (filter == 1) {
        for (uint32_t i = 0; i < row_bytes; i++) {
            uint8_t left = (i >= bpp) ? cur[i - bpp] : 0;
            cur[i]       = (uint8_t)(cur[i] + left);
        }
        return;
    }
    if (filter == 2) {
        for (uint32_t i = 0; i < row_bytes; i++) {
            uint8_t up = prev ? prev[i] : 0;
            cur[i]     = (uint8_t)(cur[i] + up);
        }
        return;
    }
    if (filter == 3) {
        for (uint32_t i = 0; i < row_bytes; i++) {
            uint8_t left = (i >= bpp) ? cur[i - bpp] : 0;
            uint8_t up   = prev ? prev[i] : 0;
            cur[i]       = (uint8_t)(cur[i] + (uint8_t)(((uint32_t)left + (uint32_t)up) >> 1));
        }
        return;
    }
    if (filter == 4) {
        for (uint32_t i = 0; i < row_bytes; i++) {
            uint8_t left    = (i >= bpp) ? cur[i - bpp] : 0;
            uint8_t up      = prev ? prev[i] : 0;
            uint8_t up_left = (prev && i >= bpp) ? prev[i - bpp] : 0;
            cur[i]          = (uint8_t)(cur[i] + paeth(left, up, up_left));
        }
        return;
    }
}

static void draw_src_row_scaled_1bit(png_row_ctx_t *ctx, uint32_t sy, const uint8_t *row)
{
    if (!ctx || !row)
        return;
    if (ctx->draw_w <= 0 || ctx->draw_h <= 0)
        return;

    uint32_t dy_min =
        (uint32_t)(((uint64_t)sy * (uint64_t)ctx->draw_h + (uint64_t)ctx->src_h - 1u) / (uint64_t)ctx->src_h);
    uint32_t dy_max =
        (uint32_t)((((uint64_t)(sy + 1u) * (uint64_t)ctx->draw_h + (uint64_t)ctx->src_h - 1u) / (uint64_t)ctx->src_h) -
                   1u);
    if (dy_min > dy_max)
        return;
    if (dy_min >= (uint32_t)ctx->draw_h)
        return;
    if (dy_max >= (uint32_t)ctx->draw_h)
        dy_max = (uint32_t)ctx->draw_h - 1u;

    for (uint32_t dy = dy_min; dy <= dy_max; dy++) {
        int yy = ctx->base_y + ctx->off_y + (int)dy;
        for (int dx = 0; dx < ctx->draw_w; dx++) {
            uint32_t       sx  = (uint32_t)(((uint64_t)dx * (uint64_t)ctx->src_w) / (uint64_t)ctx->draw_w);
            const uint8_t *p   = row + (size_t)sx * (size_t)ctx->bpp;
            uint8_t        a   = 255;
            uint8_t        lum = 255;

            if (ctx->color_type == 0) {
                lum = p[0];
            } else if (ctx->color_type == 4) {
                lum = p[0];
                a   = p[1];
            } else if (ctx->color_type == 2) {
                lum = luma_u8(p[0], p[1], p[2]);
            } else if (ctx->color_type == 6) {
                lum = luma_u8(p[0], p[1], p[2]);
                a   = p[3];
            } else {
                lum = 255;
                a   = 0;
            }

            if (a < 16)
                lum = 255;
            uint8_t thr = dither_thresh4(ctx->base_x + ctx->off_x + dx, yy);
            uint8_t c   = (lum < thr) ? BLACK : WHITE;
            Paint_SetPixel((UWORD)(ctx->base_x + ctx->off_x + dx), (UWORD)yy, c);
        }
    }
}

static int row_out_cb(void *user, const uint8_t *data, size_t len)
{
    png_row_ctx_t *ctx = (png_row_ctx_t *)user;
    if (!ctx || !data)
        return -1;
    for (size_t i = 0; i < len; i++) {
        if (ctx->row_index >= ctx->src_h)
            return -1;
        ctx->packet[ctx->packet_used++] = data[i];
        if (ctx->packet_used == ctx->row_packet_bytes) {
            uint8_t filter = ctx->packet[0];
            memcpy(ctx->cur, ctx->packet + 1, ctx->row_bytes);
            unfilter_row(filter, ctx->cur, ctx->prev, ctx->row_bytes, ctx->bpp);
            draw_src_row_scaled_1bit(ctx, ctx->row_index, ctx->cur);
            uint8_t *tmp = ctx->prev;
            ctx->prev    = ctx->cur;
            ctx->cur     = tmp;
            ctx->row_index++;
            ctx->packet_used = 0;
        }
    }
    return 0;
}

static int png_read_chunk_header(TUYA_FILE f, uint32_t *out_len, uint8_t out_type[4])
{
    uint8_t hdr[8];
    if (read_exact(f, hdr, sizeof(hdr)) != 0)
        return -1;
    *out_len = be32(&hdr[0]);
    memcpy(out_type, &hdr[4], 4);
    return 0;
}

static int png_stream_decode_draw(const char *path, png_row_ctx_t *ctx)
{
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    int file_sz = tkl_fgetsize(path);
    if (file_sz <= 0)
        return -1;

    mem_print("pngs: start");
    uint8_t *idat = (uint8_t *)big_alloc((size_t)file_sz);
    if (!idat)
        return -1;
    uint32_t idat_len = 0;

    TUYA_FILE f = tkl_fopen(path, "rb");
    if (!f)
        f = tkl_fopen(path, "r");
    if (!f) {
        big_free(idat);
        return -1;
    }

    uint8_t s[8];
    if (read_exact(f, s, sizeof(s)) != 0) {
        tkl_fclose(f);
        big_free(idat);
        return -1;
    }
    if (memcmp(s, sig, sizeof(sig)) != 0) {
        tkl_fclose(f);
        big_free(idat);
        return -1;
    }

    uint8_t ihdr_seen = 0;

    while (1) {
        uint32_t clen = 0;
        uint8_t  ctype[4];
        if (png_read_chunk_header(f, &clen, ctype) != 0)
            break;

        if (memcmp(ctype, "IHDR", 4) == 0) {
            uint8_t ihdr[13];
            if (clen != 13) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (read_exact(f, ihdr, 13) != 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            (void)skip_exact(f, 4);
            ctx->src_w        = be32(&ihdr[0]);
            ctx->src_h        = be32(&ihdr[4]);
            uint8_t bitdepth  = ihdr[8];
            ctx->color_type   = ihdr[9];
            uint8_t comp      = ihdr[10];
            uint8_t filter    = ihdr[11];
            uint8_t interlace = ihdr[12];

            if (ctx->src_w == 0 || ctx->src_h == 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (bitdepth != 8) {
                PR_ERR("png: unsupported bitdepth=%u", bitdepth);
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (comp != 0 || filter != 0) {
                PR_ERR("png: unsupported comp=%u filter=%u", comp, filter);
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (interlace != 0) {
                PR_ERR("png: unsupported interlace=%u", interlace);
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }

            if (ctx->color_type == 0)
                ctx->bpp = 1;
            else if (ctx->color_type == 2)
                ctx->bpp = 3;
            else if (ctx->color_type == 4)
                ctx->bpp = 2;
            else if (ctx->color_type == 6)
                ctx->bpp = 4;
            else {
                PR_ERR("png: unsupported color_type=%u", ctx->color_type);
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }

            fit_aspect((int)ctx->src_w, (int)ctx->src_h, ctx->view_w, ctx->view_h, &ctx->draw_w, &ctx->draw_h,
                       &ctx->off_x, &ctx->off_y);
            ctx->row_bytes        = ctx->src_w * (uint32_t)ctx->bpp;
            ctx->row_packet_bytes = 1u + ctx->row_bytes;

            PR_NOTICE("pngs: row_bytes=%lu", (unsigned long)ctx->row_bytes);
            ctx->packet = (uint8_t *)big_alloc(ctx->row_packet_bytes);
            ctx->prev   = (uint8_t *)big_alloc(ctx->row_bytes);
            ctx->cur    = (uint8_t *)big_alloc(ctx->row_bytes);
            if (!ctx->packet || !ctx->prev || !ctx->cur) {
                PR_ERR("png: row buffers alloc failed");
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            memset(ctx->prev, 0, ctx->row_bytes);
            ctx->packet_used = 0;
            ctx->row_index   = 0;

            PR_NOTICE("png: IHDR %lux%lu ct=%u bpp=%u", (unsigned long)ctx->src_w, (unsigned long)ctx->src_h,
                      (unsigned)ctx->color_type, (unsigned)ctx->bpp);
            mem_print("pngs: after ihdr");
            ihdr_seen = 1;
        } else if (memcmp(ctype, "IDAT", 4) == 0) {
            if (!ihdr_seen) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (idat_len + clen > (uint32_t)file_sz) {
                PR_ERR("png: IDAT too large");
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (read_exact(f, idat + idat_len, clen) != 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            idat_len += clen;
            if (skip_exact(f, 4) != 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
        } else if (memcmp(ctype, "IEND", 4) == 0) {
            if (clen) {
                if (skip_exact(f, clen) != 0) {
                    tkl_fclose(f);
                    big_free(idat);
                    return -1;
                }
            }
            (void)skip_exact(f, 4);
            break;
        } else {
            if (skip_exact(f, clen) != 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
            if (skip_exact(f, 4) != 0) {
                tkl_fclose(f);
                big_free(idat);
                return -1;
            }
        }
    }

    tkl_fclose(f);

    if (!ihdr_seen || idat_len == 0) {
        big_free(idat);
        return -1;
    }

    PR_NOTICE("pngs: idat_len=%lu", (unsigned long)idat_len);
    mem_print("pngs: before inflate");
    inflate_stream_t infl;
    inflate_stream_init(&infl, row_out_cb, ctx);
    if (!infl.window) {
        inflate_stream_deinit(&infl);
        big_free(idat);
        return -1;
    }

    size_t consumed = 0;
    int    rr       = inflate_stream_push(&infl, idat, idat_len, &consumed);
    big_free(idat);
    if (rr < 0) {
        PR_ERR("png: inflate failed err=%d", infl.last_err);
        inflate_stream_deinit(&infl);
        return -1;
    }
    if (rr != 1) {
        PR_ERR("png: inflate not done");
        inflate_stream_deinit(&infl);
        return -1;
    }
    if (inflate_stream_finish(&infl) != 0) {
        PR_ERR("png: inflate finish failed");
        inflate_stream_deinit(&infl);
        return -1;
    }
    mem_print("pngs: after inflate");
    inflate_stream_deinit(&infl);
    return (ctx->row_index == ctx->src_h) ? 0 : -1;
}

int png_stream_draw_1bit(const char *path, int x, int y, int w, int h)
{
    if (!path)
        return -1;
    png_row_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.base_x = x;
    ctx.base_y = y;
    ctx.view_w = w;
    ctx.view_h = h;

    int r = png_stream_decode_draw(path, &ctx);

    if (ctx.packet)
        big_free(ctx.packet);
    if (ctx.prev)
        big_free(ctx.prev);
    if (ctx.cur)
        big_free(ctx.cur);

    return r;
}
