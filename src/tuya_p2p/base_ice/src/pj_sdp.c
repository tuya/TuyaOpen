#include "pj_sdp.h"

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

#define SDP_TEMPLATE_PART_MEDIA_MID "a=mid:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_APPLICATION_RTPMAP "a=rtpmap:%d %s %d\r\n"

#define SDP_TEMPLATE_PART_MEDIA_APPLICATION_KEY "a=aes-key:%s\r\n"

#define SDP_TEMPLATE_PART_MEDIA_SSRC "a=ssrc:%u cname:%s\r\n"

int pj_sdp_init(char *session_id, char *local_id, char *fingerprint, char *ufrag, char *password)
{
    return 0;
}

int pj_sdp_deinit()
{
    return 0;
}

int pj_sdp_set_aes_key(unsigned char *aes_key, uint32_t len)
{
    return 0;
}

int pj_sdp_get_aes_key(unsigned char *aes_key, uint32_t len)
{
    return 0;
}
