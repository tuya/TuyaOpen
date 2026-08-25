/**
 * @file netmgr_cli.c
 * @brief The `netmgr` console command, split out of netmgr.c.
 *
 * netmgr.c is the state machine and owns `s_netmgr`, which stays static. This
 * file is the CLI front end and reaches netmgr only through the snapshot
 * accessors in netmgr_priv.h: netmgr_state_get() and netmgr_link_info_at() each
 * take s_netmgr.lock, copy out and release it, so every PR_* call below runs with
 * no lock held. That is the whole reason those accessors exist - the version of
 * this command that lived in netmgr.c formatted its dump inside
 * tal_mutex_lock()/tal_mutex_unlock().
 *
 * Two other things changed in the move.
 *
 * The dump is generic. It walks netmgr_link_info_at() instead of hand-coding one
 * block per technology, so cellular - which the old dump never printed, because
 * it only knew about wifi and wired - shows up on its own, and a new link type
 * needs no edit here. All `#ifdef ENABLE_<TECH>` in the dump path are gone with
 * it. One remains, around the `netmgr wifi up` handler, because that subcommand
 * needs netconn_wifi_info_t: syntax that is specific to one technology belongs in
 * code that is specific to one technology.
 *
 * Every argv[] read is now guarded. See __netmgr_cli_wifi() for why that was not
 * a cosmetic fix: tal_cli's argv is a persistent array that the tokenizer only
 * fills up to argc, so reading argv[argc] reads whatever the previous command
 * left there.
 *
 * M3 added three things, and all three are about the policy layer being
 * observable and drivable from a serial console rather than only from code.
 *
 * The dump prints the INTERNAL link state next to the public one. They are two
 * different questions - conn->status is what the driver reports, two-valued by
 * contract; netmgr_link_state_e is what netmgr's own machine believes - and the
 * case that matters in the field is exactly the one where they disagree: a link
 * at `link_up / degraded` has an address and carries LAN traffic while netmgr has
 * evidence that nothing reaches the cloud through it. No public status can say
 * that, which is why netmgr_policy.h exposes netmgr_link_state_get() separately.
 * The probe accumulator, the policy in force and the pin are printed with it, so
 * a demotion can be traced to the verdicts that caused it without a debugger.
 *
 * `netmgr switch <name>` drives netmgr_policy_pin(). Names come from
 * netconn_desc_t.name, so an operator types "wifi" rather than a bit value, and
 * this file still names no technology outside the one wifi-specific subcommand.
 *
 * `netmgr deinit` / `netmgr init` are the first callers netmgr_deinit() has ever
 * had; see __netmgr_cli_deinit() for why they are safe from this thread and for
 * what they are for.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr.h"
#include "netmgr_priv.h"
#include "netmgr_policy.h"
#include "netmgr_probe.h"
#include "netconn_registry.h"
#include "tal_api.h"

#ifdef ENABLE_WIFI
#include "netconn_wifi.h"
#endif

/***********************************************************
************************ constants *************************
***********************************************************/

/** Enough for every NETCONN_CAP_* name below plus the trailing unknown-bits. */
#define NETMGR_CLI_CAPS_STR_LEN 96

/** Upper bound on scan results printed, so one `wifi scan` cannot flood the log. */
#define NETMGR_CLI_SCAN_MAX 32

/** Enough for NETMGR_RETRY_TABLE_MAX intervals rendered as "30/60/120/300/600". */
#define NETMGR_CLI_REVAL_STR_LEN 80

/** Enough for every registered link name joined by '|', for the switch usage. */
#define NETMGR_CLI_NAMES_STR_LEN 64

/***********************************************************
*********************** capabilities ***********************
***********************************************************/

typedef struct {
    netconn_caps_t bit;
    const char    *name;
} netmgr_cli_cap_name_t;

static const netmgr_cli_cap_name_t s_cap_names[] = {
    {NETCONN_CAP_LAN, "lan"},
    {NETCONN_CAP_NETCFG_AP, "netcfg-ap"},
    {NETCONN_CAP_NETCFG_BLE, "netcfg-ble"},
    {NETCONN_CAP_METERED, "metered"},
};

/**
 * @brief Render a capability mask as a comma-separated name list.
 *
 * Bits with no name here are not dropped, they are appended as a hex remainder,
 * so a capability added to netconn_registry.h without touching this file is
 * visible in the dump instead of silently invisible.
 *
 * @param[in]  caps the mask to render
 * @param[out] buf  destination, always NUL-terminated
 * @param[in]  len  size of @a buf in bytes, must be non-zero
 */
