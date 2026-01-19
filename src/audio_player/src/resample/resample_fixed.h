#ifndef RESAMPLE_FIXED_H
#define RESAMPLE_FIXED_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure-integer Q15 linear resampler -> 16k mono int16
 * No floating point used in inner loop (fraction uses 16-bit fractional).
 */
int resample_to_16k_fixed(const int16_t *in, size_t in_frames, int in_rate, int channels,
                          int16_t *out_buf_out, size_t *out_frames_out);

/* Pure-integer Q15 linear resampler -> 8k mono int16
 * No floating point used in inner loop (fraction uses 16-bit fractional).
 */
int resample_to_8k_fixed(const int16_t *in, size_t in_frames, int in_rate, int channels,
                         int16_t *out_buf_out, size_t *out_frames_out);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_FIXED_H */
