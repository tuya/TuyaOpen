#ifndef PJ_ICE_H_
#define PJ_ICE_H_
#include <stdint.h>
#include <stdbool.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/ice_strans.h>

typedef struct pj_ice_cb {
    void (*ice_on_rx_data)(pj_ice_strans *ice_st, unsigned comp_id, void *buffer, pj_size_t size,
                           const pj_sockaddr_t *src_addr, unsigned src_addr_len);
    void (*ice_on_new_candidate)(pj_ice_strans *ice_st, const pj_ice_sess_cand *cand, pj_bool_t last);
    void (*ice_on_ice_complete)(pj_ice_strans *ice_st, pj_ice_strans_op op, pj_status_t status);
} pj_ice_cb_t;

typedef struct pj_ice_session_cfg {
    pj_ice_cb_t cb;
    unsigned rolechar; // Value: character 'o' represents Controlling role, other values represent Controlled role
    char *local_ufrag;
    char *local_passwd;
    char server_tokens[2048];
    void *user_data;
} pj_ice_session_cfg_t;

typedef struct pj_ice_session pj_ice_session_t;

bool pj_thread_register2();
int print_cand(char buffer[], unsigned maxlen, const pj_ice_sess_cand *cand);
int parse_cand(pj_pool_t *pool, const pj_str_t *orig_input, pj_ice_sess_cand *cand);

bool pj_ice_session_create(pj_ice_session_cfg_t *pCfg, pj_ice_session_t **ppIceSession);
bool pj_ice_session_destroy(pj_ice_session_t *pIceSession);
bool pj_ice_session_init(pj_ice_session_t *pIceSession, pj_ice_session_cfg_t *pCfg);
bool pj_ice_session_add_remote_candidate(pj_ice_session_t *pIceSession, pj_str_t *rem_ufrag, pj_str_t *rem_passwd,
                                         unsigned rcand_cnt, pj_ice_sess_cand rcand[], pj_bool_t rcand_end);
bool pj_ice_session_sendto(pj_ice_session_t *pIceSession, void *pkt, uint32_t len);
bool pj_ice_session_handle_events(pj_ice_session_t *pIceSession, unsigned max_msec, unsigned *p_count);

#endif /* PJ_ICE_H_ */