static void __netmgr_cli_caps_str(netconn_caps_t caps, char *buf, uint32_t len)
{
    uint32_t       used = 0;
    netconn_caps_t rest = caps;
    uint32_t       i;

    buf[0] = '\0';

    for (i = 0; i < CNTSOF(s_cap_names); i++) {
        if (!(caps & s_cap_names[i].bit)) {
            continue;
        }
        rest &= ~s_cap_names[i].bit;
        used += (uint32_t)snprintf(buf + used, len - used, "%s%s", used ? "," : "", s_cap_names[i].name);
        if (used >= len - 1) {
            /* Truncated. Stop before snprintf() is handed a zero-size buffer. */
            return;
        }
    }

    if (rest) {
        snprintf(buf + used, len - used, "%s0x%x", used ? "," : "", (unsigned int)rest);
    } else if (0 == used) {
        snprintf(buf, len, "none");
    }
}

/***********************************************************
************************* policy ***************************
***********************************************************/

/**
 * @brief The effective probe_bad_threshold: netmgr_policy.h says 0 means 1.
 *
 * Resolved here rather than printed raw so the dump agrees with the machine. A
 * row reading "bad 1 of 0" would be arithmetic nonsense on a policy that behaves
 * perfectly well.
 *
 * @param[in] policy the policy in force
 *
 * @return the threshold a BAD count is actually compared against
 */
/**
 * @brief Name a link type the way the rest of the module does: from the registry.
 *
 * NETMGR_TYPE_TO_STR() enumerates the three in-tree technologies and answers
 * "unknown" for anything else, which defeats the point of a registry - a board
 * that adds a link type through netconn_registry_set_table() would see its link
 * printed as "unknown" by every line below. Worst of it was `netmgr switch`,
 * which resolves the argument BY NAME out of the registry and then reported the
 * result through the macro.
 *
 * The macro is still the fallback and has to be, because two callers below name a
 * type that is deliberately NOT in the registry: an unregistered pin, and the
 * teardown race in `netmgr switch`. There is no descriptor to read a name from in
 * either case, which is also why netmgr_policy.c keeps using the macro directly -
 * its one call site is the arm reached only when the lookup returned NULL.
 */
static const char *__netmgr_cli_type_name(netmgr_type_e type)
{
    const netconn_desc_t *desc = netconn_registry_find(type);

    return (NULL != desc && NULL != desc->name) ? desc->name : NETMGR_TYPE_TO_STR(type);
}

static uint8_t __netmgr_cli_bad_thr(const netmgr_policy_t *policy)
{
    return (0 == policy->probe_bad_threshold) ? 1 : policy->probe_bad_threshold;
}

/**
 * @brief Render netmgr_policy_t.revalidate for one log line.
 *
 * The sentinel is resolved the way netmgr_policy.h says it must be, at the point
 * of use, because the two readings differ in a way an operator has to be able to
 * see: a NULL entry means "unset" and netmgr substitutes
 * netmgr_retry_table_revalidate for it, while a non-NULL entry with count 0 means
 * "never re-verify". Printing "0 entries" for both would hide the one distinction
 * this field exists to express.
 *
 * @param[in]  table the policy's revalidation table
 * @param[out] buf   destination, always NUL-terminated
 * @param[in]  len   size of @a buf in bytes, must be greater than 1
 */
static void __netmgr_cli_reval_str(const netmgr_retry_table_t *table, char *buf, uint32_t len)
{
    uint32_t used = 0;
    uint32_t i;

    if (NULL == table->entry) {
        snprintf(buf, len, "default, 30/60/120/300/600 s");
        return;
    }
    if (0 == table->count) {
        snprintf(buf, len, "off, a demoted link is never re-verified");
        return;
    }

    buf[0] = '\0';
    for (i = 0; i < table->count; i++) {
        used += (uint32_t)snprintf(buf + used, len - used, "%s%u", used ? "/" : "", (unsigned int)table->entry[i]);
        if (used >= len - 1) {
            /* Truncated. Stop before snprintf() is handed a zero-size buffer. */
            return;
        }
    }

    snprintf(buf + used, len - used, " s");
}

/**
 * @brief Print the policy in force and the manual pin.
 *
 * netmgr_policy_get() and netmgr_policy_pin_get() are lock-free by construction -
 * netmgr_policy.c argues why - so unlike everything else in this file they are not
 * snapshot accessors in the mutex sense. The result is the same: nothing here runs
 * with s_netmgr.lock held.
 *
 * @param[in] policy the policy in force, or NULL when it could not be read
 */
