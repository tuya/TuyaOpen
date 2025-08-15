#include "tuya_sdp.h"
#include "tuya_misc.h"
#include "tuya_log.h"
#include <string.h>

#define SDP_TEMPLATE_PART_SESSION_HEADER                                                                               \
    "v=0\r\n"                                                                                                          \
    "o=- %lu 1 IN IP4 127.0.0.1\r\n"                                                                                   \
    "s=-\r\n"                                                                                                          \
    "t=0 0\r\n"                                                                                                        \
    "a=group:BUNDLE%s\r\n"                                                                                             \
    "a=msid-semantic: WMS %s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_HEADER "m=%s 9 %s%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_TRANSPORT                                                                              \
    "c=IN IP4 0.0.0.0\r\n"                                                                                             \
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"                                                                                      \
    "a=ice-ufrag:%s\r\n"                                                                                               \
    "a=ice-pwd:%s\r\n"                                                                                                 \
    "a=ice-options:trickle\r\n"

#define SDP_TEMPLATE_PART_MEDIA_TRANSPORT_CANDIDATE "%s"

#define SDP_TEMPLATE_PART_MEDIA_DTLS                                                                                   \
    "a=fingerprint:%s\r\n"                                                                                             \
    "a=setup:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_MID "a=mid:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_DIRECTION "a=%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_MSID "a=msid:%s %s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_RTCP_MUX "a=rtcp-mux\r\n"

#define SDP_TEMPLATE_PART_MEDIA_SSRC "a=ssrc:%u cname:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_SSRC_GROUP                                                                             \
    "a=ssrc-group:FID %u %u\r\n"                                                                                       \
    "a=ssrc:%u cname:%s\r\n"                                                                                           \
    "a=ssrc:%u cname:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_APPLICATION_RTPMAP "a=rtpmap:%d %s %d\r\n"

#define SDP_TEMPLATE_PART_MEDIA_AUDIO_RTPMAP "a=rtpmap:%d %s/%d\r\n"

#define SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPMAP "a=rtpmap:%d %s/%d\r\n"

#define SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_FIR "a=rtcp-fb:%d ccm fir\r\n"

#define SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_NACK "a=rtcp-fb:%d nack\r\n"

#define SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_PLI "a=rtcp-fb:%d nack pli\r\n"

#define SDP_TEMPLATE_PART_MEDIA_VIDEO_ATTR                                                                             \
    "a=fmtp:%d level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_RTX                                                                                    \
    "a=rtpmap:%d rtx/%d\r\n"                                                                                           \
    "a=fmtp:%d apt=%d\r\n"

#define SDP_TEMPLATE_PART_MEDIA_APPLICATION_KEY "a=aes-key:%s\r\n"

rtc_audio_codec_t default_audio_rtpmaps[] = {{{NULL, NULL}, "PCMU", 0, 0, 8000, 1}};

static rtc_audio_codec_t *tuya_p2p_rtc_sdp_find_default_audio_codec(int pt)
{
    uint32_t i;
    for (i = 0; i < sizeof(default_audio_rtpmaps) / sizeof(rtc_audio_codec_t); i++) {
        rtc_audio_codec_t *codec = &default_audio_rtpmaps[i];
        if (codec->pt == pt) {
            return codec;
        }
    }
    return NULL;
}

static char *tuya_p2p_rtc_media_direction_str(tuya_p2p_rtc_media_direction_e direction)
{
    if (direction == MEDIA_DIRECTION_NONE) {
        return "none";
    } else if (direction == MEDIA_DIRECTION_SENDONLY) {
        return "sendonly";
    } else if (direction == MEDIA_DIRECTION_RECVONLY) {
        return "recvonly";
    } else if (direction == MEDIA_DIRECTION_SENDRECV) {
        return "sendrecv";
    }
    return "";
}
#if 0
static tuya_p2p_rtc_media_direction_e tuya_p2p_rtc_media_direction_number(char *direction){
    if(strcmp(direction, "sendonly") == 0){
        return MEDIA_DIRECTION_SENDONLY;
    }else if(strcmp(direction, "recvonly") == 0){
        return MEDIA_DIRECTION_RECVONLY;
    }else if(strcmp(direction, "sendrecv") == 0){
        return MEDIA_DIRECTION_SENDRECV;
    }
    return MEDIA_DIRECTION_NONE;
}
#endif
static int tuya_p2p_rtc_sdp_encode_session(rtc_sdp_t *sdp, char *buf, int size)
{
    int ret;
    int index = 0;
    char bundle[64] = {0};
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->media_info_list.queue)
    {
        media_info_t *info = QUEUE_DATA(q, media_info_t, queue);
        ret = snprintf(bundle + index, sizeof(bundle) - index, " %s", info->mid);
        if (ret < 0 || ret >= (int)sizeof(bundle) - index) {
            return -1;
        }
        index += ret;
    }

    ret = snprintf(buf, size, SDP_TEMPLATE_PART_SESSION_HEADER, (unsigned long)time(NULL), bundle, sdp->wms_id);
    if (ret < 0 || ret >= size) {
        return -1;
    }
    return ret;
}

