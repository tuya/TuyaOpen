#ifndef __TUYA_RTP_H__
#define __TUYA_RTP_H__

#include <stdint.h>
#include <stdbool.h>

#pragma pack(1)

typedef enum {
    RTX_MODE_DISABLED,
    RTX_MODE_REPLAY,
    RTX_MODE_SSRC_MULTIPLEX,
    RTX_MODE_SESSION_MULTIPLEX
} tuya_p2p_rtp_rtx_mode_e;

typedef struct tuya_p2p_rtp_hdr {
#if defined(TUYA_P2P_IS_BIG_ENDIAN) && (TUYA_P2P_IS_BIG_ENDIAN != 0)
    uint16_t v : 2;  /**< packet type/version	    */
    uint16_t p : 1;  /**< padding flag		    */
    uint16_t x : 1;  /**< extension flag		    */
    uint16_t cc : 4; /**< CSRC count			    */
    uint16_t m : 1;  /**< marker bit			    */
    uint16_t pt : 7; /**< payload type		    */
#else
    uint16_t cc : 4; /**< CSRC count			    */
    uint16_t x : 1;  /**< header extension flag	    */
    uint16_t p : 1;  /**< padding flag		    */
    uint16_t v : 2;  /**< packet type/version	    */
    uint16_t pt : 7; /**< payload type		    */
    uint16_t m : 1;  /**< marker bit			    */
#endif
    uint16_t seq;  /**< sequence number		    */
    uint32_t ts;   /**< timestamp			    */
    uint32_t ssrc; /**< synchronization source	    */
} tuya_p2p_rtp_hdr_t;
#pragma pack()

typedef struct tuya_p2p_rtp_ext_hdr {
    uint16_t profile_data; /**< Profile data.	    */
    uint16_t length;       /**< Length.		    */
} tuya_p2p_rtp_ext_hdr_t;

typedef struct tuya_p2p_rtp_dec_hdr {
    /* RTP extension header output decode */
    tuya_p2p_rtp_ext_hdr_t *ext_hdr;
    uint32_t *ext;
    unsigned ext_len;
} tuya_p2p_rtp_dec_hdr_t;

typedef struct tuya_p2p_rtp_pkt {
    tuya_p2p_rtp_hdr_t **hdr;
    tuya_p2p_rtp_dec_hdr_t ext_hdr;
    unsigned payload_len;
    void *payload;
} tuya_p2p_rtp_pkt_t;

typedef struct tuya_p2p_rtp_seq_session {
    uint16_t max_seq;   /**< Highest sequence number heard	    */
    uint32_t cycles;    /**< Shifted count of seq number cycles */
    uint32_t base_seq;  /**< Base seq number		    */
    uint32_t bad_seq;   /**< Last 'bad' seq number + 1	    */
    uint32_t probation; /**< Sequ. packets till source is valid */
} tuya_p2p_rtp_seq_session_t;

typedef struct tuya_p2p_rtp_session {
    tuya_p2p_rtp_hdr_t out_hdr;          /**< Saved hdr for outgoing pkts.   */
    tuya_p2p_rtp_seq_session_t seq_ctrl; /**< Sequence number management.    */
    uint16_t out_pt;                     /**< Default outgoing payload type. */
    uint32_t out_extseq;                 /**< Outgoing extended seq #.	    */
    uint32_t peer_ssrc;                  /**< Peer SSRC.			    */
    uint32_t received;                   /**< Number of received packets.    */
} tuya_p2p_rtp_session_t;

typedef struct tuya_p2p_rtp_status {
    union {
        struct flag {
            int bad : 1;       /**< General flag to indicate that sequence is
                        bad, and application should not process
                        this packet. More information will be given
                        in other flags.			    */
            int badpt : 1;     /**< Bad payload type.			    */
            int badssrc : 1;   /**< Bad SSRC				    */
            int dup : 1;       /**< Indicates duplicate packet		    */
            int outorder : 1;  /**< Indicates out of order packet		    */
            int probation : 1; /**< Indicates that session is in probation
                        until more packets are received.	    */
            int restart : 1;   /**< Indicates that sequence number has made
                        a large jump, and internal base sequence
                        number has been adjusted.		    */
        } flag;                /**< Status flags.				    */

        uint16_t value; /**< Status value, to conveniently address all
                    flags.					    */

    } status; /**< Status information union.		    */

    uint16_t diff; /**< Sequence number difference from previous
        packet. Normally the value should be 1.
        Value greater than one may indicate packet
        loss. If packet with lower sequence is
        received, the value will be set to zero.
        If base sequence has been restarted, the
        value will be one.			    */
} tuya_p2p_rtp_status_t;

typedef struct tuya_p2p_rtp_session_setting {
    uint8_t flags;        /**< Bitmask flags to specify whether such
                       field is set. Bitmask contents are:
                   (bit #0 is LSB)
                   bit #0: default payload type
                   bit #1: sender SSRC
                   bit #2: sequence
                   bit #3: timestamp		    */
    int default_pt;       /**< Default payload type.		    */
    uint32_t sender_ssrc; /**< Sender SSRC.			    */
    uint16_t seq;         /**< Sequence.			    */
    uint32_t ts;          /**< Timestamp.			    */
} tuya_p2p_rtp_session_setting;

void tuya_p2p_rtp_dump_rtp_hdr(tuya_p2p_rtp_hdr_t *rtp_hdr);
void tuya_p2p_rtp_dump_rtp(tuya_p2p_rtp_pkt_t *pkt);

int32_t tuya_p2p_rtp_session_init(tuya_p2p_rtp_session_t *ses, int default_pt, uint32_t sender_ssrc);

int32_t tuya_p2p_rtp_session_init2(tuya_p2p_rtp_session_t *ses, tuya_p2p_rtp_session_setting settings);

int32_t tuya_p2p_rtp_encode_rtp(tuya_p2p_rtp_session_t *ses, int pt, int m, int payload_len, int ts_len,
                                const void **rtphdr, int *hdrlen);

int32_t tuya_p2p_rtp_decode_rtp(tuya_p2p_rtp_session_t *ses, const void *pkt, int pkt_len,
                                const tuya_p2p_rtp_hdr_t **hdr, const void **payload, unsigned *payloadlen);

int32_t tuya_p2p_rtp_decode_rtp2(tuya_p2p_rtp_session_t *ses, const void *pkt, int pkt_len,
                                 const tuya_p2p_rtp_hdr_t **hdr, tuya_p2p_rtp_dec_hdr_t *dec_hdr, const void **payload,
                                 unsigned *payloadlen);

void tuya_p2p_rtp_session_update(tuya_p2p_rtp_session_t *ses, const tuya_p2p_rtp_hdr_t *hdr,
                                 tuya_p2p_rtp_status_t *seq_st);

void tuya_p2p_rtp_session_update2(tuya_p2p_rtp_session_t *ses, const tuya_p2p_rtp_hdr_t *hdr,
                                  tuya_p2p_rtp_status_t *seq_st, bool check_pt);

void tuya_p2p_rtp_seq_init(tuya_p2p_rtp_seq_session_t *seq_ctrl, uint16_t seq);

void tuya_p2p_rtp_seq_update(tuya_p2p_rtp_seq_session_t *seq_ctrl, uint16_t seq, tuya_p2p_rtp_status_t *seq_status);

uint32_t tuya_p2p_rtp_get_timestamp(void *rtphdr);
int32_t tuya_p2p_rtp_set_timestamp(void *rtphdr, uint32_t ts);
uint16_t tuya_p2p_rtp_get_seq(void *rtphdr);

#endif