static void __netmgr_cli_dump_policy(const netmgr_policy_t *policy)
{
    netmgr_type_e pin = NETCONN_AUTO;
    char          reval[NETMGR_CLI_REVAL_STR_LEN];

    if (NULL == policy) {
        PR_NOTICE("policy: unavailable");
        return;
    }

    /* Cannot fail with a non-NULL argument; the pin is left at NETCONN_AUTO,
     * i.e. "none", if it somehow does. */
    (void)netmgr_policy_pin_get(&pin);

    __netmgr_cli_reval_str(&policy->revalidate, reval, sizeof(reval));

    PR_NOTICE("policy: pin %s, preempt %s, up_switch %s", (NETCONN_AUTO == pin) ? "none" : __netmgr_cli_type_name(pin),
              policy->preempt ? "on" : "off", policy->emit_up_switch ? "on" : "off");
    PR_NOTICE("policy: debounce %u ms, grace %u ms, dwell %u ms", (unsigned int)policy->up_debounce_ms,
              (unsigned int)policy->down_grace_ms, (unsigned int)policy->min_dwell_ms);
    PR_NOTICE("policy: probe %s, demote %s, reconn %s, bad_thr %u, verify %u ms", policy->probe_enable ? "on" : "off",
              policy->probe_demote ? "on" : "off", policy->probe_reconnect ? "on" : "off",
              (unsigned int)__netmgr_cli_bad_thr(policy), (unsigned int)policy->verify_timeout_ms);
    PR_NOTICE("policy: revalidate %s", reval);
}

/***********************************************************
*************************** dump ***************************
***********************************************************/

/**
 * @brief Print one link's row, plus its probe annotation when it has one.
 *
 * Three snapshots, then the printing. netmgr_link_info_at(),
 * netmgr_link_state_get() and netmgr_probe_stat_get() each take s_netmgr.lock,
 * copy out and release it, so no PR_* below runs under it - the same contract for
 * all three, stated in netmgr_priv.h, netmgr_policy.h and netmgr_probe.h
 * respectively.
 *
 * @param[in] index   position, 0 .. netmgr_state_t.link_num - 1
 * @param[in] active  the link traffic currently leaves through
 * @param[in] bad_thr the effective probe_bad_threshold, for the "n of m" count
 *
 * @return TRUE when a row was printed, FALSE when the link is gone
 */
static BOOL_T __netmgr_cli_dump_link(uint32_t index, netmgr_type_e active, uint8_t bad_thr)
{
    netmgr_link_info_t  info;
    netmgr_link_state_e lstate = NETMGR_LINK_STATE_DOWN;
    netmgr_probe_stat_t stat   = {0};
    char                caps[NETMGR_CLI_CAPS_STR_LEN];
    BOOL_T              have_state = FALSE;
    BOOL_T              have_stat  = FALSE;

    /* A link cannot appear or disappear between netmgr_init() and
     * netmgr_deinit(), so the only way this fails is a teardown racing the dump.
     * The caller stops rather than print a half-torn-down table. */
    if (OPRT_OK != netmgr_link_info_at(index, &info)) {
        return FALSE;
    }

    have_state = (OPRT_OK == netmgr_link_state_get(info.type, &lstate));
    have_stat  = (OPRT_OK == netmgr_probe_stat_get(info.type, &stat));

    __netmgr_cli_caps_str(info.caps, caps, sizeof(caps));

    /* Both status columns are here because they legitimately disagree, and the
     * disagreement is the whole point of the M3 dump. `status` is the driver's
     * two-valued view from conn->status; `state` is netmgr's own machine. Read
     * "link_up degraded" as: the link has an address and carries LAN traffic, and
     * netmgr has evidence that nothing gets to the cloud through it. Read
     * "link_up backoff" or "link_up unverified" as: the driver has reported but
     * netmgr has not acted on it yet, or is holding it out on debounce. */
    PR_NOTICE("  %s%-2u %-9s %3u %-9s %-10s %-9s %4u %s", (info.type == active) ? "*" : " ", (unsigned int)index,
              info.name, (unsigned int)info.pri, NETMGR_STATUS_TO_STR(info.status),
              have_state ? NETMGR_LINK_STATE_TO_STR(lstate) : "?", NETCONN_CTRL_TO_STR(info.ctrl),
              (unsigned int)info.provider, caps);

    /* Only when there is something to say. With probing off, or for a link that
     * has never been active, the accumulator is all zeroes; a row of zeroes per
     * link would bury the one link that does have a verdict. */
    if (have_stat && (NETMGR_PROBE_UNKNOWN != stat.last || 0 != stat.good_total || 0 != stat.bad_total)) {
        PR_NOTICE("      probe last %s from %s, bad %u of %u, total %u good %u bad",
                  NETMGR_PROBE_VERDICT_TO_STR(stat.last), NETMGR_PROBE_SRC_TO_STR(stat.source),
                  (unsigned int)stat.bad_count, (unsigned int)bad_thr, (unsigned int)stat.good_total,
                  (unsigned int)stat.bad_total);
    }

    return TRUE;
}

/**
 * @brief Print the global state, the policy, and one row per registered link.
 *
 * @param[in] state a snapshot already taken by the caller
 */
