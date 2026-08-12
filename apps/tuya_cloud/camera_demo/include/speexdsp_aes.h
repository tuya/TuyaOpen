/**
 * @file speexdsp_aes.h
 * @brief Declarations for Speex AES APIs in libaudio_subsys.a
 * @note Mirrors OS speexdsp/audio_subsys_speexdsp_wrap2.h
 */
#ifndef __SPEEXDSP_AES_H__
#define __SPEEXDSP_AES_H__

#ifdef __cplusplus
extern "C" {
#endif

/* framesize = samples per 20ms frame (e.g. 320 @ 16 kHz) */
void *speex_aes_create(int framesize);
int speex_aes_process(void *obj, short *mic, short *ref, short *aec);
int speex_aes_set_param(void *obj, int level);
int speex_ns_set_param(void *obj, int level1, int level2);
float speex_get_param(void *obj, float *out, short *linearout);
void speex_aes_destory(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SPEEXDSP_AES_H__ */
