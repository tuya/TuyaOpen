#include "pj_ice.h"
#include "cJSON.h"
#include <pj/lock.h>
#include <pj/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    pj_bool_t bLocalGatherDone; /* set on PJ_ICE_STRANS_OP_INIT success */
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

/**
 * @brief Register current OS thread into pjlib
 * @return true on success or already registered, false on failure
 * @note pj_thread_desc must remain valid for the thread lifetime; allocate on heap.
 * @note Also initializes lwIP per-thread netconn semaphore (T5 requires
 *       LWIP_NETCONN_SEM_PER_THREAD).
 */
bool pj_thread_register2()
{
    pj_thread_desc *desc = NULL;
    pj_thread_t *thread = NULL;

    if (pj_thread_is_registered()) {
#if defined(PJ_HAS_LWIP_SOCKETS) && PJ_HAS_LWIP_SOCKETS != 0
        /* Ensure lwIP TLS sem exists even if pj was registered earlier. */
        lwip_socket_thread_init();
#endif
        return true;
    }
    desc = (pj_thread_desc *)calloc(1, sizeof(*desc));
    if (desc == NULL) {
        return false;
    }
    if (pj_thread_register(NULL, *desc, &thread) != PJ_SUCCESS) {
        free(desc);
        return false;
    }
#if defined(PJ_HAS_LWIP_SOCKETS) && PJ_HAS_LWIP_SOCKETS != 0
    lwip_socket_thread_init();
#endif
    /* Leak desc intentionally: pjlib keeps an internal pointer into it. */
    return true;
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

/**
 * @brief Check if buffer of given length is an IPv4 address
 * @param[in] ip_str address bytes (need not be NUL-terminated)
 * @param[in] len address length
 * @return true if IPv4
 */
static bool is_ipv4_n(const char *ip_str, size_t len)
{
    char tmp[PJ_INET6_ADDRSTRLEN + 8];
    if (ip_str == NULL || len == 0 || len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, ip_str, len);
    tmp[len] = '\0';
    return is_ipv4(tmp);
}

/**
 * @brief Check if buffer of given length is an IPv6 address
 * @param[in] ip_str address bytes (need not be NUL-terminated)
 * @param[in] len address length
 * @return true if IPv6
 */
static bool is_ipv6_n(const char *ip_str, size_t len)
{
    char tmp[PJ_INET6_ADDRSTRLEN + 8];
    if (ip_str == NULL || len == 0 || len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, ip_str, len);
    tmp[len] = '\0';
    return is_ipv6(tmp);
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
    {
        pj_timer_heap_t *heap = pIceStransCfg->stun_cfg.timer_heap;
        pj_size_t pending = heap ? pj_timer_heap_count(heap) : 0;

        c = pj_timer_heap_poll(heap, &timeout);
        if (c > 0) {
            static unsigned s_timer_fire_log;
            count += c;
            s_timer_fire_log++;
            if (s_timer_fire_log <= 10 || (s_timer_fire_log % 100) == 0) {
                PJ_LOG(5, ("pj_ice", "ice timer fired n=%u count=%d", s_timer_fire_log, count));
            }
        } else if (pending > 0) {
            static unsigned s_timer_stall_log;
            s_timer_stall_log++;
            if (s_timer_stall_log <= 8 || (s_timer_stall_log % 50) == 0) {
                PJ_LOG(4, ("pj_ice", "ice timer pending but not due n=%u pending=%d", s_timer_stall_log, pending));
            }
        }
    }

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
                PJ_LOG(1, ("pj_ice", "pj_handle_events error: %d", err));
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
    char baseaddr[PJ_INET6_ADDRSTRLEN];
    char *p = buffer;
    int printed;
    pj_uint32_t prio = cand->prio;

    /*
     * ice_strans cand_list entries often still have prio==0 when trickled via
     * on_new_candidate (prio is only stored on ice_sess lcand). Compute RFC5245
     * priority so the peer does not discard the candidate.
     */
    if (prio == 0 && cand->type <= PJ_ICE_CAND_TYPE_RELAYED) {
        static const pj_uint32_t type_prefs[] = {
            126, /* HOST */
            100, /* SRFLX */
            110, /* PRFLX */
            0,   /* RELAYED */
        };
        pj_uint32_t local_pref = cand->local_pref ? cand->local_pref : 65535;
        prio = ((type_prefs[cand->type] & 0xFF) << 24) + ((local_pref & 0xFFFF) << 8) +
               (((256 - cand->comp_id) & 0xFF) << 0);
    }

    PRINT("a=candidate:%.*s %u UDP %u %s %u typ ", (int)cand->foundation.slen, cand->foundation.ptr,
          (unsigned)cand->comp_id, prio, pj_sockaddr_print(&cand->addr, ipaddr, sizeof(ipaddr), 0),
          (unsigned)pj_sockaddr_get_port(&cand->addr));

    PRINT("%s", pj_ice_get_cand_type_name(cand->type));

    if (cand->type == PJ_ICE_CAND_TYPE_SRFLX || cand->type == PJ_ICE_CAND_TYPE_RELAYED) {
        if (pj_sockaddr_has_addr(&cand->base_addr)) {
            PRINT(" raddr %s rport %u", pj_sockaddr_print(&cand->base_addr, baseaddr, sizeof(baseaddr), 0),
                  (unsigned)pj_sockaddr_get_port(&cand->base_addr));
        }
    }

    PRINT("\r\n");

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
        goto on_return;
    }

    /* Port */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + host.slen);
    if (found_idx == orig_input->slen) {
        goto on_return;
    }
    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)pj_strtoul(&token));

    /* typ */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        goto on_return;
    }
    if (pj_stricmp2(&token, "typ") != 0) {
        goto on_return;
    }

    /* candidate type */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
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
        goto on_return;
    }

    return 0;