static void __netmgr_cli_dump(const netmgr_state_t *state)
{
    netmgr_policy_t policy   = {0};
    BOOL_T          have_pol = FALSE;
    uint8_t         bad_thr  = 1;
    uint32_t        i;

    PR_NOTICE("netmgr: configured 0x%02x, active %s, status %s, links %u", (unsigned int)state->configured,
              __netmgr_cli_type_name(state->active), NETMGR_STATUS_TO_STR(state->status),
              (unsigned int)state->link_num);

    /* Read once, for the policy lines and for every row's threshold, so the dump
     * cannot show two different thresholds if netmgr_policy_set() lands mid-dump. */
    have_pol = (OPRT_OK == netmgr_policy_get(&policy));
    if (have_pol) {
        bad_thr = __netmgr_cli_bad_thr(&policy);
    }
    __netmgr_cli_dump_policy(have_pol ? &policy : NULL);

    PR_NOTICE("  idx name      pri status    state      ctrl      prov caps");
    PR_NOTICE("  ----------------------------------------------------------");

    for (i = 0; i < state->link_num; i++) {
        if (!__netmgr_cli_dump_link(i, state->active, bad_thr)) {
            PR_NOTICE("  (link %u vanished, netmgr is being torn down)", (unsigned int)i);
            break;
        }
    }
}

/***********************************************************
************************** usage ***************************
***********************************************************/

static void __netmgr_cli_usage(void)
{
    PR_INFO("usage:");
    PR_INFO("  netmgr                            dump links, policy and probe stats");
    PR_INFO("  netmgr wifi up <ssid> [password]  join an AP, no password means open");
    PR_INFO("  netmgr wifi down                  leave the current AP");
    PR_INFO("  netmgr wifi scan                  list nearby APs");
    PR_INFO("  netmgr wired [up|down]            not supported, wired is observe-only");
    PR_INFO("  netmgr switch <name|auto>         pin the active link, auto releases");
    PR_INFO("  netmgr deinit                     tear netmgr down");
    PR_INFO("  netmgr init                       bring it back up");
}

/***********************************************************
************************** wifi ****************************
***********************************************************/

#ifdef ENABLE_WIFI

static void __netmgr_cli_wifi_usage(void)
{
    PR_INFO("usage: netmgr wifi [up <ssid> [password] | down | scan]");
}

/**
 * @brief `netmgr wifi up <ssid> [password]`.
 *
 * @param[in] argc argument count, at least 3
 * @param[in] argv argument vector
 */
static void __netmgr_cli_wifi_up(int argc, char *argv[])
{
    netconn_wifi_info_t wifi_info = {0};
    const char         *pswd      = "";

    /* argv[3] is the ssid and is mandatory. */
    if (argc < 4) {
        PR_INFO("usage: netmgr wifi up <ssid> [password]");
        return;
    }

    /* The tokenizer splits on spaces and does not honour quotes, so a password
     * with a space in it arrives as several arguments. Refusing is better than
     * silently joining with only the first word of it. */
    if (argc > 5) {
        PR_INFO("too many arguments: ssid and password must not contain spaces");
        return;
    }

    /* argc is 4 or 5 here. argv[4] exists only in the second case; reading it
     * unconditionally is the bug this replaces. argc == 4 now means an open
     * network, which the old code could not express at all. */
    if (argc >= 5) {
        pswd = argv[4];
    }

    if (strlen(argv[3]) > WIFI_SSID_LEN || strlen(pswd) > WIFI_PASSWD_LEN) {
        PR_INFO("ssid or password too long (max %d / %d)", WIFI_SSID_LEN, WIFI_PASSWD_LEN);
        return;
    }

    strncpy(wifi_info.ssid, argv[3], sizeof(wifi_info.ssid) - 1);
    wifi_info.ssid[sizeof(wifi_info.ssid) - 1] = '\0';
    strncpy(wifi_info.pswd, pswd, sizeof(wifi_info.pswd) - 1);
    wifi_info.pswd[sizeof(wifi_info.pswd) - 1] = '\0';

    PR_NOTICE("wifi up: ssid \"%s\", %s", wifi_info.ssid, (0 == wifi_info.pswd[0]) ? "open" : "with password");
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
}

