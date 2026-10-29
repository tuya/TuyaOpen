/**
 * @file compare_packets.c
 * @brief Synthetic NSQ regression inputs; stdout contains length-delimited packets.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "opus/opus.h"
#include "test_audio.h"

int main(void)
{
    unsigned int total = 0;
    for (int complexity = 0; complexity <= 10; complexity++) {
        for (int signal = 0; signal < 5; signal++) {
            int          error;
            OpusEncoder *encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &error);
            assert(encoder && error == OPUS_OK);
            assert(opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity)) == OPUS_OK);
            assert(opus_encoder_ctl(encoder, OPUS_SET_BITRATE(16000)) == OPUS_OK);
            assert(opus_encoder_ctl(encoder, OPUS_SET_VBR(0)) == OPUS_OK);
            assert(opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)) == OPUS_OK);
            unsigned char packet[1275];
            opus_int16    pcm[640];
            uint32_t      rng = 1, hash = 2166136261u;
            int           count = signal == 0 ? 200 : 40;
            for (int frame = 0; frame < count; frame++) {
                for (int i = 0; i < 640; i++) {
                    rng = rng * 1664525u + 1013904223u;
                    if (signal == 0) {
                        pcm[i] = test_audio_pcm[(frame * 640 + i) % TEST_AUDIO_SAMPLES];
                    } else if (signal == 1) {
                        pcm[i] = 0;
                    } else if (signal == 2) {
                        pcm[i] = (i & 1) ? 32767 : -32768;
                    } else if (signal == 3) {
                        pcm[i] = (i == 0) ? 32767 : 0;
                    } else {
                        pcm[i] = (opus_int16)(rng >> 16);
                    }
                }
                int n = opus_encode(encoder, pcm, 640, packet, sizeof(packet));
                assert(n > 0);
                uint32_t record[4] = {(uint32_t)complexity, (uint32_t)signal, (uint32_t)frame, (uint32_t)n};
                assert(fwrite(record, sizeof(record), 1, stdout) == 1);
                assert(fwrite(packet, 1, n, stdout) == (size_t)n);
                hash = (hash ^ (uint32_t)n) * 16777619u;
                for (int i = 0; i < n; i++) {
                    hash = (hash ^ packet[i]) * 16777619u;
                }
                total++;
            }
            if (complexity == 0 && signal == 0) {
                assert(hash == 0xa679fc2eu);
                fprintf(stderr, "complexity 0 synthetic hash=%08x\n", hash);
            }
            opus_encoder_destroy(encoder);
        }
    }
    fprintf(stderr, "packets=%u\n", total);
    return 0;
}