static int tuya_p2p_rtc_sdp_encode_media_audio(rtc_sdp_t *sdp, char *type, char *mid, char *buf, int size)
{
    int ret;
    int index = 0;
    int already = 0;
    int remain = size;

    char pt[128] = {0};
    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->audio.codec_list)
        {
            rtc_audio_codec_t *codec = QUEUE_DATA(q, rtc_audio_codec_t, queue);
            ret = snprintf(pt + index, sizeof(pt) - index, " %d", codec->pt);
            if (ret < 0 || ret >= (int)sizeof(pt) - index) {
                return -1;
            }
            index += ret;
        }
    } else {
        ret = snprintf(pt, sizeof(pt), " %d", sdp->audio.negotiated_codec.pt);
        if (ret < 0 || ret >= (int)sizeof(pt)) {
            return -1;
        }
    }

    // m line
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_HEADER, "audio", "UDP/TLS/RTP/SAVPF", pt);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // ice
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_TRANSPORT, sdp->ufrag, sdp->password);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // dtls
    char dtls_role[32] = {0};
    if (sdp->dtls_role == DTLS_ROLE_CLIENT) {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "active");
    } else if (sdp->dtls_role == DTLS_ROLE_SERVER) {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "passive");
    } else {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "actpass");
    }
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_DTLS, sdp->fingerprint, dtls_role);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // mid
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_MID, mid);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // media direction
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_DIRECTION,
                   tuya_p2p_rtc_media_direction_str(sdp->audio.direction));
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // msid
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_MSID, sdp->wms_id, sdp->audio.track_id);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // rtcp-mux
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_RTCP_MUX);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // rtpmap
    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->audio.codec_list)
        {
            rtc_audio_codec_t *codec = QUEUE_DATA(q, rtc_audio_codec_t, queue);
            ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_AUDIO_RTPMAP, codec->pt, codec->name,
                           codec->sample_rate);
            if (ret < 0 || ret >= remain) {
                return -1;
            }
            already += ret;
            remain -= ret;
        }
    } else {
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_AUDIO_RTPMAP, sdp->audio.negotiated_codec.pt,
                       sdp->audio.negotiated_codec.name, sdp->audio.negotiated_codec.sample_rate);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;
    }

    // ssrc
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_SSRC, sdp->audio.negotiated_codec.ssrc, sdp->cname);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    return already;
}

static int tuya_p2p_rtc_sdp_encode_media_video(rtc_sdp_t *sdp, char *type, char *mid, char *buf, int size)
{
    int ret;
    int index = 0;
    int already = 0;
    int remain = size;

    char pt[128] = {0};
    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->video.codec_list)
        {
            rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
            ret = snprintf(pt + index, sizeof(pt) - index, " %d", codec->pt);
            if (ret < 0 || ret >= (int)sizeof(pt) - index) {
                return -1;
            }
            index += ret;
        }
    } else {
        ret = snprintf(pt + index, sizeof(pt) - index, " %d", sdp->video.negotiated_codec.pt);
        if (ret < 0 || ret >= (int)sizeof(pt) - index) {
            return -1;
        }
        index += ret;

        if (sdp->video.rtx_codec.pt != -1) {
            ret = snprintf(pt + index, sizeof(pt) - index, " %d", sdp->video.rtx_codec.pt);
            if (ret < 0 || ret >= (int)sizeof(pt) - index) {
                return -1;
            }
            index += ret;
        }
    }

    // m line
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_HEADER, "video", "UDP/TLS/RTP/SAVPF", pt);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // ice
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_TRANSPORT, sdp->ufrag, sdp->password);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // dtls
    char dtls_role[32] = {0};
    if (sdp->dtls_role == DTLS_ROLE_CLIENT) {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "active");
    } else if (sdp->dtls_role == DTLS_ROLE_SERVER) {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "passive");
    } else {
        snprintf(dtls_role, sizeof(dtls_role), "%s", "actpass");
    }
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_DTLS, sdp->fingerprint, dtls_role);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // mid
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_MID, mid);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // media direction
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_DIRECTION,
                   tuya_p2p_rtc_media_direction_str(sdp->video.direction));
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // msid
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_MSID, sdp->wms_id, sdp->video.track_id);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // rtcp-mux
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_RTCP_MUX);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->video.codec_list)
        {
            rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
            if (strcmp(codec->name, "rtx")) {
                // rtpmap
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPMAP, codec->pt, codec->name,
                               codec->clock_rate);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;

                // rtcp ccm fir
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_FIR, codec->pt);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;

                // rtcp nack
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_NACK, codec->pt);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;

                // rtcp pli
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_PLI, codec->pt);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;

                // attr
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_ATTR, codec->pt,
                               codec->profile_level_id);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;
            } else {
                ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_RTX, codec->pt, codec->clock_rate,
                               codec->pt, codec->original_pt);
                if (ret < 0 || ret >= remain) {
                    return -1;
                }
                already += ret;
                remain -= ret;
            }
        }
    } else {
        // rtpmap
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPMAP, sdp->video.negotiated_codec.pt,
                       sdp->video.negotiated_codec.name, sdp->video.negotiated_codec.clock_rate);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;

        // rtcp ccm fir
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_FIR, sdp->video.negotiated_codec.pt);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;

        // rtcp nack
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_NACK, sdp->video.negotiated_codec.pt);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;

        // rtcp pli
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_RTPFB_PLI, sdp->video.negotiated_codec.pt);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;

        // attr
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_VIDEO_ATTR, sdp->video.negotiated_codec.pt,
                       sdp->video.negotiated_codec.profile_level_id);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;

        if (sdp->video.rtx_mode == RTX_MODE_SSRC_MULTIPLEX) {
            // rtx
            ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_RTX, sdp->video.rtx_codec.pt,
                           sdp->video.rtx_codec.clock_rate, sdp->video.rtx_codec.pt, sdp->video.negotiated_codec.pt);
            if (ret < 0 || ret >= remain) {
                return -1;
            }
            already += ret;
            remain -= ret;
        }
    }

    if (sdp->video.rtx_mode == RTX_MODE_SSRC_MULTIPLEX) {
        // ssrc group
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_SSRC_GROUP, sdp->video.negotiated_codec.ssrc,
                       sdp->video.rtx_codec.ssrc, sdp->video.negotiated_codec.ssrc, sdp->cname,
                       sdp->video.rtx_codec.ssrc, sdp->cname);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;
    } else {
        // ssrc
        ret =
            snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_SSRC, sdp->video.negotiated_codec.ssrc, sdp->cname);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;
    }

    return already;
}