static void __netmgr_cli_wifi_scan(void)
{
    AP_IF_S    *ap_list = NULL;
    uint32_t    ap_num  = 0;
    uint32_t    i;
    OPERATE_RET rt;

    rt = tal_wifi_all_ap_scan(&ap_list, &ap_num);
    if (OPRT_OK != rt || NULL == ap_list) {
        PR_INFO("wifi scan failed, rt %d", rt);
        return;
    }

    PR_NOTICE("wifi scan: %u ap(s)", (unsigned int)ap_num);
    if (ap_num > NETMGR_CLI_SCAN_MAX) {
        PR_NOTICE("  showing the first %d", NETMGR_CLI_SCAN_MAX);
        ap_num = NETMGR_CLI_SCAN_MAX;
    }
    for (i = 0; i < ap_num; i++) {
        PR_NOTICE("  [%2u] %-32s ch %2u rssi %4d sec %u", (unsigned int)i, (char *)ap_list[i].ssid,
                  (unsigned int)ap_list[i].channel, (int)ap_list[i].rssi, (unsigned int)ap_list[i].security);
    }

    /* The version of this command in netmgr.c never released the list. */
    tal_wifi_release_ap(ap_list);
}

/**
 * @brief `netmgr wifi ...`.
 *
 * @param[in] argc argument count, at least 2
 * @param[in] argv argument vector
 */
static void __netmgr_cli_wifi(int argc, char *argv[])
{
    netmgr_link_info_t info;

    if (OPRT_OK != netmgr_link_info_get(NETCONN_WIFI, &info)) {
        PR_INFO("wifi is compiled in but not registered with netmgr");
        return;
    }

    /* argv[2] is the subcommand. The old code read it whenever argc != 1, so
     * a bare `netmgr wifi` (argc == 2) reached strcmp(argv[2], "up"). */
    if (argc < 3) {
        __netmgr_cli_wifi_usage();
        return;
    }

    if (0 == strcmp(argv[2], "up")) {
        __netmgr_cli_wifi_up(argc, argv);
    } else if (0 == strcmp(argv[2], "down")) {
        netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);
    } else if (0 == strcmp(argv[2], "scan")) {
        __netmgr_cli_wifi_scan();
    } else {
        __netmgr_cli_wifi_usage();
    }
}

#else /* ENABLE_WIFI */

static void __netmgr_cli_wifi(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    PR_INFO("wifi is not enabled in this build");
}

#endif /* ENABLE_WIFI */

/***********************************************************
************************** wired ***************************
***********************************************************/

/**
 * @brief `netmgr wired ...`.
 *
 * There is nothing to drive. tal_wired.h exposes get_status, set_status_cb and
 * the ip/mac pairs and no connect or disconnect, which is what the wired
 * descriptor records as NETCONN_CTRL_OBSERVE. The handler this replaces had `up`
 * and `down` arms whose bodies were `// TBD`, so both reported success and did
 * nothing; say so instead.
 *
 * No `#ifdef ENABLE_WIRED` here on purpose: whether the link exists is a
 * question for the registry, and netmgr_link_info_get() answers it.
 *
 * @param[in] argc argument count, at least 2
 * @param[in] argv argument vector
 */
static void __netmgr_cli_wired(int argc, char *argv[])
{
    netmgr_link_info_t info;

    if (OPRT_OK != netmgr_link_info_get(NETCONN_WIRED, &info)) {
        PR_INFO("wired is not registered in this build");
        return;
    }

    /* argv[2] read only once argc says it is there. */
    if (argc >= 3) {
        PR_INFO("netmgr wired %s: not supported", argv[2]);
    }
    PR_INFO("wired control level is %s: netmgr can observe the link and read its "
            "ip/mac, but the TAL has no connect or disconnect to call",
            NETCONN_CTRL_TO_STR(info.ctrl));
    PR_INFO("use `netmgr` to see its current status");
}

/***********************************************************
************************* switch ***************************
***********************************************************/

/**
 * @brief Resolve a link name to its type, using the names the descriptors carry.
 *
 * An operator types "wifi", not the value of NETCONN_WIFI. The names come from
 * netconn_desc_t.name by way of netmgr_link_info_at(), so this function names no
 * technology and a link added to netconn_table.c becomes switchable with no edit
 * here - the same reason the dump walks the registry.
 *
 * @param[in] name  the token the operator typed, never NULL
 * @param[in] links netmgr_state_t.link_num
 *
 * @return the type, or NETCONN_AUTO when no registered link has that name. That
 *         is unambiguous as a "not found": NETCONN_AUTO is not a link, and the
 *         caller has already handled the literal "auto" before getting here.
 */
static netmgr_type_e __netmgr_cli_name_to_type(const char *name, uint32_t links)
{
    uint32_t i;

    for (i = 0; i < links; i++) {
        netmgr_link_info_t info;

        if (OPRT_OK != netmgr_link_info_at(i, &info)) {
            break;
        }
        if (0 == strcmp(info.name, name)) {
            return info.type;
        }
    }

    return NETCONN_AUTO;
}

/**
 * @brief `netmgr switch` usage, listing the names this build actually has.
 *
 * @param[in] links netmgr_state_t.link_num
 */
