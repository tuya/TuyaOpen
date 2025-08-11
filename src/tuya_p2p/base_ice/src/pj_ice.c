#include "pj_ice.h"
#include "cJSON.h"

typedef struct tagIceWorkerThreadParam {
    pj_ice_session_t *pIceSession;
    pj_ice_strans_cfg *pCfg;
    bool bThreadQuitFlag;
} ICE_WORKER_THREAD_PARAM;

typedef struct pj_ice_session {
    pj_caching_pool cachePool;
    pj_pool_t *pPool;
    pj_thread_t *pThread;
    pj_bool_t bThreadQuitFlag;
    pj_ice_strans_cfg iceCfg;
    pj_ice_strans *pIceSTransport;
    pj_bool_t bLastCand;
    unsigned int uComponentCount;
    ICE_WORKER_THREAD_PARAM *pIceThreadParam;
} pj_ice_session_t;

bool g_bInited = false;
#define KA_INTERVAL 300
#define THIS_FILE   "pj_ice.c"
#define INDENT      "    "

#define PRINT(...)                                                                                                     \
    printed = pj_ansi_snprintf(p, maxlen - (p - buffer), __VA_ARGS__);                                                 \
    if (printed <= 0 || printed >= (int)(maxlen - (p - buffer)))                                                       \
        return -PJ_ETOOSMALL;                                                                                          \
    p += printed

void pj_print_error(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    // PJ_LOG(1, (THIS_FILE, "%s: %s", title, errmsg));
    return;
}

bool pj_thread_register2()
{
    pj_thread_desc desc;
    pj_thread_t *thread = 0;
    if (!pj_thread_is_registered()) {
        return (pj_thread_register(NULL, desc, &thread) == PJ_SUCCESS ? true : false);
    }
    return false;
}

bool is_ipv4(char *ip_str)
{
    pj_str_t ip = pj_str(ip_str);
    pj_sockaddr addr;
    if (pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &ip, &addr) != PJ_SUCCESS) {
        // Not a valid IP address
        return false;
    }
    return (addr.addr.sa_family == pj_AF_INET());
}

bool is_ipv6(char *ip_str)
{
    pj_str_t ip = pj_str(ip_str);
    pj_sockaddr addr;
    if (pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &ip, &addr) != PJ_SUCCESS) {
        // Not a valid IP address
        return false;
    }
    return (addr.addr.sa_family == pj_AF_INET6());
}

/*
 * This function checks for events from both timer and ioqueue (for
 * network events). It is invoked by the worker thread.
 */
bool pj_ice_session_handle_events(pj_ice_session_t *pIceSession, unsigned max_msec, unsigned *p_count)
{
    if (pIceSession == NULL) {
        return false;
    }

    enum { MAX_NET_EVENTS = 1 };
    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = {0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    pj_ice_strans_cfg *pIceStransCfg = &pIceSession->iceCfg;
    max_timeout.msec = max_msec;

    /* Poll the timer to run it and also to retrieve the earliest entry. */
    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll(pIceStransCfg->stun_cfg.timer_heap, &timeout);
    if (c > 0)
        count += c;

    /* timer_heap_poll should never ever returns negative value, or otherwise
     * ioqueue_poll() will block forever!
     */
    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000)
        timeout.msec = 999;

    /* compare the value with the timeout to wait from timer, and use the
     * minimum value.
     */
    if (PJ_TIME_VAL_GT(timeout, max_timeout))
        timeout = max_timeout;

    /* Poll ioqueue.
     * Repeat polling the ioqueue while we have immediate events, because
     * timer heap may process more than one events, so if we only process
     * one network events at a time (such as when IOCP backend is used),
     * the ioqueue may have trouble keeping up with the request rate.
     *
     * For example, for each send() request, one network event will be
     *   reported by ioqueue for the send() completion. If we don't poll
     *   the ioqueue often enough, the send() completion will not be
     *   reported in timely manner.
     */
    do {
        c = pj_ioqueue_poll(pIceStransCfg->stun_cfg.ioqueue, &timeout);
        if (c < 0) {
            pj_status_t err = pj_get_netos_error();
            if (err != PJ_SUCCESS)
                printf("pj_handle_events error: %d\n", err);
            pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
            if (p_count)
                *p_count = count;
            return false;
        } else if (c == 0) {
            break;
        } else {
            net_event_count += c;
            timeout.sec = timeout.msec = 0;
        }
    } while (c > 0 && net_event_count < MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count)
        *p_count = count;

    return true;
}