static int tuya_p2p_rtc_sdp_encode_media_tuya(rtc_sdp_t *sdp, char *type, char *mid, char *buf, int size)
{
    (void)sdp;
    (void)type;
    int ret;
    int index = 0;
    int already = 0;
    int remain = size;

    char pt[128] = {0};
    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->tuya.codec_list)
        {
            rtc_tuya_codec_t *codec = QUEUE_DATA(q, rtc_tuya_codec_t, queue);
            ret = snprintf(pt + index, sizeof(pt) - index, " %d", codec->pt);
            if (ret < 0 || ret >= (int)sizeof(pt) - index) {
                return -1;
            }
            index += ret;
        }
    } else {
        ret = snprintf(pt, sizeof(pt), " %d", sdp->tuya.negotiated_codec.pt);
        if (ret < 0 || ret >= (int)sizeof(pt)) {
            return -1;
        }
    }

    // m line
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_HEADER, "application", "tuya", pt);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // ice
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_TRANSPORT, sdp->ufrag, sdp->password);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // aes key
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_APPLICATION_KEY, sdp->aes_key);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // mid
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_MID, mid);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    // rtpmap
    if (strcmp(type, "offer") == 0) {
        QUEUE *q;
        QUEUE_FOREACH(q, &sdp->tuya.codec_list)
        {
            rtc_tuya_codec_t *codec = QUEUE_DATA(q, rtc_tuya_codec_t, queue);
            ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_APPLICATION_RTPMAP, codec->pt, codec->name,
                           codec->channel_number);
            if (ret < 0 || ret >= remain) {
                return -1;
            }
            already += ret;
            remain -= ret;
        }
    } else {
        ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_APPLICATION_RTPMAP, sdp->tuya.negotiated_codec.pt,
                       sdp->tuya.negotiated_codec.name, sdp->tuya.negotiated_codec.channel_number);
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        already += ret;
        remain -= ret;
    }

    // ssrc
    ret = snprintf(buf + already, remain, SDP_TEMPLATE_PART_MEDIA_SSRC, sdp->video.negotiated_codec.ssrc, sdp->cname);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    already += ret;
    remain -= ret;

    return already;
}

static int tuya_p2p_rtc_sdp_set_original_pt(rtc_sdp_t *sdp, int pt, int original_pt)
{
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->video.codec_list)
    {
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        if (codec->pt == pt) {
            codec->original_pt = original_pt;
        }
    }
    return 0;
}

int tuya_p2p_rtc_sdp_init(rtc_sdp_t *sdp, char *session_id, char *local_id, char *fingerprint, char *ufrag,
                          char *password, tuya_p2p_rtc_dtls_role_e dtls_role)
{
    memset(sdp, 0, sizeof(*sdp));
    snprintf(sdp->wms_id, sizeof(sdp->wms_id), "%s", session_id);
    snprintf(sdp->cname, sizeof(sdp->cname), "%s", local_id);
    snprintf(sdp->fingerprint, sizeof(sdp->fingerprint), "%s", fingerprint);
    sdp->dtls_role = dtls_role;
    if (ufrag != NULL && password != NULL) {
        snprintf(sdp->ufrag, sizeof(sdp->ufrag), "%s", ufrag);
        snprintf(sdp->password, sizeof(sdp->password), "%s", password);
    }

    QUEUE_INIT(&sdp->media_info_list.queue);
    QUEUE_INIT(&sdp->candidates.queue);

    // audio
    tuya_p2p_misc_rand_string(sdp->audio.track_id, 32);
    sdp->audio.rtx_mode = RTX_MODE_DISABLED;
    sdp->audio.direction = MEDIA_DIRECTION_SENDRECV;
    sdp->audio.rtx_codec.pt = -1;
    QUEUE_INIT(&sdp->audio.codec_list);

    // video
    tuya_p2p_misc_rand_string(sdp->video.track_id, 32);
    sdp->video.rtx_mode = RTX_MODE_REPLAY;
    sdp->video.direction = MEDIA_DIRECTION_SENDONLY;
    sdp->video.rtx_codec.pt = -1;
    QUEUE_INIT(&sdp->video.codec_list);

    // tuya
    QUEUE_INIT(&sdp->tuya.codec_list);
    sdp->inited = 1;
    return 0;
}