static void __netmgr_cli_switch_usage(uint32_t links)
{
    char     names[NETMGR_CLI_NAMES_STR_LEN];
    uint32_t used = 0;
    uint32_t i;

    names[0] = '\0';
    for (i = 0; i < links; i++) {
        netmgr_link_info_t info;

        if (OPRT_OK != netmgr_link_info_at(i, &info)) {
            break;
        }
        used += (uint32_t)snprintf(names + used, sizeof(names) - used, "%s%s", used ? "|" : "", info.name);
        if (used >= sizeof(names) - 1) {
            /* Truncated. Stop before snprintf() is handed a zero-size buffer. */
            break;
        }
    }

    PR_INFO("usage: netmgr switch <%s|auto>", (0 != used) ? names : "no link registered");
    PR_INFO("  a pin outranks priority, stickiness and dwell, and survives link");
    PR_INFO("  events, reselects and policy changes; `auto` releases it");
    PR_INFO("  a pin does not dial: pinning a down link only arms the pin, use");
    PR_INFO("  `netmgr wifi up <ssid>` to actually bring a managed link up");
}

/**
 * @brief `netmgr switch <name|auto>`.
 *
 * No reselect is asked for here on purpose. netmgr_policy_pin() calls
 * netmgr_reselect_request() itself, on both the arm and the release path, which is
 * what makes this command take effect at once rather than at the next unrelated
 * link event.
 *
 * @param[in] argc  argument count, at least 2
 * @param[in] argv  argument vector
 * @param[in] state the snapshot netmgr_cmd() took, for link_num
 */
static void __netmgr_cli_switch(int argc, char *argv[], const netmgr_state_t *state)
{
    netmgr_type_e type = NETCONN_AUTO;
    OPERATE_RET   rt   = OPRT_OK;

    /* argv[2] is the link name and is mandatory. */
    if (argc < 3) {
        __netmgr_cli_switch_usage(state->link_num);
        return;
    }

    /* A link name cannot contain a space, so a third token is a typo rather than
     * something to join - same reasoning as `netmgr wifi up`. */
    if (argc > 3) {
        PR_INFO("too many arguments: `netmgr switch` takes exactly one name");
        __netmgr_cli_switch_usage(state->link_num);
        return;
    }

    /* "auto" is not a link name, it is the release. Handled before the lookup so
     * a build that somehow had a link called "auto" could not shadow it. */
    if (0 == strcmp(argv[2], "auto")) {
        /* Documented as unable to fail: there is nothing to look up. */
        (void)netmgr_policy_pin(NETCONN_AUTO);
        PR_NOTICE("switch: pin released, the ranking chooses again from now on");
        return;
    }

    type = __netmgr_cli_name_to_type(argv[2], state->link_num);
    if (NETCONN_AUTO == type) {
        PR_INFO("no registered link is called \"%s\"", argv[2]);
        __netmgr_cli_switch_usage(state->link_num);
        return;
    }

    rt = netmgr_policy_pin(type);
    switch (rt) {
    case OPRT_OK:
        PR_NOTICE("switch: pinned to %s, which can carry traffic now", __netmgr_cli_type_name(type));
        break;

    case OPRT_RESOURCE_NOT_READY:
        /* NOT a failure, and the return this command most has to get right: the
         * pin IS armed and takes effect the moment the link becomes eligible.
         * Reporting it as an error would send an operator looking for a bug in a
         * command that did exactly what it was asked to do. */
        PR_NOTICE("switch: pin armed for %s, which cannot carry traffic yet", __netmgr_cli_type_name(type));
        PR_NOTICE("  the pin is remembered and takes effect when the link comes up;");
        PR_NOTICE("  it does not dial, so bring the link up yourself if it is managed");
        break;

    case OPRT_NOT_FOUND:
        /* The name came out of the registry a moment ago, so this is a teardown
         * racing the command rather than a typo. The pin is unchanged. */
        PR_INFO("switch: %s is no longer registered, pin unchanged", NETMGR_TYPE_TO_STR(type));
        /* The macro, not the registry helper: the lookup is what just failed,
         * so there is no descriptor left to take a name from. */
        break;

    default:
        PR_INFO("switch: pin failed, rt %d", rt);
        break;
    }
}

/***********************************************************
********************** init / deinit ***********************
***********************************************************/

/**
 * @brief The type mask to hand the next netmgr_init(), remembered at deinit.
 *
 * netmgr_state_t.configured is gone once netmgr_deinit() has run - step 6 zeroes
 * s_netmgr - so `netmgr init` has to have been told what to restore. 0 means
 * nothing was remembered, which __netmgr_cli_all_types() covers.
 */
static netmgr_type_e s_cli_last_configured = 0;