/*
 * This is the worker thread that polls event in the background.
 */
int ice_worker_thread(void *pParam)
{
    ICE_WORKER_THREAD_PARAM *pThis = (ICE_WORKER_THREAD_PARAM *)(pParam);
    if (pThis == NULL) {
        return -1;
    }
    while (!pThis->bThreadQuitFlag) {
        pj_ice_session_handle_events(pThis->pIceSession, 10, NULL);
    }
    return 0;
}

/* Utility to create a=candidate SDP attribute */
int print_cand(char buffer[], unsigned maxlen, const pj_ice_sess_cand *cand)
{
    char ipaddr[PJ_INET6_ADDRSTRLEN];
    char *p = buffer;
    int printed;

    PRINT("a=candidate:%.*s %u UDP %u %s %u typ ", (int)cand->foundation.slen, cand->foundation.ptr,
          (unsigned)cand->comp_id, cand->prio, pj_sockaddr_print(&cand->addr, ipaddr, sizeof(ipaddr), 0),
          (unsigned)pj_sockaddr_get_port(&cand->addr));

    PRINT("%s\r\n", pj_ice_get_cand_type_name(cand->type));

    if (p == buffer + maxlen)
        return -PJ_ETOOSMALL;

    *p = '\0';

    return (int)(p - buffer);
}

