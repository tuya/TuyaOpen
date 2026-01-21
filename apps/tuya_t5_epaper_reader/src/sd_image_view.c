#include "sd_image_view.h"

#include "tal_api.h"
#include "tkl_fs.h"
#include "GUI_Paint.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tjpgd.h"
#include "lodepng.h"
#include "tkl_memory.h"
#include "png_stream_decoder.h"

static void *heap_malloc(size_t size)
{
    return tal_malloc(size);
}

static void heap_free(void *ptr)
{
    if (!ptr)
        return;
    tal_free(ptr);
}

static int psram_free_now(void)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    return tkl_system_psram_get_free_heap_size();
#else
    return -1;
#endif
}

static void mem_print(const char *tag)
{
    int heap_free  = tal_system_get_free_heap_size();
    int psram_free = psram_free_now();
    if (psram_free >= 0)
        PR_NOTICE("%s heap_free=0x%x psram_free=0x%x", tag, heap_free, psram_free);
    else
        PR_NOTICE("%s heap_free=0x%x", tag, heap_free);
}

static TUYA_FILE fopen_read_bin(const char *path)
{
    TUYA_FILE f = tkl_fopen(path, "rb");
    if (f)
        return f;
    return tkl_fopen(path, "r");
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int read_exact(TUYA_FILE f, void *buf, int bytes)
{
    int rd = tkl_fread(buf, bytes, f);
    return (rd == bytes) ? 0 : -1;
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

static int draw_bmp_1bit(const char *path, int x, int y, int w, int h)
{
    TUYA_FILE f = fopen_read_bin(path);
    if (!f)
        return -1;

    uint8_t hdr[54];
    if (read_exact(f, hdr, (int)sizeof(hdr)) != 0) {
        tkl_fclose(f);
        return -1;
    }
    if (hdr[0] != 'B' || hdr[1] != 'M') {
        tkl_fclose(f);
        return -1;
    }

    uint32_t pixel_off = le32(&hdr[10]);
    uint32_t dib_sz    = le32(&hdr[14]);
    int32_t  src_w     = (int32_t)le32(&hdr[18]);
    int32_t  src_h_raw = (int32_t)le32(&hdr[22]);
    uint16_t planes    = le16(&hdr[26]);
    uint16_t bpp       = le16(&hdr[28]);
    uint32_t comp      = le32(&hdr[30]);
    if (planes != 1 || bpp != 1 || comp != 0 || src_w <= 0 || src_h_raw == 0 || dib_sz < 40) {
        tkl_fclose(f);
        return -1;
    }

    int     top_down = (src_h_raw < 0);
    int32_t src_h    = top_down ? -src_h_raw : src_h_raw;

    uint8_t pal[8];
    if (tkl_fseek(f, 14 + (int64_t)dib_sz, SEEK_SET) != 0) {
        tkl_fclose(f);
        return -1;
    }
    if (read_exact(f, pal, (int)sizeof(pal)) != 0) {
        tkl_fclose(f);
        return -1;
    }
    uint8_t b0 = pal[0], g0 = pal[1], r0 = pal[2];
    uint8_t b1 = pal[4], g1 = pal[5], r1 = pal[6];
    uint8_t y0     = luma_u8(r0, g0, b0);
    uint8_t y1     = luma_u8(r1, g1, b1);
    uint8_t color0 = (y0 < 128) ? BLACK : WHITE;
    uint8_t color1 = (y1 < 128) ? BLACK : WHITE;

    int draw_w, draw_h, off_x, off_y;
    fit_aspect(src_w, src_h, w, h, &draw_w, &draw_h, &off_x, &off_y);
    if (draw_w <= 0 || draw_h <= 0) {
        tkl_fclose(f);
        return -1;
    }

    uint32_t row_sz = (uint32_t)(((src_w + 31) / 32) * 4);
    uint8_t *row    = (uint8_t *)tal_malloc(row_sz);
    if (!row) {
        tkl_fclose(f);
        return -1;
    }

    int last_row = -1;
    for (int dy = 0; dy < draw_h; dy++) {
        int sy          = (int)((int64_t)dy * src_h / draw_h);
        int row_in_file = top_down ? sy : (src_h - 1 - sy);
        if (row_in_file != last_row) {
            int64_t off = (int64_t)pixel_off + (int64_t)row_in_file * row_sz;
            if (tkl_fseek(f, off, SEEK_SET) != 0 || read_exact(f, row, (int)row_sz) != 0) {
                tal_free(row);
                tkl_fclose(f);
                return -1;
            }
            last_row = row_in_file;
        }
        for (int dx = 0; dx < draw_w; dx++) {
            int     sx   = (int)((int64_t)dx * src_w / draw_w);
            uint8_t byte = row[(uint32_t)sx / 8];
            uint8_t bit  = (byte & (0x80u >> (sx & 7))) ? 1 : 0;
            uint8_t c    = bit ? color1 : color0;
            Paint_SetPixel((UWORD)(x + off_x + dx), (UWORD)(y + off_y + dy), c);
        }
    }

    tal_free(row);
    tkl_fclose(f);
    return 0;
}

static int draw_bmp_24bit(const char *path, int x, int y, int w, int h)
{
    TUYA_FILE f = fopen_read_bin(path);
    if (!f)
        return -1;

    uint8_t hdr[54];
    if (read_exact(f, hdr, (int)sizeof(hdr)) != 0) {
        tkl_fclose(f);
        return -1;
    }
    if (hdr[0] != 'B' || hdr[1] != 'M') {
        tkl_fclose(f);
        return -1;
    }

    uint32_t pixel_off = le32(&hdr[10]);
    uint32_t dib_sz    = le32(&hdr[14]);
    int32_t  src_w     = (int32_t)le32(&hdr[18]);
    int32_t  src_h_raw = (int32_t)le32(&hdr[22]);
    uint16_t planes    = le16(&hdr[26]);
    uint16_t bpp       = le16(&hdr[28]);
    uint32_t comp      = le32(&hdr[30]);
    if (planes != 1 || bpp != 24 || comp != 0 || src_w <= 0 || src_h_raw == 0 || dib_sz < 40) {
        tkl_fclose(f);
        return -1;
    }

    int     top_down = (src_h_raw < 0);
    int32_t src_h    = top_down ? -src_h_raw : src_h_raw;

    int draw_w, draw_h, off_x, off_y;
    fit_aspect(src_w, src_h, w, h, &draw_w, &draw_h, &off_x, &off_y);
    if (draw_w <= 0 || draw_h <= 0) {
        tkl_fclose(f);
        return -1;
    }

    uint32_t row_sz = (uint32_t)(((src_w * 3 + 3) / 4) * 4);
    uint8_t *row    = (uint8_t *)tal_malloc(row_sz);
    if (!row) {
        tkl_fclose(f);
        return -1;
    }

    int last_row = -1;
    for (int dy = 0; dy < draw_h; dy++) {
        int sy          = (int)((int64_t)dy * src_h / draw_h);
        int row_in_file = top_down ? sy : (src_h - 1 - sy);
        if (row_in_file != last_row) {
            int64_t off = (int64_t)pixel_off + (int64_t)row_in_file * row_sz;
            if (tkl_fseek(f, off, SEEK_SET) != 0 || read_exact(f, row, (int)row_sz) != 0) {
                tal_free(row);
                tkl_fclose(f);
                return -1;
            }
            last_row = row_in_file;
        }
        for (int dx = 0; dx < draw_w; dx++) {
            int            sx = (int)((int64_t)dx * src_w / draw_w);
            const uint8_t *p  = &row[(uint32_t)sx * 3];
            uint8_t        b  = p[0];
            uint8_t        g  = p[1];
            uint8_t        r  = p[2];
            uint8_t        yy = luma_u8(r, g, b);
            uint8_t        c  = (yy < 128) ? BLACK : WHITE;
            Paint_SetPixel((UWORD)(x + off_x + dx), (UWORD)(y + off_y + dy), c);
        }
    }

    tal_free(row);
    tkl_fclose(f);
    return 0;
}

static int draw_bmp_any(const char *path, int x, int y, int w, int h)
{
    TUYA_FILE f = fopen_read_bin(path);
    if (!f)
        return -1;
    uint8_t hdr[54];
    if (read_exact(f, hdr, (int)sizeof(hdr)) != 0) {
        tkl_fclose(f);
        return -1;
    }
    tkl_fclose(f);
    uint16_t bpp = le16(&hdr[28]);
    if (bpp == 24)
        return draw_bmp_24bit(path, x, y, w, h);
    if (bpp == 1)
        return draw_bmp_1bit(path, x, y, w, h);
    return -1;
}

typedef struct {
    TUYA_FILE file;
    int       x;
    int       y;
    int       w;
    int       h;
    int       draw_w;
    int       draw_h;
    int       off_x;
    int       off_y;
    int       src_w;
    int       src_h;
} JPG_DEV_T;

static size_t tjpgd_infunc(JDEC *jd, uint8_t *buf, size_t len)
{
    JPG_DEV_T *dev = (JPG_DEV_T *)jd->device;
    if (!dev || !dev->file)
        return 0;
    if (!buf) {
        if (tkl_fseek(dev->file, (INT64_T)len, SEEK_CUR) != 0)
            return 0;
        return len;
    }
    int rd = tkl_fread(buf, (int)len, dev->file);
    if (rd < 0)
        rd = 0;
    return (size_t)rd;
}

static int tjpgd_outfunc(JDEC *jd, void *bitmap, JRECT *rect)
{
    JPG_DEV_T *dev = (JPG_DEV_T *)jd->device;
    if (!dev || !bitmap || !rect)
        return 0;

    int            rect_w = (int)rect->right - (int)rect->left + 1;
    int            rect_h = (int)rect->bottom - (int)rect->top + 1;
    const uint8_t *src    = (const uint8_t *)bitmap;

    for (int ry = 0; ry < rect_h; ry++) {
        int sy = (int)rect->top + ry;
        int dy = dev->y + dev->off_y + (int)((int64_t)sy * dev->draw_h / dev->src_h);
        for (int rx = 0; rx < rect_w; rx++) {
            int sx = (int)rect->left + rx;
            int dx = dev->x + dev->off_x + (int)((int64_t)sx * dev->draw_w / dev->src_w);
            if (dx < dev->x || dy < dev->y || dx >= dev->x + dev->w || dy >= dev->y + dev->h)
                continue;
            const uint8_t *p  = &src[(ry * rect_w + rx) * 3];
            uint8_t        yy = luma_u8(p[0], p[1], p[2]);
            uint8_t        c  = (yy < 128) ? BLACK : WHITE;
            Paint_SetPixel((UWORD)dx, (UWORD)dy, c);
        }
    }
    return 1;
}

static int draw_jpg_1bit(const char *path, int x, int y, int w, int h)
{
    TUYA_FILE f = fopen_read_bin(path);
    if (!f)
        return -1;

    JPG_DEV_T dev = {0};
    dev.file      = f;
    dev.x         = x;
    dev.y         = y;
    dev.w         = w;
    dev.h         = h;

    size_t pool_sz = 96 * 1024;
    void  *pool    = heap_malloc(pool_sz);
    if (!pool) {
        tkl_fclose(f);
        return -1;
    }

    JDEC    jd = {0};
    JRESULT r  = jd_prepare(&jd, tjpgd_infunc, pool, pool_sz, &dev);
    if (r != JDR_OK) {
        heap_free(pool);
        tkl_fclose(f);
        return -1;
    }

    dev.src_w = jd.width;
    dev.src_h = jd.height;

    fit_aspect(dev.src_w, dev.src_h, w, h, &dev.draw_w, &dev.draw_h, &dev.off_x, &dev.off_y);
    if (dev.draw_w <= 0 || dev.draw_h <= 0) {
        heap_free(pool);
        tkl_fclose(f);
        return -1;
    }

    r = jd_decomp(&jd, tjpgd_outfunc, 0);

    heap_free(pool);
    tkl_fclose(f);
    return (r == JDR_OK) ? 0 : -1;
}

static int load_file_all(const char *path, uint8_t **out_buf, size_t *out_len, BOOL_T *out_from_psram)
{
    if (out_buf)
        *out_buf = NULL;
    if (out_len)
        *out_len = 0;
    if (out_from_psram)
        *out_from_psram = FALSE;
    if (!path || !out_buf || !out_len)
        return -1;
    int sz = tkl_fgetsize(path);
    if (sz <= 0)
        return -1;
    TUYA_FILE f = fopen_read_bin(path);
    if (!f)
        return -1;
    PR_NOTICE("png: file size=%d", sz);
    mem_print("png: before load");
    uint8_t *buf = (uint8_t *)heap_malloc((size_t)sz);
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    if (!buf) {
        buf = (uint8_t *)tal_psram_malloc((size_t)sz);
        if (buf && out_from_psram)
            *out_from_psram = TRUE;
    }
#endif
    if (!buf) {
        tkl_fclose(f);
        return -1;
    }
    PR_NOTICE("png: file buffer=%s", (out_from_psram && *out_from_psram) ? "psram" : "heap");
    mem_print("png: after alloc file buf");
    int rd = tkl_fread(buf, sz, f);
    tkl_fclose(f);
    if (rd != sz) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
        if (out_from_psram && *out_from_psram)
            tal_psram_free(buf);
        else
            heap_free(buf);
#else
        heap_free(buf);
#endif
        return -1;
    }
    *out_buf = buf;
    *out_len = (size_t)sz;
    return 0;
}

static int draw_png_1bit(const char *path, int x, int y, int w, int h)
{
    int sr = png_stream_draw_1bit(path, x, y, w, h);
    if (sr == 0)
        return 0;

    uint8_t *png        = NULL;
    size_t   png_len    = 0;
    BOOL_T   from_psram = FALSE;
    if (load_file_all(path, &png, &png_len, &from_psram) != 0)
        return -1;

    unsigned       src_w = 0, src_h = 0;
    unsigned char *gray1 = NULL;
    mem_print("png: before decode");
    unsigned err = lodepng_decode_memory(&gray1, &src_w, &src_h, (const unsigned char *)png, png_len, LCT_GREY, 1);
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    if (from_psram)
        tal_psram_free(png);
    else
        heap_free(png);
#else
    heap_free(png);
#endif
    if (err != 0 || !gray1 || src_w == 0 || src_h == 0) {
        PR_ERR("png: decode failed err=%u(%s) w=%u h=%u", err, lodepng_error_text(err), src_w, src_h);
        mem_print("png: after decode failed");
        if (gray1)
            lodepng_free(gray1);
        return -1;
    }

    PR_NOTICE("png: decoded %ux%u 1bpp", src_w, src_h);
    mem_print("png: after decode ok");
    int draw_w, draw_h, off_x, off_y;
    fit_aspect((int)src_w, (int)src_h, w, h, &draw_w, &draw_h, &off_x, &off_y);
    if (draw_w <= 0 || draw_h <= 0) {
        lodepng_free(gray1);
        return -1;
    }

    size_t row_stride = ((size_t)src_w + 7u) >> 3;
    for (int dy = 0; dy < draw_h; dy++) {
        int            sy  = (int)((int64_t)dy * (int)src_h / draw_h);
        const uint8_t *row = gray1 + (size_t)sy * row_stride;
        for (int dx = 0; dx < draw_w; dx++) {
            int     sx   = (int)((int64_t)dx * (int)src_w / draw_w);
            uint8_t byte = row[(size_t)sx >> 3];
            uint8_t bit  = (byte >> (7 - (sx & 7))) & 1u;
            uint8_t c    = bit ? WHITE : BLACK;
            Paint_SetPixel((UWORD)(x + off_x + dx), (UWORD)(y + off_y + dy), c);
        }
    }

    lodepng_free(gray1);
    return 0;
}

static const char *ext_ptr(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return "";
    return dot + 1;
}

static int ext_ieq(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
    }
    return (*a == 0 && *b == 0) ? 1 : 0;
}

int sd_draw_image_1bit(const char *path, int x, int y, int w, int h)
{
    const char *ext = ext_ptr(path);
    if (ext_ieq(ext, "bmp"))
        return draw_bmp_any(path, x, y, w, h);
    if (ext_ieq(ext, "jpg") || ext_ieq(ext, "jpeg"))
        return draw_jpg_1bit(path, x, y, w, h);
    if (ext_ieq(ext, "png"))
        return draw_png_1bit(path, x, y, w, h);
    return -1;
}
