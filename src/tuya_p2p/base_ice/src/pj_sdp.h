#ifndef PJ_SDP_H_
#define PJ_SDP_H_

#include <stdint.h>

int pj_sdp_init(char *session_id, char *local_id, char *fingerprint, char *ufrag, char *password);
int pj_sdp_deinit();
int pj_sdp_set_aes_key(unsigned char *aes_key, uint32_t len);
int pj_sdp_get_aes_key(unsigned char *aes_key, uint32_t len);

#endif /* PJ_SDP_H_ */
