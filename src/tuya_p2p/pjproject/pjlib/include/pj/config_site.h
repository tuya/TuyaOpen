/*
 * Put this file in pjlib/include/pj
 */

/* sample configure command:
   CFLAGS="-g -Wno-unused-label" ./aconfigure --enable-ext-sound --disable-speex-aec --disable-g711-codec
   --disable-l16-codec --disable-gsm-codec --disable-g722-codec --disable-g7221-codec --disable-speex-codec
   --disable-ilbc-codec --disable-opencore-amrnb --disable-sdl --disable-ffmpeg --disable-v4l2
 */

#define THIRD_PARTY_MEDIA 1

#if THIRD_PARTY_MEDIA

#endif /* THIRD_PARTY_MEDIA */

/* lwIP on TuyaOS: setsockopt(IP_TOS/SO_PRIORITY) often returns ENOSYS.
 * Use dummy QoS so TURN/STUN never depend on unsupported sockopts. */
#define PJ_QOS_IMPLEMENTATION PJ_QOS_DUMMY

/* RTOS: no native pthread event; disable pj event object */
#if defined(PJ_TUYAOS) && PJ_TUYAOS != 0
#define PJ_HAS_EVENT_OBJ 0
#endif