/**
 * @brief Every link type this build has a descriptor for.
 *
 * The fallback mask for `netmgr init` when nothing was remembered, i.e. when the
 * CLI was not what tore netmgr down. Asked of the registry rather than hard-coded
 * so this file still names no technology, and it is the widest honest answer:
 * netmgr_init() ignores a bit it has no row for anyway.
 *
 * @return the union of every row's type, or 0 when this build has no link driver
 */
static netmgr_type_e __netmgr_cli_all_types(void)
{
    const netconn_desc_t *table = NULL;
    uint32_t              mask  = 0;
    uint32_t              count = 0;
    uint32_t              i;

    table = netconn_registry_get_table(&count);
    if (NULL == table) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        mask |= (uint32_t)table[i].type;
    }

    return (netmgr_type_e)mask;
}

/**
 * @brief `netmgr deinit`.
 *
 * The first caller netmgr_deinit() has ever had, and that is the point of it.
 * Everything M0-M2 built into that function - the teardown gate closed before
 * anything else, the bounded drain, the deliberately RETAINED mutex, the
 * reverse-order unlink, `stopping` being restored after the memset, the probe
 * backend stop, the pin release - was until now reachable only from
 * netmgr_init()'s own error rollback, i.e. only on a device that failed to bring
 * up any link at all. This command runs it on a working one.
 *
 * WHY IT IS SAFE FROM HERE. netmgr.h forbids calling netmgr_deinit() from the
 * WORKQ_SYSTEM thread: step 3 drains the notify handler and would wait on itself.
 * tal_cli does not use a work queue - tal_cli_init_with_uart() creates a dedicated
 * "cli" thread running cli_task(), and cli_enter_key() -> cli_cmd_exec() calls
 * cmd->func() on that thread - so the drain here waits on a different thread and
 * the notify handler can finish. Every command in this file inherits that; one
 * moved into an EVENT_LINK_* subscriber would not, which is exactly the case the
 * contract rules out.
 *
 * @param[in] state the snapshot netmgr_cmd() took, for the mask to remember
 */
static void __netmgr_cli_deinit(const netmgr_state_t *state)
{
    OPERATE_RET rt = OPRT_OK;

    /* Remembered BEFORE the teardown zeroes it, so `netmgr init` restores the same
     * mask instead of guessing one. Only when there is something to remember: a
     * second `netmgr deinit` must not overwrite the first one's answer with 0. */
    if (0 != state->configured) {
        s_cli_last_configured = state->configured;
    }

    rt = netmgr_deinit();

    if (OPRT_TIMEOUT == rt) {
        /* Not a failed teardown - everything that could safely be torn down was,
         * and the retained mutex is what makes a straggler harmless. What it does
         * mean is that a notify handler was still inside when the drain window
         * expired, so work it had already started can still land AFTER this
         * returned; a tal_net_route_set() in particular, which would point the data
         * plane at a link netmgr no longer manages. Worth an operator's attention
         * before re-initing, which is why it is not folded into the generic error. */
        PR_ERR("netmgr deinit: drain timed out, a notify handler is still in flight");
        PR_ERR("  teardown finished anyway, but work that handler had already");
        PR_ERR("  started can still land, including a route push");
        return;
    }
    if (OPRT_OK != rt) {
        PR_ERR("netmgr deinit failed, rt %d", rt);
        return;
    }

    PR_NOTICE("netmgr deinit: torn down%s", state->inited ? "" : " (it was already down)");
    PR_NOTICE("  mask 0x%02x remembered for `netmgr init`", (unsigned int)s_cli_last_configured);
}

/**
 * @brief `netmgr init`.
 *
 * @param[in] state the snapshot netmgr_cmd() took, for the already-up check
 */
