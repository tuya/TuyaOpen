#include "resample_fixed.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Fixed-point linear interpolation:
   - use 16.16 style for position: integer part in high 32-? but we use 32-bit with 16 frac (Q16)
   - step = (in_rate << 16) / OUT_SR
   - pos starts at 0
   - interpolation: s = ((s0*(1-frac) + s1*frac) >> 16)
*/
#define OUT_SR_16K 16000
#define OUT_SR_8K  8000

static inline int16_t clip16_from_i64(int64_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

int resample_to_16k_fixed(const int16_t *in, size_t in_frames, int in_rate, int channels,
                          int16_t *out_buf_out, size_t *out_frames_out)
{
    if (!in || !out_buf_out || !out_frames_out || in_frames==0 || in_rate<=0 || channels<=0) return -1;
    /* compute output frames */
    size_t out_frames = (size_t)((double)in_frames * ((double)OUT_SR_16K / (double)in_rate));
    if (out_frames == 0) { *out_frames_out = 0; return 0; }
    int16_t *out = out_buf_out;
    if (!out) return -2;

    uint32_t step = (uint32_t)(((uint64_t)in_rate << 16) / (uint32_t)OUT_SR_16K);
    uint32_t pos = 0; /* Q16 fractional position in input samples, pos integer part = pos>>16 */

    for (size_t j=0;j<out_frames;++j){
        uint32_t idx = pos >> 16; /* index in input frames */
        uint32_t frac = pos & 0xFFFF; /* 0..65535 */

        /* read s0 and s1 as mono int32 */
        int32_t s0 = 0, s1 = 0;
        if ((size_t)idx < in_frames) {
            int64_t sum = 0;
            for (int c=0;c<channels;++c) sum += in[idx * channels + c];
            s0 = (int32_t)(sum / channels);
        }
        if ((size_t)(idx + 1) < in_frames) {
            int64_t sum = 0;
            for (int c=0;c<channels;++c) sum += in[(idx + 1) * channels + c];
            s1 = (int32_t)(sum / channels);
        }

        /* linear interpolation in integer: s = (s0*(65536-frac) + s1*frac) >> 16 */
        int64_t v = (int64_t)s0 * (int64_t)(0x10000 - frac) + (int64_t)s1 * (int64_t)frac;
        v >>= 16;
        out[j] = clip16_from_i64(v);

        pos += step;
        /* if pos beyond last sample, clamp to last (prevents reading out of range) */
        if ((pos >> 16) >= in_frames) break;
    }

    *out_frames_out = out_frames;
    return 0;
}

int resample_to_8k_fixed(const int16_t *in, size_t in_frames, int in_rate, int channels,
                         int16_t *out_buf_out, size_t *out_frames_out)
{
    if (!in || !out_buf_out || !out_frames_out || in_frames==0 || in_rate<=0 || channels<=0) return -1;
    /* compute output frames */
    size_t out_frames = (size_t)((double)in_frames * ((double)OUT_SR_8K / (double)in_rate));
    if (out_frames == 0) { *out_frames_out = 0; return 0; }
    int16_t *out = out_buf_out;
    if (!out) return -2;

    uint32_t step = (uint32_t)(((uint64_t)in_rate << 16) / (uint32_t)OUT_SR_8K);
    uint32_t pos = 0; /* Q16 fractional position in input samples, pos integer part = pos>>16 */

    for (size_t j=0;j<out_frames;++j){
        uint32_t idx = pos >> 16; /* index in input frames */
        uint32_t frac = pos & 0xFFFF; /* 0..65535 */

        /* read s0 and s1 as mono int32 */
        int32_t s0 = 0, s1 = 0;
        if ((size_t)idx < in_frames) {
            int64_t sum = 0;
            for (int c=0;c<channels;++c) sum += in[idx * channels + c];
            s0 = (int32_t)(sum / channels);
        }
        if ((size_t)(idx + 1) < in_frames) {
            int64_t sum = 0;
            for (int c=0;c<channels;++c) sum += in[(idx + 1) * channels + c];
            s1 = (int32_t)(sum / channels);
        }

        /* linear interpolation in integer: s = (s0*(65536-frac) + s1*frac) >> 16 */
        int64_t v = (int64_t)s0 * (int64_t)(0x10000 - frac) + (int64_t)s1 * (int64_t)frac;
        v >>= 16;
        out[j] = clip16_from_i64(v);

        pos += step;
        /* if pos beyond last sample, clamp to last (prevents reading out of range) */
        if ((pos >> 16) >= in_frames) break;
    }

    *out_frames_out = out_frames;
    return 0;
}