on_return:
    return -1;
}

int pj_sdp_token_url_parse(const char *token_url, const char *type, char **addr, size_t *addr_len, uint16_t *port)
{
    if (token_url == NULL || type == NULL || addr == NULL || addr_len == NULL || port == NULL) {
        PJ_LOG(1, ("pj_ice", "invalid param"));
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
        PJ_LOG(1, ("pj_ice", "invalid token url"));
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

/* Turns of the event loop given to deferred socket teardown before the
 * ioqueue is destroyed. */
#define ICE_DESTROY_DRAIN_ROUNDS 10u
#define ICE_DESTROY_DRAIN_MS     10u

bool pj_ice_session_create(pj_ice_session_cfg_t *pCfg, pj_ice_session_t **ppIceSession)
{
    pj_init();
    pjlib_util_init();
    pjnath_init();
    /* Keep PJ log quiet on RTOS: level 4 + ice callbacks overflow rtc_worker stack */
    pj_log_set_level(0);

    pj_ice_session_t *pIceSession = malloc(sizeof(pj_ice_session_t));
    pIceSession->pPool = NULL;
    pIceSession->pThread = NULL;
    pIceSession->bThreadQuitFlag = false;
    pIceSession->pIceSTransport = NULL;
    pIceSession->bLastCand = false;
    pIceSession->bLocalGatherDone = PJ_FALSE;
    pIceSession->uComponentCount = 1;
    pj_caching_pool_init(&pIceSession->cachePool, NULL, 0);
    pj_ice_strans_cfg_default(&pIceSession->iceCfg);
    pIceSession->iceCfg.stun_cfg.pf = &pIceSession->cachePool.factory;
    pIceSession->pPool = pj_pool_create(&pIceSession->cachePool.factory, "ice_Pool", 512, 512, NULL);
    pj_timer_heap_create(pIceSession->pPool, 100, &pIceSession->iceCfg.stun_cfg.timer_heap);
    /* Signaling thread schedules ICE timers; rtc_wrk polls them — must lock. */
    {
        pj_lock_t *timer_lock = NULL;
        pj_status_t lock_st =
            pj_lock_create_recursive_mutex(pIceSession->pPool, "ice_tmr", &timer_lock);
        if (lock_st == PJ_SUCCESS && timer_lock != NULL) {
            pj_timer_heap_set_lock(pIceSession->iceCfg.stun_cfg.timer_heap, timer_lock, PJ_TRUE);
        } else {
            PJ_LOG(3, ("pj_ice", "ice timer lock create failed st=%d", lock_st));
        }
    }
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
    pj_ice_strans *ice_st = NULL;
    bool ok = true;
    unsigned i;

    if (pIceSession == NULL) {
        return false;
    }

    g_bInited = false;

    pj_thread_register2();

    /* The worker polls the ioqueue and timer heap torn down below. */
    if (pIceSession->pIceThreadParam != NULL) {
        pIceSession->pIceThreadParam->bThreadQuitFlag = true;
    }
    if (pIceSession->pThread != NULL) {
        pj_thread_join(pIceSession->pThread);
        pj_thread_destroy(pIceSession->pThread);
        pIceSession->pThread = NULL;
    }

    ice_st = pIceSession->pIceSTransport;
    pIceSession->pIceSTransport = NULL;
    if (ice_st != NULL) {
        if (pj_ice_strans_has_sess(ice_st)) {
            pj_status_t status = pj_ice_strans_stop_ice(ice_st);
            if (status != PJ_SUCCESS) {
                pj_print_error("error stopping session", status);
                ok = false;
            }
        }
        /* stop_ice only ends the negotiation - pjnath deliberately keeps the
         * sockets open so the transport can be reused. Nothing here reuses it,
         * so without this every session leaks its UDP sockets and its pool. */
        pj_ice_strans_destroy(ice_st);
    }

    /* Socket close is reference counted and completes in a group lock handler,
     * so give the ioqueue and timer a few turns before taking them away. */
    for (i = 0; i < ICE_DESTROY_DRAIN_ROUNDS; i++) {
        unsigned handled = 0;
        pj_ice_session_handle_events(pIceSession, ICE_DESTROY_DRAIN_MS, &handled);
    }

    if (pIceSession->iceCfg.stun_cfg.ioqueue != NULL) {
        pj_ioqueue_destroy(pIceSession->iceCfg.stun_cfg.ioqueue);
        pIceSession->iceCfg.stun_cfg.ioqueue = NULL;
    }
    if (pIceSession->iceCfg.stun_cfg.timer_heap != NULL) {
        pj_timer_heap_destroy(pIceSession->iceCfg.stun_cfg.timer_heap);
        pIceSession->iceCfg.stun_cfg.timer_heap = NULL;
    }
    if (pIceSession->pPool != NULL) {
        pj_pool_release(pIceSession->pPool);
        pIceSession->pPool = NULL;
    }
    pj_caching_pool_destroy(&pIceSession->cachePool);

    if (pIceSession->pIceThreadParam != NULL) {
        free(pIceSession->pIceThreadParam);
        pIceSession->pIceThreadParam = NULL;
    }
    free(pIceSession);

    PJ_LOG(3, (THIS_FILE, "ICE session destroyed"));
    return ok;
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
            if (pj_sdp_token_url_parse(p, "turn:", &paddr, &addrlen, &server_port) == 0) {
                pj_str_t pjstrServerHost;
                pj_str_t pjstrUser;
                pj_str_t pjstrCred;
                pjstrServerHost.ptr = paddr;
                pjstrServerHost.slen = addrlen;
                PJ_LOG(4, ("pj_ice", "+ turn server: %.*s port:%d", (int)pjstrServerHost.slen, pjstrServerHost.ptr,
                           server_port));
                if (!is_ipv4_n(paddr, addrlen) && !is_ipv6_n(paddr, addrlen)) {
                    PJ_LOG(2, ("pj_ice", "- turn: %.*s is domain, ignore connect", (int)addrlen, paddr));
                    continue;
                }
                pIceCfg->turn_tp_cnt = 1;
                pj_ice_strans_turn_cfg_default(&pIceCfg->turn_tp[0]);
                pj_strdup_with_null(pIceSession->pPool, &pIceCfg->turn_tp[0].server, &pjstrServerHost);
                pIceCfg->turn_tp[0].port = server_port;
                /*
                 * Must copy username/credential into pool: valuestring is owned
                 * by cJSON and freed by cJSON_Delete below. Dangling TURN auth
                 * caused intermittent missing local relay (cross-subnet flaky).
                 */
                pjstrUser = pj_str(el_username->valuestring);
                pjstrCred = pj_str(el_credential->valuestring);
                pIceCfg->turn_tp[0].auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
                pj_strdup(pIceSession->pPool, &pIceCfg->turn_tp[0].auth_cred.data.static_cred.username, &pjstrUser);
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
                pj_strdup(pIceSession->pPool, &pIceCfg->turn_tp[0].auth_cred.data.static_cred.data, &pjstrCred);
                /* Empty realm/nonce: short-term first; TURN 401 will refresh long-term */
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.realm = pj_str("");
                pIceCfg->turn_tp[0].auth_cred.data.static_cred.nonce = pj_str("");
            }

        } else if (strncmp(p, "stun:", strlen("stun:")) == 0) {
            if (pj_sdp_token_url_parse(p, "stun:", &paddr, &addrlen, &server_port) == 0) {
                pj_str_t pjstrServerHost;
                pjstrServerHost.ptr = paddr;
                pjstrServerHost.slen = addrlen;
                PJ_LOG(4, ("pj_ice", "+ stun server: %.*s port:%d", (int)pjstrServerHost.slen, pjstrServerHost.ptr,
                           server_port));
                if (!is_ipv4_n(paddr, addrlen) && !is_ipv6_n(paddr, addrlen)) {
                    PJ_LOG(2, ("pj_ice", "- stun: %.*s is domain, ignore connect", (int)pjstrServerHost.slen,
                               pjstrServerHost.ptr));
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
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_ice_strans *ice_st = NULL;
    char addrbuf[PJ_INET6_ADDRSTRLEN + 10];

    if (pIceSession == NULL) {
        return false;
    }
    ice_st = pIceSession->pIceSTransport;
    if (ice_st == NULL) {
        return false;
    }


    /* Update the checklist */
    status = pj_ice_strans_update_check_list(ice_st, rem_ufrag, rem_passwd, rcand_cnt, rcand, rcand_end);
    if (status != PJ_SUCCESS) {
        pj_strerror(status, errmsg, sizeof(errmsg));
        pj_ice_session_dbg_dump(pIceSession, "update_fail");
        return false;
    }
    /*
     * Checklist can be updated while local gathering is still running.
     * Only call start_ice after local INIT OK (bLocalGatherDone) and remote ufrag.
     */
    return pj_ice_session_try_start_ice(pIceSession, "add_remote");
}

/**
 * @brief Try start_ice when local gather done and remote ufrag ready
 * @param[in] pIceSession ICE session
 * @param[in] tag log tag
 * @return true on success or deferred / already running
 */
bool pj_ice_session_try_start_ice(pj_ice_session_t *pIceSession, const char *tag)
{
    pj_status_t status;
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_ice_strans *ice_st;
    pj_str_t rufrag;

    if (pIceSession == NULL || pIceSession->pIceSTransport == NULL) {
        return false;
    }
    ice_st = pIceSession->pIceSTransport;

    if (pj_ice_strans_sess_is_running(ice_st) || pj_ice_strans_sess_is_complete(ice_st)) {
        return true;
    }

    if (!pIceSession->bLocalGatherDone) {
        return true;
    }

    pj_ice_strans_get_ufrag_pwd(ice_st, NULL, NULL, &rufrag, NULL);
    if (rufrag.slen <= 0) {
        return true;
    }

    status = pj_ice_strans_start_ice(ice_st, NULL, NULL, 0, NULL);
    pj_strerror(status, errmsg, sizeof(errmsg));
    if (status != PJ_SUCCESS) {
        pj_ice_session_dbg_dump(pIceSession, "start_ice_fail");
        return false;
    }
    pj_ice_session_dbg_dump(pIceSession, "start_ice_ok");
    return true;
}

/**
 * @brief Mark local candidate gathering complete and try start_ice
 * @param[in] pIceSession ICE session
 * @return true if start_ice ran successfully or already running / waiting remote
 */
bool pj_ice_session_on_local_gather_done(pj_ice_session_t *pIceSession)
{
    if (pIceSession == NULL) {
        return false;
    }
    pIceSession->bLocalGatherDone = PJ_TRUE;
    PJ_LOG(4, ("pj_ice", "ICE local gather done"));
    return pj_ice_session_try_start_ice(pIceSession, "gather_done");
}

/**
 * @brief Dump ICE transport state / candidate counts for debug
 * @param[in] pIceSession ICE session
 * @param[in] tag log tag
 * @return none
 */
void pj_ice_session_dbg_dump(pj_ice_session_t *pIceSession, const char *tag)
{
    (void)pIceSession;
    (void)tag;
}

/**
 * @brief Whether ICE negotiation finished (success or failed terminal)
 * @param[in] pIceSession ICE session
 * @return true if RUNNING or FAILED or complete flag set
 */
bool pj_ice_session_is_nego_done(pj_ice_session_t *pIceSession)
{
    pj_ice_strans *ice_st;
    pj_ice_strans_state st;

    if (pIceSession == NULL || pIceSession->pIceSTransport == NULL) {
        return false;
    }
    ice_st = pIceSession->pIceSTransport;
    if (pj_ice_strans_sess_is_complete(ice_st)) {
        return true;
    }
    st = pj_ice_strans_get_state(ice_st);
    return (st == PJ_ICE_STRANS_STATE_RUNNING || st == PJ_ICE_STRANS_STATE_FAILED);
}

/**
 * @brief Whether ICE negotiation succeeded (media path ready)
 * @param[in] pIceSession ICE session
 * @return true only when transport is RUNNING
 */
bool pj_ice_session_is_nego_success(pj_ice_session_t *pIceSession)
{
    pj_ice_strans *ice_st;

    if (pIceSession == NULL || pIceSession->pIceSTransport == NULL) {
        return false;
    }
    ice_st = pIceSession->pIceSTransport;
    return (pj_ice_strans_get_state(ice_st) == PJ_ICE_STRANS_STATE_RUNNING);
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