static void __netmgr_cli_init(const netmgr_state_t *state)
{
    netmgr_type_e mask       = 0;
    BOOL_T        remembered = FALSE;
    OPERATE_RET   rt         = OPRT_OK;

    /* netmgr_init() early-returns OPRT_OK when it is already up, so this check is
     * not what keeps a double init safe - it is here to say so out loud. Silently
     * reporting success for a command that did nothing reads as if the mask had
     * been applied, and `netmgr init` exists precisely to change nothing else.
     *
     * The reason netmgr_init() had to grow that early return: a second init used to
     * reset active, status and last_pin, and put route_epoch back to 1 while
     * netmgr_probe.c still held a session epoch from before the reset - so valid
     * verdicts were discarded as stale until the counter climbed back. The
     * connection list itself always survived, since __netmgr_conn_register()
     * refuses a link whose type is already registered.
     *
     * (An earlier version of this comment claimed the EVENT_MQTT_CONNECTED
     * subscription was added twice. It is not: both adders behind
     * tal_event_subscribe() match on desc plus callback and return OPRT_OK for a
     * duplicate, and netmgr passes constants for both.) */
    if (state->inited) {
        PR_INFO("netmgr is already up, `netmgr deinit` first");
        return;
    }

    remembered = (0 != s_cli_last_configured);
    mask       = remembered ? s_cli_last_configured : __netmgr_cli_all_types();
    if (0 == mask) {
        PR_INFO("netmgr init: this build has no link driver to register");
        return;
    }

    /* Said on the INIT side of the pair, because init is the half that installs
     * the callback. netmgr does not withdraw one it installed, because what NULL
     * means is undefined across the four tkl_wired implementations - two withdraw
     * cleanly, one ignores it, one crashes; see the table in
     * netconn_wired_close(). So on some platforms a callback stays live across a
     * deinit, and on LINUX a poller thread stays live with it. Recorded in the
     * netmgr_deinit() design note in netmgr_priv.h; closing it needs a TKL
     * contract for NULL, not a change here.
     *
     * The warning deliberately does NOT say the thread count climbs, which an
     * earlier version did. It does not, on the platform that ships:
     * platform/LINUX/tuyaos_adapter/src/tkl_wired.c guards its pthread_create()
     * with `if (!wired_event_thread)`, so there is one poller for the life of the
     * process no matter how many times netmgr is re-initialised. */
    PR_WARN("netmgr init: a re-init cannot withdraw callbacks a driver already");
    PR_WARN("  installed, so a platform poller may outlive the netmgr it calls");

    /* The other half of the caveat, and the one that ends the session rather than
     * leaking a thread. netmgr_init() runs every conn->open() on THIS thread - see
     * the stack note on netmgr_init() in netmgr.h - and this thread has
     * SERIAL_CLI_STACK_SIZE, 3072 bytes by default (src/tal_cli/Kconfig). The
     * figure is spelled in the message rather than read from the macro on purpose:
     * the symbol only exists in builds that enable the serial CLI, and this file
     * is compiled unconditionally. It is warned about rather than refused because
     * a stack-headroom test would have to run on the stack it is protecting. */
    PR_WARN("  and it opens every link on this CLI thread, whose stack is 3072");
    PR_WARN("  bytes by default - that can overflow on a build with bluetooth");

    rt = netmgr_init(mask);
    if (OPRT_OK != rt) {
        /* netmgr_init() rolls itself back through netmgr_deinit() on every error
         * path since M2, so there is nothing to clean up here and the command can
         * simply be retried. */
        PR_ERR("netmgr init failed, mask 0x%02x, rt %d", (unsigned int)mask, rt);
        return;
    }

    PR_NOTICE("netmgr init: up, mask 0x%02x (%s)", (unsigned int)mask,
              remembered ? "remembered" : "every link in this build");
}

/***********************************************************
*********************** entry point ************************
***********************************************************/

void netmgr_cmd(int argc, char *argv[])
{
    netmgr_state_t state = {0};

    /* argv[0] is the command name; the tokenizer never calls a handler with
     * argc < 1, but this file does not get to assume that. */
    if (argc < 1 || NULL == argv) {
        return;
    }

    /* Cannot fail with a non-NULL argument, and "not inited" is a legitimate
     * answer rather than an error - it is reported through state.inited below. */
    if (OPRT_OK != netmgr_state_get(&state)) {
        PR_INFO("netmgr state unavailable");
        return;
    }

    /* init and deinit are dispatched BEFORE the readiness gate, and they have to
     * be. `netmgr init` exists precisely to be run on a netmgr that is DOWN, so
     * behind the gate it would be unreachable - after `netmgr deinit` the only
     * command that could undo it would refuse to run. netmgr_deinit() is
     * documented idempotent and safe when netmgr_init() never ran, so it is
     * meaningful with inited FALSE too and is dispatched here for symmetry.
     * Everything else needs a live netmgr and stays behind the gate. */
    if (argc >= 2 && 0 == strcmp(argv[1], "init")) {
        __netmgr_cli_init(&state);
        return;
    }
    if (argc >= 2 && 0 == strcmp(argv[1], "deinit")) {
        __netmgr_cli_deinit(&state);
        return;
    }

    if (!state.inited) {
        PR_INFO("network not ready! `netmgr init` brings it up");
        return;
    }

    if (1 == argc) {
        __netmgr_cli_dump(&state);
        return;
    }

    /* argc >= 2 from here, so argv[1] is a real token. */
    if (0 == strcmp(argv[1], "wifi")) {
        __netmgr_cli_wifi(argc, argv);
    } else if (0 == strcmp(argv[1], "wired")) {
        __netmgr_cli_wired(argc, argv);
    } else if (0 == strcmp(argv[1], "switch")) {
        __netmgr_cli_switch(argc, argv, &state);
    } else {
        __netmgr_cli_usage();
    }
}
