/**
 * @file wav_encode.c
 * @brief wav_encode module is used to
 * @version 0.1
 * @date 2025-02-21
 */

#include "wav_encode.h"

#include "tal_log.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef unsigned char ID[4];

typedef struct {
    ID RIFF; /* {'R', 'I', 'F', 'F'} */
    uint8_t size[4];
    ID CDDA;              /* {'W', 'A', 'V', 'E'} */
    ID fmt;               /* {'f', 'm', 't', ' '} */
    uint8_t chunkSize[4]; /* 16 */

    uint8_t wFormatTag[2];
    uint8_t wChannels[2];
    uint8_t dwSamplesPerSec[4];
    uint8_t dwAvgBytesPerSec[4];
    uint8_t wBlockAlign[2];
    uint8_t wBitsPerSample[2];
    uint8_t dataID[4]; /* {'d', 'a', 't', 'a'} */
    uint8_t dataSize[4];
} __attribute__((packed)) WAVE_HEAD_FMT;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_get_wav_head(uint32_t pcm_len, uint8_t cd_format, uint32_t sample_rate, uint16_t bit_depth,
                             uint16_t channel, uint8_t head[])
{
    uint32_t total_len = 0;
    uint32_t byte_rate = 0;
    uint32_t block_align = 0;

    if (NULL == head || 0 == pcm_len) {
        PR_ERR("head is NULL or pcm_len is 0");
        return OPRT_INVALID_PARM;
    }

    if (cd_format != 1) {
        PR_ERR("cd_format is not 1");
        return OPRT_INVALID_PARM;
    }

    total_len = pcm_len + 36;
    byte_rate = sample_rate * channel * bit_depth / 8;
    block_align = channel * bit_depth / 8;

    WAVE_HEAD_FMT *wav_head = (WAVE_HEAD_FMT *)head;
    memcpy(wav_head->RIFF, "RIFF", 4);
    wav_head->size[0] = total_len & 0xff;
    wav_head->size[1] = (total_len >> 8) & 0xff;
    wav_head->size[2] = (total_len >> 16) & 0xff;
    wav_head->size[3] = (total_len >> 24) & 0xff;
    memcpy(wav_head->CDDA, "WAVE", 4);
    memcpy(wav_head->fmt, "fmt ", 4);
    wav_head->chunkSize[0] = 16;
    wav_head->chunkSize[1] = 0;
    wav_head->chunkSize[2] = 0;
    wav_head->chunkSize[3] = 0;
    wav_head->wFormatTag[0] = cd_format;
    wav_head->wFormatTag[1] = 0;
    wav_head->wChannels[0] = channel;
    wav_head->wChannels[1] = 0;
    wav_head->dwSamplesPerSec[0] = sample_rate & 0xff;
    wav_head->dwSamplesPerSec[1] = (sample_rate >> 8) & 0xff;
    wav_head->dwSamplesPerSec[2] = (sample_rate >> 16) & 0xff;
    wav_head->dwSamplesPerSec[3] = (sample_rate >> 24) & 0xff;
    wav_head->dwAvgBytesPerSec[0] = byte_rate & 0xff;
    wav_head->dwAvgBytesPerSec[1] = (byte_rate >> 8) & 0xff;
    wav_head->dwAvgBytesPerSec[2] = (byte_rate >> 16) & 0xff;
    wav_head->dwAvgBytesPerSec[3] = (byte_rate >> 24) & 0xff;
    wav_head->wBlockAlign[0] = block_align & 0xff;
    wav_head->wBlockAlign[1] = (block_align >> 8) & 0xff;
    wav_head->wBitsPerSample[0] = bit_depth & 0xff;
    wav_head->wBitsPerSample[1] = (bit_depth >> 8) & 0xff;
    memcpy(wav_head->dataID, "data", 4);
    wav_head->dataSize[0] = pcm_len & 0xff;
    wav_head->dataSize[1] = (pcm_len >> 8) & 0xff;
    wav_head->dataSize[2] = (pcm_len >> 16) & 0xff;
    wav_head->dataSize[3] = (pcm_len >> 24) & 0xff;

    return OPRT_OK;
}