int tuya_p2p_rtc_sdp_deinit(rtc_sdp_t *sdp)
{
    if (sdp->inited == 0) {
        return 0;
    }

    while (!QUEUE_EMPTY(&sdp->candidates.queue)) {
        QUEUE *q = QUEUE_HEAD(&sdp->candidates.queue);
        QUEUE_REMOVE(q);
        rtc_cand_t *cand = QUEUE_DATA(q, rtc_cand_t, queue);
        free(cand);
    }
    while (!QUEUE_EMPTY(&sdp->media_info_list.queue)) {
        QUEUE *q = QUEUE_HEAD(&sdp->media_info_list.queue);
        QUEUE_REMOVE(q);
        media_info_t *info = QUEUE_DATA(q, media_info_t, queue);
        free(info);
    }
    while (!QUEUE_EMPTY(&sdp->audio.codec_list)) {
        QUEUE *q = QUEUE_HEAD(&sdp->audio.codec_list);
        QUEUE_REMOVE(q);
        rtc_audio_codec_t *codec = QUEUE_DATA(q, rtc_audio_codec_t, queue);
        free(codec);
    }
    while (!QUEUE_EMPTY(&sdp->video.codec_list)) {
        QUEUE *q = QUEUE_HEAD(&sdp->video.codec_list);
        QUEUE_REMOVE(q);
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        free(codec);
    }
    while (!QUEUE_EMPTY(&sdp->tuya.codec_list)) {
        QUEUE *q = QUEUE_HEAD(&sdp->tuya.codec_list);
        QUEUE_REMOVE(q);
        rtc_tuya_codec_t *codec = QUEUE_DATA(q, rtc_tuya_codec_t, queue);
        free(codec);
    }
    return 0;
}

int tuya_p2p_rtc_sdp_set_aes_key(rtc_sdp_t *sdp, unsigned char *aes_key, uint32_t len)
{
    if (len * 2 >= sizeof(sdp->aes_key)) {
        return -1;
    }

    memset(sdp->aes_key, 0, sizeof(sdp->aes_key));
    uint32_t i;
    for (i = 0; i < len; i++) {
        sdp->aes_key[2 * i] = tuya_p2p_misc_hex_to_char((aes_key[i] & 0xf0) >> 4);
        sdp->aes_key[2 * i + 1] = tuya_p2p_misc_hex_to_char(aes_key[i] & 0x0f);
    }
    return 0;
}

int tuya_p2p_rtc_sdp_get_aes_key(rtc_sdp_t *sdp, unsigned char *aes_key, uint32_t len)
{
    if (len * 2 >= sizeof(sdp->aes_key)) {
        return -1;
    }
    uint32_t i;
    for (i = 0; i < len; i++) {
        aes_key[i] = tuya_p2p_misc_char_to_hex(sdp->aes_key[2 * i]) << 4;
        aes_key[i] |= tuya_p2p_misc_char_to_hex(sdp->aes_key[2 * i + 1]);
    }
    return 0;
}

int tuya_p2p_rtc_sdp_set_dtls_cert_fingerprint(rtc_sdp_t *sdp, char *fingerprint)
{
    snprintf(sdp->fingerprint, sizeof(sdp->fingerprint), "%s", fingerprint);
    return 0;
}