/* Parse a=candidate line */
int parse_cand(pj_pool_t *pool, const pj_str_t *orig_input, pj_ice_sess_cand *cand)
{
    pj_str_t token, delim, host;
    int af;
    pj_ssize_t found_idx;
    pj_status_t status = PJNATH_EICEINCANDSDP;

    pj_bzero(cand, sizeof(*cand));

    // PJ_UNUSED_ARG(obj_name);

    /* Foundation */
    delim = pj_str(" ");
    found_idx = pj_strtok(orig_input, &delim, &token, 0);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE foundation in candidate"));
        goto on_return;
    }
    if (pool) {
        pj_strdup(pool, &cand->foundation, &token);
    } else {
        cand->foundation = token;
    }

    /* Component ID */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE component ID in candidate"));
        goto on_return;
    }
    cand->comp_id = (pj_uint8_t)pj_strtoul(&token);

    /* Transport */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE transport in candidate"));
        goto on_return;
    }
    if (pj_stricmp2(&token, "UDP") != 0) {
        // TRACE__((obj_name, "Expecting ICE UDP transport only in candidate"));
        goto on_return;
    }

    /* Priority */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE priority in candidate"));
        goto on_return;
    }
    cand->prio = pj_strtoul(&token);

    /* Host */
    found_idx = pj_strtok(orig_input, &delim, &host, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE host in candidate"));
        goto on_return;
    }
    /* Detect address family */
    if (pj_strchr(&host, ':'))
        af = pj_AF_INET6();
    else
        af = pj_AF_INET();
    /* Assign address */
    if (pj_sockaddr_init(af, &cand->addr, &host, 0)) {
        // TRACE__((obj_name, "Invalid ICE candidate address"));
        goto on_return;
    }

    /* Port */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + host.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE port number in candidate"));
        goto on_return;
    }
    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)pj_strtoul(&token));

    /* typ */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }
    if (pj_stricmp2(&token, "typ") != 0) {
        // TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }

    /* candidate type */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        // TRACE__((obj_name, "Expecting ICE candidate type in candidate"));
        goto on_return;
    }

    if (pj_stricmp2(&token, "host") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_HOST;
    } else if (pj_stricmp2(&token, "srflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_SRFLX;
    } else if (pj_stricmp2(&token, "relay") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_RELAYED;
    } else if (pj_stricmp2(&token, "prflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_PRFLX;
    } else {
        printf("Invalid ICE candidate type %.*s in candidate", (int)token.slen, token.ptr);
        goto on_return;
    }

    return 0;

on_return:
    return -1;
}

int pj_sdp_token_url_parse(const char *token_url, const char *type, char **addr, size_t *addr_len, uint16_t *port)
{
    if (token_url == NULL || type == NULL || addr == NULL || addr_len == NULL || port == NULL) {
        printf("invalid param\n");
        return -1;
    }

    char *paddr = (char*)token_url + strlen(type);
    char *pport = NULL;
    int i;
    for (i = strlen(paddr); i > 0; i--) {
        if (paddr[i] == ':') {
            pport = paddr + i + 1;
            break;
        }
    }

    if (pport == NULL) {
        printf("invalid token url\n");
        return -1;
    }

    *port = atoi(pport);
    *addr_len = pport - paddr - 1;
    if (paddr[0] == '[') {
        paddr += 1;
        *addr_len -= 2;
    }
    *addr = paddr;
    return 0;
}

bool pj_ice_session_create(pj_ice_session_cfg_t *pCfg, pj_ice_session_t **ppIceSession)
{
    pj_init();
    pjlib_util_init();
    pjnath_init();
    pj_log_set_level(0);

    pj_ice_session_t *pIceSession = malloc(sizeof(pj_ice_session_t));
    pIceSession->pPool = NULL;
    pIceSession->pThread = NULL;
    pIceSession->bThreadQuitFlag = false;
    pIceSession->pIceSTransport = NULL;
    pIceSession->bLastCand = false;
    pIceSession->uComponentCount = 1;
    pj_caching_pool_init(&pIceSession->cachePool, NULL, 0);
    pj_ice_strans_cfg_default(&pIceSession->iceCfg);
    pIceSession->iceCfg.stun_cfg.pf = &pIceSession->cachePool.factory;
    pIceSession->pPool = pj_pool_create(&pIceSession->cachePool.factory, "ice_Pool", 512, 512, NULL);
    pj_timer_heap_create(pIceSession->pPool, 100, &pIceSession->iceCfg.stun_cfg.timer_heap);
    pj_ioqueue_create(pIceSession->pPool, 16, &pIceSession->iceCfg.stun_cfg.ioqueue);

    pj_ice_strans_cfg *pIceCfg = &pIceSession->iceCfg;
    pIceCfg->opt.aggressive = PJ_FALSE;
    pIceCfg->opt.trickle = PJ_ICE_SESS_TRICKLE_FULL;
    ICE_WORKER_THREAD_PARAM *pIceThreadParam = malloc(sizeof(ICE_WORKER_THREAD_PARAM));
    pIceThreadParam->pIceSession = pIceSession;
    pIceThreadParam->pCfg = pIceCfg;
    pIceThreadParam->bThreadQuitFlag = false;
    pIceSession->pIceThreadParam = pIceThreadParam;
    // pj_thread_create(pIceSession->pPool, "ice_worker_thread", &ice_worker_thread, pIceThreadParam, 0, 0,
    // &pIceSession->pThread);
    //  pj_str_t szDNSServers[2];
    //  szDNSServers[0] = pj_str((char*)"8.8.8.8");
    //  szDNSServers[1] = pj_str((char*)"144.144.144.144");
    //  pj_dns_resolver_create(&pIceCfg->cachePool.factory, "resolver", 0, pIceCfg->iceCfg.stun_cfg.timer_heap,
    //  pIceCfg->iceCfg.stun_cfg.ioqueue, &pIceCfg->iceCfg.resolver); pj_dns_resolver_set_ns(pIceCfg->iceCfg.resolver,
    //  1, szDNSServers, NULL);
    *ppIceSession = pIceSession;
    return true;
}

bool pj_ice_session_destroy(pj_ice_session_t *pIceSession)
{
    pj_status_t status = PJ_SUCCESS;
    g_bInited = false;

    pj_thread_register2();
    pIceSession->pIceThreadParam->bThreadQuitFlag = true;
    if (pIceSession->pThread != NULL) {
        pj_thread_join(pIceSession->pThread);
        pj_thread_destroy(pIceSession->pThread);
        pIceSession->pThread = NULL;
    }
    pj_pool_release(pIceSession->pPool);
    free(pIceSession->pIceThreadParam);
    pIceSession->pIceThreadParam = NULL;

    pj_ice_strans *ice_st = pIceSession->pIceSTransport;
    if (ice_st == NULL) {
        PJ_LOG(1, (THIS_FILE, "Error: No ICE instance, create it first"));
        return false;
    }

    if (!pj_ice_strans_has_sess(ice_st)) {
        PJ_LOG(1, (THIS_FILE, "Error: No ICE session, initialize first"));
        return false;
    }

    status = pj_ice_strans_stop_ice(ice_st);
    if (status != PJ_SUCCESS) {
        pj_print_error("error stopping session", status);
        return false;
    } else {
        PJ_LOG(3, (THIS_FILE, "ICE session stopped"));
        return true;
    }
}

bool pj_ice_session_init(pj_ice_session_t *pIceSession, pj_ice_session_cfg_t *pCfg)
{
    if (g_bInited) {
        return true;
    } else {
        g_bInited = true;
    }

    pj_ice_strans_cfg *pIceCfg = &pIceSession->iceCfg;

    // Get STUN server or TURN server information from cloud server
    char *paddr = NULL;
    size_t addrlen = 0;
    uint16_t server_port = 0;
    cJSON *el_root_token = cJSON_Parse(pCfg->server_tokens);
    if (!cJSON_IsArray(el_root_token)) {
        return -1;
    }
    cJSON *el_one_token;
    cJSON_ArrayForEach(el_one_token, el_root_token)
    {
        if (!cJSON_IsObject(el_one_token)) {
            continue;
        }
        cJSON *el_username = cJSON_GetObjectItemCaseSensitive(el_one_token, "username");
        cJSON *el_credential = cJSON_GetObjectItemCaseSensitive(el_one_token, "credential");
        cJSON *el_urls = cJSON_GetObjectItemCaseSensitive(el_one_token, "urls");
        if (!cJSON_IsString(el_urls)) {
            continue;
        }
        char *p = el_urls->valuestring;
        char *ptransport = strstr(p, "?transport=");
        if (ptransport != NULL) {
            char *ptransport_type = ptransport + strlen("?transport=");
            if ((strncmp(ptransport_type, "tcp", strlen("tcp")) == 0) ||
                (strncmp(ptransport_type, "TCP", strlen("TCP")) == 0)) {
                continue;
            }
        }
        if (strncmp(p, "turn:", strlen("turn:")) == 0) {
            if ((!cJSON_IsString(el_username)) || (!cJSON_IsString(el_credential)))
                continue;
            pj_str_t username = pj_str(el_username->valuestring);
            pj_str_t credential = pj_str(el_credential->valuestring);
            if (pj_sdp_token_url_parse(p, "turn:", &paddr, &addrlen, &server_port) == 0) {
                pj_str_t pjstrServerHost;
                pjstrServerHost.ptr = paddr;
                pjstrServerHost.slen = addrlen;
                printf("+ turn server: %.*s port:%d\n", (int)pjstrServerHost.slen, pjstrServerHost.ptr, server_port);
                if (!is_ipv4(paddr) && !is_ipv6(paddr)) {
                    printf("- turn: %.*s is domain, ignore connect\n", (int)addrlen, paddr);
                    continue;
                }
                /*pIceCfg->turn.server = pj_str((char*)serverHost.base);
                pIceCfg->turn.port = server_port;*/
                pIceCfg->turn_tp_cnt = 1;
                pj_ice_strans_turn_cfg_default(&pIceCfg->turn_tp[0]);
                pj_strdup_with_null(pIceSession->pPool, &pIceCfg->turn_tp[0].server, &pjstrServerHost);
                pIceCfg->turn_tp[0].port = server_port;
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.username = pj_str(username.ptr);
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.data = pj_str(credential.ptr);
            }

        } else if (strncmp(p, "stun:", strlen("stun:")) == 0) {
            if (pj_sdp_token_url_parse(p, "stun:", &paddr, &addrlen, &server_port) == 0) {
                pj_str_t pjstrServerHost;
                pjstrServerHost.ptr = paddr;
                pjstrServerHost.slen = addrlen;
                printf("+ stun server: %.*s port:%d\n", (int)pjstrServerHost.slen, pjstrServerHost.ptr, server_port);
                if (!is_ipv4(paddr) && !is_ipv6(paddr)) {
                    printf("- stun: %.*s is domain, ignore connect\n", (int)pjstrServerHost.slen, pjstrServerHost.ptr);
                    continue;
                }
                pj_strdup_with_null(pIceSession->pPool, &pIceCfg->stun.server, &pjstrServerHost);
                pIceCfg->stun.port = server_port;
                pIceCfg->stun.cfg.ka_interval = KA_INTERVAL;
            }
        } else {
            continue;
        }
    } // cJSON_ArrayForEach(el_one_token, el_root_token)
    if (el_root_token != NULL) {
        cJSON_Delete(el_root_token);
        el_root_token = NULL;
    }

    /* init the callback */
    pj_ice_strans_cb icecb;
    pj_bzero(&icecb, sizeof(icecb));
    icecb.on_rx_data = pCfg->cb.ice_on_rx_data;
    icecb.on_ice_complete = pCfg->cb.ice_on_ice_complete;
    icecb.on_new_candidate = pCfg->cb.ice_on_new_candidate;
    /* create the instance */
    pj_status_t status = pj_ice_strans_create("icedemo", &pIceSession->iceCfg, pIceSession->uComponentCount,
                                              pCfg->user_data, &icecb, &pIceSession->pIceSTransport);
    if (status != PJ_SUCCESS) {
        return false;
    }

    unsigned rolechar = pCfg->rolechar;
    char *local_ufrag = pCfg->local_ufrag;
    char *local_passwd = pCfg->local_passwd;
    pj_ice_sess_role role =
        (pj_tolower((pj_uint8_t)rolechar) == 'o' ? PJ_ICE_SESS_ROLE_CONTROLLING : PJ_ICE_SESS_ROLE_CONTROLLED);
    pj_ice_strans *ice_st = pIceSession->pIceSTransport;
    if (ice_st == NULL) {
        PJ_LOG(1, (THIS_FILE, "Error: No ICE instance, create it first"));
        return false;
    }
    if (pj_ice_strans_has_sess(ice_st)) {
        PJ_LOG(1, (THIS_FILE, "Error: Session already created"));
        return false;
    }
    pj_str_t pjstrLocalUFrag = pj_str(local_ufrag);
    pj_str_t pjstrLocalPasswd = pj_str(local_passwd);
    status = pj_ice_strans_init_ice(ice_st, role, &pjstrLocalUFrag, &pjstrLocalPasswd);
    if (status != PJ_SUCCESS)
        pj_print_error("error creating session", status);
    else
        PJ_LOG(3, (THIS_FILE, "ICE session created"));
    return true;
}

bool pj_ice_session_add_remote_candidate(pj_ice_session_t *pIceSession, pj_str_t *rem_ufrag, pj_str_t *rem_passwd,
                                         unsigned rcand_cnt, pj_ice_sess_cand rcand[], pj_bool_t rcand_end)
{
    pj_status_t status = PJ_FALSE;
    pj_ice_strans *ice_st = pIceSession->pIceSTransport;
    if (ice_st == NULL) {
        return false;
    }
    /* Update the checklist */
    status = pj_ice_strans_update_check_list(ice_st, rem_ufrag, rem_passwd, rcand_cnt, rcand, rcand_end);
    if (status != PJ_SUCCESS)
        return false;
    /* Start ICE if both sides have sent their (initial) SDPs */
    if (!pj_ice_strans_sess_is_running(ice_st)) {
        unsigned i = 0, comp_cnt = 0;
        comp_cnt = pj_ice_strans_get_running_comp_cnt(ice_st);
        // for (i = 0; i < comp_cnt; ++i) {
        //     if (tp_ice->last_send_cand_cnt[i] > 0)
        //         break;
        // }
        if (i != comp_cnt) {
            pj_str_t rufrag;
            pj_ice_strans_get_ufrag_pwd(ice_st, NULL, NULL, &rufrag, NULL);
            if (rufrag.slen > 0) {
                PJ_LOG(3, (THIS_FILE, "Trickle ICE starts connectivity check"));
                status = pj_ice_strans_start_ice(ice_st, NULL, NULL, 0, NULL);
            }
        }
    }
    return true;
}

bool pj_ice_session_sendto(pj_ice_session_t *pIceSession, void *pkt, uint32_t len)
{
    pj_thread_register2();

    pj_status_t status = PJ_FALSE;
    pj_ice_strans *ice_st = pIceSession->pIceSTransport;
    char szLCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
    char szRCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
    unsigned comp_id = 1; // Component starts with ID 1
    const pj_ice_sess_check *pIceSessCheck = pj_ice_strans_get_valid_pair(ice_st, comp_id);
    pj_sockaddr_print(&pIceSessCheck->lcand->addr, szLCandAddr, sizeof(szLCandAddr), 3);
    pj_sockaddr_print(&pIceSessCheck->rcand->addr, szRCandAddr, sizeof(szRCandAddr), 3);
    status = pj_ice_strans_sendto2(ice_st, comp_id, pkt, len, &pIceSessCheck->rcand->addr,
                                   pj_sockaddr_get_len(&pIceSessCheck->rcand->addr));
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        return false;
    }
    return true;
}
