/*
LodePNG version 20201017
Copyright (c) 2005-2020 Lode Vandevenne
*/
#ifndef LODEPNG_H
#define LODEPNG_H

#include <stddef.h>

#ifndef LODEPNG_NO_COMPILE_ZLIB
#define LODEPNG_COMPILE_ZLIB
#endif
#ifndef LODEPNG_NO_COMPILE_PNG
#define LODEPNG_COMPILE_PNG
#endif
#ifndef LODEPNG_NO_COMPILE_DECODER
#define LODEPNG_COMPILE_DECODER
#endif
#ifndef LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_COMPILE_ENCODER
#endif
#ifndef LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define LODEPNG_COMPILE_ANCILLARY_CHUNKS
#endif
#ifndef LODEPNG_NO_COMPILE_ERROR_TEXT
#define LODEPNG_COMPILE_ERROR_TEXT
#endif
#ifndef LODEPNG_NO_COMPILE_ALLOCATORS
#define LODEPNG_COMPILE_ALLOCATORS
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *lodepng_malloc(size_t size);
void *lodepng_realloc(void *ptr, size_t new_size);
void  lodepng_free(void *ptr);

#ifdef LODEPNG_COMPILE_PNG
typedef enum LodePNGColorType {
    LCT_GREY            = 0,
    LCT_RGB             = 2,
    LCT_PALETTE         = 3,
    LCT_GREY_ALPHA      = 4,
    LCT_RGBA            = 6,
    LCT_MAX_OCTET_VALUE = 255
} LodePNGColorType;
#endif

#ifdef LODEPNG_COMPILE_ERROR_TEXT
const char *lodepng_error_text(unsigned code);
#endif

#ifdef LODEPNG_COMPILE_DECODER
typedef struct LodePNGDecompressSettings LodePNGDecompressSettings;
struct LodePNGDecompressSettings {
    unsigned ignore_adler32;
    unsigned ignore_nlen;
    size_t   max_output_size;
    unsigned (*custom_zlib)(unsigned char **, size_t *, const unsigned char *, size_t,
                            const LodePNGDecompressSettings *);
    unsigned (*custom_inflate)(unsigned char **, size_t *, const unsigned char *, size_t,
                               const LodePNGDecompressSettings *);
    const void *custom_context;
};

extern const LodePNGDecompressSettings lodepng_default_decompress_settings;
void                                   lodepng_decompress_settings_init(LodePNGDecompressSettings *settings);
#endif

#ifdef LODEPNG_COMPILE_PNG
typedef struct LodePNGColorMode {
    LodePNGColorType colortype;
    unsigned         bitdepth;
    unsigned char   *palette;
    size_t           palettesize;
    unsigned         key_defined;
    unsigned         key_r;
    unsigned         key_g;
    unsigned         key_b;
} LodePNGColorMode;

void             lodepng_color_mode_init(LodePNGColorMode *info);
void             lodepng_color_mode_cleanup(LodePNGColorMode *info);
unsigned         lodepng_color_mode_copy(LodePNGColorMode *dest, const LodePNGColorMode *source);
LodePNGColorMode lodepng_color_mode_make(LodePNGColorType colortype, unsigned bitdepth);
void             lodepng_palette_clear(LodePNGColorMode *info);
unsigned         lodepng_palette_add(LodePNGColorMode *info, unsigned char r, unsigned char g, unsigned char b,
                                     unsigned char a);
unsigned         lodepng_get_bpp(const LodePNGColorMode *info);
unsigned         lodepng_get_channels(const LodePNGColorMode *info);
unsigned         lodepng_can_have_alpha(const LodePNGColorMode *info);
size_t           lodepng_get_raw_size(unsigned w, unsigned h, const LodePNGColorMode *color);

typedef struct LodePNGInfo {
    unsigned         compression_method;
    unsigned         filter_method;
    unsigned         interlace_method;
    LodePNGColorMode color;
#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS
    size_t   text_num;
    char   **text_keys;
    char   **text_strings;
    size_t   itext_num;
    char   **itext_keys;
    char   **itext_langtags;
    char   **itext_transkeys;
    char   **itext_strings;
    unsigned time_defined;
    struct LodePNGTime {
        unsigned year, month, day, hour, minute, second;
    } time;
    unsigned       phys_defined;
    unsigned       phys_x;
    unsigned       phys_y;
    unsigned       phys_unit;
    unsigned       gama_defined;
    unsigned       gama_gamma;
    unsigned       chrm_defined;
    unsigned       chrm_white_x;
    unsigned       chrm_white_y;
    unsigned       chrm_red_x;
    unsigned       chrm_red_y;
    unsigned       chrm_green_x;
    unsigned       chrm_green_y;
    unsigned       chrm_blue_x;
    unsigned       chrm_blue_y;
    unsigned       srgb_defined;
    unsigned       srgb_intent;
    unsigned       iccp_defined;
    char          *iccp_name;
    unsigned char *iccp_profile;
    unsigned       iccp_profile_size;
    unsigned char *unknown_chunks_data[3];
    size_t         unknown_chunks_size[3];
#endif
} LodePNGInfo;

void     lodepng_info_init(LodePNGInfo *info);
void     lodepng_info_cleanup(LodePNGInfo *info);
unsigned lodepng_info_copy(LodePNGInfo *dest, const LodePNGInfo *source);
#endif

#ifdef LODEPNG_COMPILE_DECODER
typedef struct LodePNGDecoderSettings {
    LodePNGDecompressSettings zlibsettings;
    unsigned                  ignore_crc;
    unsigned                  ignore_critical;
    unsigned                  ignore_end;
    unsigned                  color_convert;
#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS
    unsigned read_text_chunks;
    unsigned remember_unknown_chunks;
    size_t   max_text_size;
    size_t   max_icc_size;
#endif
} LodePNGDecoderSettings;

void lodepng_decoder_settings_init(LodePNGDecoderSettings *settings);
#endif

#if defined(LODEPNG_COMPILE_DECODER) || defined(LODEPNG_COMPILE_ENCODER)
typedef struct LodePNGState {
#ifdef LODEPNG_COMPILE_DECODER
    LodePNGDecoderSettings decoder;
#endif
    LodePNGColorMode info_raw;
    LodePNGInfo      info_png;
    unsigned         error;
} LodePNGState;

void lodepng_state_init(LodePNGState *state);
void lodepng_state_cleanup(LodePNGState *state);
void lodepng_state_copy(LodePNGState *dest, const LodePNGState *source);
#endif

#ifdef LODEPNG_COMPILE_DECODER
unsigned lodepng_decode_memory(unsigned char **out, unsigned *w, unsigned *h, const unsigned char *in, size_t insize,
                               LodePNGColorType colortype, unsigned bitdepth);
unsigned lodepng_decode32(unsigned char **out, unsigned *w, unsigned *h, const unsigned char *in, size_t insize);
unsigned lodepng_decode24(unsigned char **out, unsigned *w, unsigned *h, const unsigned char *in, size_t insize);
unsigned lodepng_decode(unsigned char **out, unsigned *w, unsigned *h, LodePNGState *state, const unsigned char *in,
                        size_t insize);
unsigned lodepng_inspect(unsigned *w, unsigned *h, LodePNGState *state, const unsigned char *in, size_t insize);
unsigned lodepng_inspect_chunk(LodePNGState *state, size_t pos, const unsigned char *in, size_t insize);
#endif

#ifdef __cplusplus
}
#endif

#endif