int tuya_p2p_rtc_sdp_add_media(rtc_sdp_t *sdp, char *mid, char *type)
{
    media_info_t *m = (media_info_t *)malloc(sizeof(media_info_t));
    if (m == NULL) {
        return -1;
    }
    memset(m, 0, sizeof(*m));
    snprintf(m->mid, sizeof(m->mid), "%s", mid);
    snprintf(m->type, sizeof(m->type), "%s", type);
    QUEUE_INSERT_TAIL(&sdp->media_info_list.queue, &m->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_set_media_type(rtc_sdp_t *sdp, char *mid, char *type)
{
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->media_info_list.queue)
    {
        media_info_t *m = QUEUE_DATA(q, media_info_t, queue);
        if (strcmp(m->mid, mid) == 0) {
            snprintf(m->type, sizeof(m->type), "%s", type);
        }
    }
    return 0;
}

int tuya_p2p_rtc_sdp_add_audio_codec(rtc_sdp_t *sdp, char *name, int pt, uint32_t ssrc, int sample_rate,
                                     int channel_number)
{
    if (pt != 0) {
        return -1;
    }
    // printf("add audio codec: %s %d %u %d %d\n", name, pt, ssrc, sample_rate, channel_number);
    rtc_audio_codec_t *codec = (rtc_audio_codec_t *)malloc(sizeof(rtc_audio_codec_t));
    if (codec == NULL) {
        return -1;
    }
    memset(codec, 0, sizeof(*codec));

    if (name == NULL) {
        rtc_audio_codec_t *default_codec;
        default_codec = tuya_p2p_rtc_sdp_find_default_audio_codec(pt);
        if (default_codec == NULL) {
            if (codec != NULL) {
                free(codec);
            }
            return -1;
        }
        snprintf(codec->name, sizeof(codec->name), "%s", default_codec->name);
        codec->pt = default_codec->pt;
        codec->ssrc = default_codec->ssrc;
        codec->sample_rate = default_codec->sample_rate;
        codec->channel_number = default_codec->channel_number;
    } else {
        snprintf(codec->name, sizeof(codec->name), "%s", name);
        codec->pt = pt;
        codec->ssrc = ssrc;
        codec->sample_rate = sample_rate;
        codec->channel_number = channel_number;
    }

    QUEUE_INSERT_TAIL(&sdp->audio.codec_list, &codec->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_add_video_codec(rtc_sdp_t *sdp, char *name, int pt, uint32_t ssrc, int clock_rate,
                                     char *profile_level_id)
{
    rtc_video_codec_t *codec = (rtc_video_codec_t *)malloc(sizeof(rtc_video_codec_t));
    if (codec == NULL) {
        return -1;
    }
    memset(codec, 0, sizeof(*codec));

    snprintf(codec->name, sizeof(codec->name), "%s", name);
    codec->original_pt = -1;
    codec->pt = pt;
    codec->ssrc = ssrc;
    codec->clock_rate = clock_rate;
    snprintf(codec->profile_level_id, sizeof(codec->profile_level_id), "%s", profile_level_id);

    QUEUE_INSERT_TAIL(&sdp->video.codec_list, &codec->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_add_video_rtx_codec(rtc_sdp_t *sdp, int origin_pt, int pt, uint32_t ssrc, int clock_rate)
{
    rtc_video_codec_t *codec = (rtc_video_codec_t *)malloc(sizeof(rtc_video_codec_t));
    if (codec == NULL) {
        return -1;
    }
    memset(codec, 0, sizeof(*codec));

    snprintf(codec->name, sizeof(codec->name), "%s", "rtx");
    codec->original_pt = origin_pt;
    codec->pt = pt;
    codec->ssrc = ssrc;
    codec->clock_rate = clock_rate;

    QUEUE_INSERT_TAIL(&sdp->video.codec_list, &codec->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_add_tuya_codec(rtc_sdp_t *sdp, char *name, int pt, int channel_number)
{
    rtc_tuya_codec_t *codec = (rtc_tuya_codec_t *)malloc(sizeof(rtc_tuya_codec_t));
    if (codec == NULL) {
        return -1;
    }
    memset(codec, 0, sizeof(*codec));
    snprintf(codec->name, sizeof(codec->name), "%s", name);
    codec->pt = pt;
    codec->channel_number = channel_number;

    QUEUE_INSERT_TAIL(&sdp->tuya.codec_list, &codec->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_add_candidate(rtc_sdp_t *sdp, char *candidate)
{
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->candidates.queue)
    {
        rtc_cand_t *cand = QUEUE_DATA(q, rtc_cand_t, queue);
        if (strcmp(cand->str, candidate) == 0) {
            return 0;
        }
    }

    rtc_cand_t *cand = (rtc_cand_t *)malloc(sizeof(rtc_cand_t));
    if (cand == NULL) {
        return -1;
    }
    memset(cand, 0, sizeof(*cand));
    snprintf(cand->str, sizeof(cand->str), "%s", candidate);
    cand->time_ms = tuya_p2p_misc_get_timestamp_ms();
    QUEUE_INSERT_TAIL(&sdp->candidates.queue, &cand->queue);
    return 0;
}

int tuya_p2p_rtc_sdp_update_audio_codec(rtc_sdp_t *sdp, int pt, char *name, char *sample_rate, char *channel_number)
{
    tuya_p2p_log_debug("update audio codec: pt = %d, codec = %s, %s, %s\n", pt, name, sample_rate, channel_number);
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->audio.codec_list)
    {
        rtc_audio_codec_t *codec = QUEUE_DATA(q, rtc_audio_codec_t, queue);
        if (codec->pt == pt) {
            if (name != NULL) {
                snprintf(codec->name, sizeof(codec->name), "%s", name);
            }
            if (sample_rate != NULL) {
                codec->sample_rate = atoi(sample_rate);
            }
            if (channel_number != NULL) {
                codec->channel_number = atoi(channel_number);
            } else {
                codec->channel_number = 1;
            }
            break;
        }
    }
    return 0;
}

int tuya_p2p_rtc_sdp_update_video_codec(rtc_sdp_t *sdp, int pt, char *name, char *clock_rate)
{
    tuya_p2p_log_debug("update video codec: pt = %d, codec = %s, %s\n", pt, name, clock_rate);
    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->video.codec_list)
    {
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        if (codec->pt == pt) {
            if (name != NULL) {
                snprintf(codec->name, sizeof(codec->name), "%s", name);
            }
            if (clock_rate != NULL) {
                codec->clock_rate = atoi(clock_rate);
            }
            break;
        }
    }
    return 0;
}

int tuya_p2p_rtc_sdp_encode(rtc_sdp_t *sdp, char *type, char *buf, int size)
{
    int ret;
    int already = 0;
    int remain = size;
    ret = tuya_p2p_rtc_sdp_encode_session(sdp, buf + already, remain);
    if (ret < 0 || ret >= remain) {
        return -1;
    }
    remain -= ret;
    already += ret;

    QUEUE *q;
    QUEUE_FOREACH(q, &sdp->media_info_list.queue)
    {
        media_info_t *info = QUEUE_DATA(q, media_info_t, queue);
        if (strcmp(info->type, "audio") == 0) {
            ret = tuya_p2p_rtc_sdp_encode_media_audio(sdp, type, info->mid, buf + already, remain);
        } else if (strcmp(info->type, "video") == 0) {
            ret = tuya_p2p_rtc_sdp_encode_media_video(sdp, type, info->mid, buf + already, remain);
        } else if (strcmp(info->type, "tuya") == 0) {
            ret = tuya_p2p_rtc_sdp_encode_media_tuya(sdp, type, info->mid, buf + already, remain);
        } else {
            ret = 0;
        }
        if (ret < 0 || ret >= remain) {
            return -1;
        }
        remain -= ret;
        already += ret;
    }

    return already;
}

int tuya_p2p_rtc_sdp_decode(rtc_sdp_t *sdp, char *buf)
{
    char *lasts = NULL;
    char *p = strtok_r(buf, "\r\n", &lasts);
    if (p == NULL) {
        return 0;
    }
    char m = '0';
    while (1) {
        p = strtok_r(NULL, "\r\n", &lasts);
        if (p == NULL) {
            break;
        }
        if (strncmp(p, "a=msid-semantic:", strlen("a=msid-semantic:")) == 0) {
            p += strlen("a=msid-semantic:");
            char wms_name[65] = {0};
            char wms_id[65] = {0};
            int cnt = sscanf(p, "%s %s", wms_name, wms_id);
            if (cnt != 2 || strcmp(wms_name, "WMS") != 0) {
                continue;
            }
            snprintf(sdp->wms_id, sizeof(sdp->wms_id), "%s", wms_id);
            continue;
        }
        if (strncmp(p, "a=msid:", strlen("a=msid:")) == 0) {
            p += strlen("a=msid:");
            char msid[65] = {0};
            char track_id[65] = {0};
            int cnt = sscanf(p, "%s %s", msid, track_id);
            if (cnt != 2 || strcmp(msid, sdp->wms_id) != 0) {
                continue;
            }
            if (m == 'a') {
                snprintf(sdp->audio.track_id, sizeof(sdp->audio.track_id), "%s", track_id);
            } else if (m == 'v') {
                snprintf(sdp->video.track_id, sizeof(sdp->video.track_id), "%s", track_id);
            }
            continue;
        }
        if (strncmp(p, "a=group:BUNDLE", strlen("a=group:BUNDLE")) == 0) {
            p += strlen("a=group:BUNDLE");
            char m1[65] = {0};
            char m2[65] = {0};
            char m3[65] = {0};
            int cnt = sscanf(p, "%s %s %s", m1, m2, m3);
            if (cnt >= 1) {
                tuya_p2p_rtc_sdp_add_media(sdp, m1, "");
            }
            if (cnt >= 2) {
                tuya_p2p_rtc_sdp_add_media(sdp, m2, "");
            }
            if (cnt >= 3) {
                tuya_p2p_rtc_sdp_add_media(sdp, m3, "");
            }

            continue;
        }
        if (strncmp(p, "m=audio", strlen("m=audio")) == 0) {
            m = 'a';
            char tmp[128] = {0};
            char *ptmp = strstr(p, "SAVPF");
            if (ptmp != NULL) {
                snprintf(tmp, sizeof(tmp), "%s", ptmp + strlen("SAVPF") + 1);
            }
            char *tmplasts = NULL;
            char *pt = strtok_r(tmp, " ", &tmplasts);
            while (pt != NULL) {
                tuya_p2p_rtc_sdp_add_audio_codec(sdp, NULL, atoi(pt), 0, 0, 0);
                pt = strtok_r(NULL, " ", &tmplasts);
            }
            continue;
        }
        if (strncmp(p, "m=video", strlen("m=video")) == 0) {
            m = 'v';
            char tmp[128] = {0};
            char *ptmp = strstr(p, "SAVPF");
            if (ptmp != NULL) {
                snprintf(tmp, sizeof(tmp), "%s", ptmp + strlen("SAVPF") + 1);
            }
            char *tmplasts = NULL;
            char *pt = strtok_r(tmp, " ", &tmplasts);
            while (pt != NULL) {
                tuya_p2p_rtc_sdp_add_video_codec(sdp, "", atoi(pt), 0, 0, "");
                pt = strtok_r(NULL, " ", &tmplasts);
            }
            continue;
        }
        if (strncmp(p, "m=application", strlen("m=application")) == 0) {
            m = 't';
            char tmp[128] = {0};
            char *ptmp = strstr(p, "tuya");
            if (ptmp != NULL) {
                snprintf(tmp, sizeof(tmp), "%s", ptmp + strlen("tuya") + 1);
            }
            char *tmplasts = NULL;
            char *pt = strtok_r(tmp, " ", &tmplasts);
            while (pt != NULL) {
                tuya_p2p_rtc_sdp_add_tuya_codec(sdp, "", atoi(pt), 0);
                pt = strtok_r(NULL, " ", &tmplasts);
            }
            continue;
        }
        if (strncmp(p, "a=rtpmap:", strlen("a=rtpmap:")) == 0) {
            p += strlen("a=rtpmap:");
            int pt = atoi(p);

            char *p1 = strstr(p, " ");
            char *p2 = NULL;
            char *p3 = NULL;
            if (p1 != NULL) {
                p1 += 1;

                p2 = strstr(p1, "/");
                if (p2 != NULL) {
                    *p2 = '\0';
                    p2 += 1;

                    p3 = strstr(p2, "/");
                    if (p3 != NULL) {
                        *p3 = '\0';
                        p3 += 1;
                    }
                }
            }

            if (m == 'a') {
                tuya_p2p_rtc_sdp_update_audio_codec(sdp, pt, p1, p2, p3);
            } else if (m == 'v') {
                tuya_p2p_rtc_sdp_update_video_codec(sdp, pt, p1, p2);
            } else if (m == 't') {
            } else {
                tuya_p2p_log_warn("got invalid rtpmap, m = %c\n", m);
            }
            continue;
        }
        if (strncmp(p, "a=fmtp:", strlen("a=fmtp:")) == 0) {
            p += strlen("a=fmtp:");
            char str_pt[32] = {0};
            char str_attr[256] = {0};
            int cnt = sscanf(p, "%s %s", str_pt, str_attr);
            if (cnt == 2) {
                if (strncmp(str_attr, "apt=", strlen("apt=")) == 0) {
                    int pt = atoi(str_pt);
                    int original_pt = atoi(str_attr + strlen("apt="));
                    tuya_p2p_rtc_sdp_set_original_pt(sdp, pt, original_pt);
                }
            }
        }

        if (strncmp(p, "a=ssrc-group:FID", strlen("a=ssrc-group:FID")) == 0) {
            char *p1 = p + strlen("a=ssrc-group:FID");
            char ssrc[65] = {0};
            char ssrc_rtx[65] = {0};
            int cnt = sscanf(p1, "%s %s", ssrc, ssrc_rtx);
            if (cnt == 2) {
                if (m == 'a') {
                    sdp->audio.negotiated_codec.ssrc = strtoul(ssrc, NULL, 10);
                    sdp->audio.rtx_codec.ssrc = strtoul(ssrc_rtx, NULL, 10);
                } else if (m == 'v') {
                    sdp->video.negotiated_codec.ssrc = strtoul(ssrc, NULL, 10);
                    sdp->video.rtx_codec.ssrc = strtoul(ssrc_rtx, NULL, 10);
                } else {
                    // do nothing
                }
            }
            continue;
        }
        if (strncmp(p, "a=ssrc:", strlen("a=ssrc:")) == 0) {
            char *p1 = p + strlen("a=ssrc:");
            char *p2 = strstr(p1, " ");
            if (p2 != NULL) {
                *p2 = '\0';
                p2 += 1;

                if (strncmp(p2, "cname:", strlen("cname:")) == 0) {
                    p2 = p2 + strlen("cname:");
                } else {
                    p2 = NULL;
                }
            }

            if (p1 != NULL) {
                if (m == 'a') {
                    if (sdp->audio.negotiated_codec.ssrc == 0) {
                        sdp->audio.negotiated_codec.ssrc = strtoul(p1, NULL, 10);
                    } else if (sdp->audio.rtx_codec.ssrc == 0) {
                        sdp->audio.rtx_codec.ssrc = strtoul(p1, NULL, 10);
                    }
                } else if (m == 'v') {
                    if (sdp->video.negotiated_codec.ssrc == 0) {
                        sdp->video.negotiated_codec.ssrc = strtoul(p1, NULL, 10);
                    } else if (sdp->video.rtx_codec.ssrc == 0) {
                        sdp->video.rtx_codec.ssrc = strtoul(p1, NULL, 10);
                    }
                } else {
                    // do nothing
                }
            }
            if (p2 != NULL) {
                snprintf(sdp->cname, sizeof(sdp->cname), "%s", p2);
            }
            continue;
        }

        if (strncmp(p, "a=mid:", strlen("a=mid:")) == 0) {
            char *p1 = p + strlen("a=mid:");
            if (p1 != NULL) {
                if (m == 'a') {
                    tuya_p2p_rtc_sdp_set_media_type(sdp, p1, "audio");
                } else if (m == 'v') {
                    tuya_p2p_rtc_sdp_set_media_type(sdp, p1, "video");
                } else {
                    tuya_p2p_rtc_sdp_set_media_type(sdp, p1, "tuya");
                }
            }
            continue;
        }

        if (strncmp(p, "a=ice-ufrag:", strlen("a=ice-ufrag:")) == 0) {
            snprintf(sdp->ufrag, sizeof(sdp->ufrag), "%s", p + strlen("a=ice-ufrag:"));
            continue;
        }
        if (strncmp(p, "a=ice-pwd:", strlen("a=ice-pwd:")) == 0) {
            snprintf(sdp->password, sizeof(sdp->password), "%s", p + strlen("a=ice-pwd:"));
            continue;
        }
        if (strncmp(p, "a=fingerprint:", strlen("a=fingerprint:")) == 0) {
            snprintf(sdp->fingerprint, sizeof(sdp->fingerprint), "%s", p + strlen("a=fingerprint:"));
            continue;
        }
        if (strncmp(p, "a=aes-key:", strlen("a=aes-key:")) == 0) {
            snprintf((char *)sdp->aes_key, sizeof(sdp->aes_key), "%s", p + strlen("a=aes-key:"));
            continue;
        }
        if (strncmp(p, "a=candidate:", strlen("a=candidate:")) == 0) {
            tuya_p2p_rtc_sdp_add_candidate(sdp, p);
            continue;
        }
    }

    if (!QUEUE_EMPTY(&sdp->candidates.queue)) {
        tuya_p2p_rtc_sdp_add_candidate(sdp, "");
    }

    return 0;
}

int tuya_p2p_rtc_sdp_negotiate(rtc_sdp_t *local_sdp, rtc_sdp_t *remote_sdp, char *type)
{
    QUEUE *q;
    if (strcmp(type, "offer") == 0) {
        memcpy(local_sdp->aes_key, remote_sdp->aes_key, sizeof(local_sdp->aes_key));

        QUEUE_FOREACH(q, &remote_sdp->media_info_list.queue)
        {
            media_info_t *m = QUEUE_DATA(q, media_info_t, queue);
            tuya_p2p_rtc_sdp_add_media(local_sdp, m->mid, m->type);
        }
    }

    // audio
    // printf("negotiate codec\n");
    QUEUE_FOREACH(q, &local_sdp->audio.codec_list)
    {
        rtc_audio_codec_t *codec = QUEUE_DATA(q, rtc_audio_codec_t, queue);
        // printf("local audio pt: %d\n", codec->pt);
        QUEUE *tmp_q;
        QUEUE_FOREACH(tmp_q, &remote_sdp->audio.codec_list)
        {
            rtc_audio_codec_t *tmp_codec = QUEUE_DATA(tmp_q, rtc_audio_codec_t, queue);
            // printf("nego audio: %d(%s:%d:%d:%u) - %d(%s:%d:%d:%u)\n",
            // codec->pt, codec->name, codec->sample_rate, codec->channel_number, codec->ssrc,
            // tmp_codec->pt, tmp_codec->name, tmp_codec->sample_rate, tmp_codec->channel_number, codec->ssrc);
            if ((strncmp(codec->name, tmp_codec->name, sizeof(codec->name)) == 0) &&
                (codec->sample_rate == tmp_codec->sample_rate) &&
                (codec->channel_number == tmp_codec->channel_number)) {
                local_sdp->audio.negotiated_codec.channel_number = codec->channel_number;
                local_sdp->audio.negotiated_codec.sample_rate = codec->sample_rate;
                local_sdp->audio.negotiated_codec.pt = tmp_codec->pt;
                local_sdp->audio.negotiated_codec.ssrc = codec->ssrc;
                snprintf(local_sdp->audio.negotiated_codec.name, sizeof(local_sdp->audio.negotiated_codec.name), "%s",
                         codec->name);
                remote_sdp->audio.negotiated_codec.channel_number = tmp_codec->channel_number;
                remote_sdp->audio.negotiated_codec.sample_rate = tmp_codec->sample_rate;
                remote_sdp->audio.negotiated_codec.pt = tmp_codec->pt;
                // remote_sdp->audio.negotiated_codec.ssrc = tmp_codec->ssrc;
                snprintf(remote_sdp->audio.negotiated_codec.name, sizeof(remote_sdp->audio.negotiated_codec.name), "%s",
                         tmp_codec->name);
                goto finish_audio;
            }
        }
    }

finish_audio:

    // video
    QUEUE_FOREACH(q, &local_sdp->video.codec_list)
    {
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        // printf("local video pt: %d\n", codec->pt);
        QUEUE *tmp_q;
        QUEUE_FOREACH(tmp_q, &remote_sdp->video.codec_list)
        {
            rtc_video_codec_t *tmp_codec = QUEUE_DATA(tmp_q, rtc_video_codec_t, queue);
            if ((strncmp(codec->name, tmp_codec->name, sizeof(codec->name)) == 0) &&
                (codec->clock_rate == tmp_codec->clock_rate)) {
                local_sdp->video.negotiated_codec.clock_rate = codec->clock_rate;
                local_sdp->video.negotiated_codec.pt = tmp_codec->pt;
                local_sdp->video.negotiated_codec.ssrc = codec->ssrc;
                local_sdp->video.negotiated_codec.original_pt = -1;
                snprintf(local_sdp->video.negotiated_codec.profile_level_id,
                         sizeof(local_sdp->video.negotiated_codec.profile_level_id), "%s", codec->profile_level_id);
                snprintf(local_sdp->video.negotiated_codec.name, sizeof(local_sdp->video.negotiated_codec.name), "%s",
                         codec->name);
                remote_sdp->video.negotiated_codec.clock_rate = tmp_codec->clock_rate;
                remote_sdp->video.negotiated_codec.pt = tmp_codec->pt;
                // remote_sdp->video.negotiated_codec.ssrc = tmp_codec->ssrc;
                remote_sdp->video.negotiated_codec.original_pt = -1;
                snprintf(remote_sdp->video.negotiated_codec.profile_level_id,
                         sizeof(remote_sdp->video.negotiated_codec.profile_level_id), "%s",
                         tmp_codec->profile_level_id);
                snprintf(remote_sdp->video.negotiated_codec.name, sizeof(remote_sdp->video.negotiated_codec.name), "%s",
                         tmp_codec->name);
                goto finish_video;
            }
        }
    }

finish_video:

    // video rtx codec
    QUEUE_FOREACH(q, &remote_sdp->video.codec_list)
    {
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        // printf("remote video codec: %s %d %d (negotiated_codec pt = %d)\n",
        // codec->name, codec->pt, codec->original_pt, local_sdp->video.negotiated_codec.pt);
        if (strcmp(codec->name, "rtx") == 0 && codec->original_pt == remote_sdp->video.negotiated_codec.pt) {
            remote_sdp->video.rtx_codec.pt = codec->pt;
            remote_sdp->video.rtx_codec.ssrc = codec->ssrc;
            remote_sdp->video.rtx_mode = RTX_MODE_SSRC_MULTIPLEX;
        }
    }
    QUEUE_FOREACH(q, &local_sdp->video.codec_list)
    {
        rtc_video_codec_t *codec = QUEUE_DATA(q, rtc_video_codec_t, queue);
        // printf("local video codec: %s %d %d (negotiated_codec pt = %d)\n",
        // codec->name, codec->pt, codec->original_pt, local_sdp->video.negotiated_codec.pt);
        if (strcmp(codec->name, "rtx") == 0) {
            local_sdp->video.rtx_codec.ssrc = codec->ssrc;
            local_sdp->video.rtx_codec.clock_rate = codec->clock_rate;
        }
    }
    local_sdp->video.rtx_codec.pt = remote_sdp->video.rtx_codec.pt;
    local_sdp->video.rtx_mode = remote_sdp->video.rtx_mode;

    // tuya media (hardcode)
    local_sdp->tuya.negotiated_codec.channel_number = 3;
    snprintf(local_sdp->tuya.negotiated_codec.name, sizeof(local_sdp->tuya.negotiated_codec.name), "AES/KCP");
    local_sdp->tuya.negotiated_codec.pt = 6001;
    remote_sdp->tuya.negotiated_codec.channel_number = 3;
    snprintf(remote_sdp->tuya.negotiated_codec.name, sizeof(remote_sdp->tuya.negotiated_codec.name), "AES/KCP");
    remote_sdp->tuya.negotiated_codec.pt = 6001;

    return 0;
}
