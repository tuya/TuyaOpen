/**
 * @file demo_g711.c
 * @brief G.711 u-law, the ITU reference algorithm
 * @version 1.0
 * @date 2026-09-02
 * @copyright Copyright (c) Tuya Inc.
 */
#include "demo_g711.h"

#define G711_SIGN_BIT   0x80
#define G711_QUANT_MASK 0x0F
#define G711_SEG_SHIFT  4
#define G711_SEG_MASK   0x70
#define G711_BIAS       0x84
#define G711_CLIP       8159

static const int16_t c_seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF};

static int __g711_segment(int val)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (val <= c_seg_uend[i]) {
            return i;
        }
    }
    return 8;
}

uint8_t demo_g711u_encode_sample(int pcm_val)
{
    int     mask;
    int     seg;
    uint8_t uval;

    /* The law is defined on 14 bits, so the two low bits go first. */
    pcm_val = pcm_val >> 2;
    if (pcm_val < 0) {
        pcm_val = -pcm_val;
        mask = 0x7F;
    } else {
        mask = 0xFF;
    }
    if (pcm_val > G711_CLIP) {
        pcm_val = G711_CLIP;
    }
    pcm_val += (G711_BIAS >> 2);

    seg = __g711_segment(pcm_val);
    if (seg >= 8) {
        return (uint8_t)(0x7F ^ mask);
    }
    uval = (uint8_t)((seg << 4) | ((pcm_val >> (seg + 1)) & G711_QUANT_MASK));
    return (uint8_t)(uval ^ mask);
}

int demo_g711u_decode_sample(uint8_t u_val)
{
    int t;

    u_val = (uint8_t)~u_val;
    t = ((u_val & G711_QUANT_MASK) << 3) + G711_BIAS;
    t <<= ((unsigned int)u_val & G711_SEG_MASK) >> G711_SEG_SHIFT;

    return ((u_val & G711_SIGN_BIT) ? (G711_BIAS - t) : (t - G711_BIAS));
}
