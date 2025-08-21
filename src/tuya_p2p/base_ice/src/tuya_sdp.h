#include "queue.h"
#include "tuya_rtp.h"
#include <stdint.h>

typedef enum tuya_p2p_rtc_media_direction {
    MEDIA_DIRECTION_NONE = 0,
    MEDIA_DIRECTION_SENDONLY = 1,
    MEDIA_DIRECTION_RECVONLY = 2,
    MEDIA_DIRECTION_SENDRECV = 3
} tuya_p2p_rtc_media_direction_e;

typedef enum tuya_p2p_rtc_dtls_role {
    DTLS_ROLE_BOTH = 0,
    DTLS_ROLE_CLIENT = 1,
    DTLS_ROLE_SERVER = 2,
} tuya_p2p_rtc_dtls_role_e;

typedef struct rtc_audio_codec {
    QUEUE queue;
    char name[32];
    int pt;
    uint32_t ssrc;
    int sample_rate;
    int channel_number;
} rtc_audio_codec_t;

typedef struct rtc_video_codec {
    QUEUE queue;
    char name[32];
    int pt;
    int original_pt;
    uint32_t ssrc;
    int clock_rate;
    char profile_level_id[65];
} rtc_video_codec_t;

typedef struct rtc_tuya_codec {
    QUEUE queue;
    char name[32];
    int pt;
    int channel_number;
} rtc_tuya_codec_t;

typedef struct {
    QUEUE queue;
    char str[256];
    uint64_t time_ms;
} rtc_cand_t;

typedef struct media_info {
    QUEUE queue;
    char type[8];
    char mid[65];
} media_info_t;

typedef struct rtc_sdp {
    int inited;
    char wms_id[65];
    char cname[65];
    unsigned char aes_key[48];
    char fingerprint[256];
    char ufrag[128];
    char password[128];
    rtc_cand_t candidates;
    media_info_t media_info_list;
    tuya_p2p_rtc_dtls_role_e dtls_role;
    struct {
        char track_id[65];
        tuya_p2p_rtp_rtx_mode_e rtx_mode;
        tuya_p2p_rtc_media_direction_e direction;
        QUEUE codec_list;
        rtc_audio_codec_t negotiated_codec;
        rtc_audio_codec_t rtx_codec;
    } audio;
    struct {
        char track_id[65];
        tuya_p2p_rtp_rtx_mode_e rtx_mode;
        tuya_p2p_rtc_media_direction_e direction;
        QUEUE codec_list;
        rtc_video_codec_t negotiated_codec;
        rtc_video_codec_t rtx_codec;
    } video;
    struct {
        QUEUE codec_list;
        rtc_tuya_codec_t negotiated_codec;
    } tuya;
} rtc_sdp_t;

int tuya_p2p_rtc_sdp_init(rtc_sdp_t *sdp, char *session_id, char *local_id, char *fingerprint, char *ufrag,
                          char *password, tuya_p2p_rtc_dtls_role_e dtls_role);
int tuya_p2p_rtc_sdp_deinit(rtc_sdp_t *sdp);
int tuya_p2p_rtc_sdp_add_media(rtc_sdp_t *sdp, char *mid, char *type);
int tuya_p2p_rtc_sdp_add_audio_codec(rtc_sdp_t *sdp, char *name, int pt, uint32_t ssrc, int sample_rate,
                                     int channel_number);
int tuya_p2p_rtc_sdp_add_video_codec(rtc_sdp_t *sdp, char *name, int pt, uint32_t ssrc, int clock_rate,
                                     char *profile_level_id);
int tuya_p2p_rtc_sdp_add_video_rtx_codec(rtc_sdp_t *sdp, int origin_pt, int pt, uint32_t ssrc, int clock_rate);
int tuya_p2p_rtc_sdp_add_tuya_codec(rtc_sdp_t *sdp, char *name, int pt, int channel_number);
int tuya_p2p_rtc_sdp_encode(rtc_sdp_t *sdp, char *type, char *buf, int size);
int tuya_p2p_rtc_sdp_decode(rtc_sdp_t *sdp, char *buf);
int tuya_p2p_rtc_sdp_negotiate(rtc_sdp_t *local_sdp, rtc_sdp_t *remote_sdp, char *type);
int tuya_p2p_rtc_sdp_add_candidate(rtc_sdp_t *sdp, char *candidate);
int tuya_p2p_rtc_sdp_set_aes_key(rtc_sdp_t *sdp, unsigned char *aes_key, uint32_t len);
int tuya_p2p_rtc_sdp_get_aes_key(rtc_sdp_t *sdp, unsigned char *aes_key, uint32_t len);
int tuya_p2p_rtc_sdp_set_dtls_cert_fingerprint(rtc_sdp_t *sdp, char *fingerprint);
