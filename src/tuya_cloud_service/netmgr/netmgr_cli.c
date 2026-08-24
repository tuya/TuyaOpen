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
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr.h"
#include "netmgr_priv.h"
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
*************************** dump ***************************
***********************************************************/

/**
 * @brief Print the global state and one row per registered link.
 *
 * @param[in] state a snapshot already taken by the caller
 */
static void __netmgr_cli_dump(const netmgr_state_t *state)
{
    char     caps[NETMGR_CLI_CAPS_STR_LEN];
    uint32_t i;

    PR_NOTICE("netmgr: configured 0x%02x, active %s, status %s, links %u", (unsigned int)state->configured,
              NETMGR_TYPE_TO_STR(state->active), NETMGR_STATUS_TO_STR(state->status), (unsigned int)state->link_num);
    PR_NOTICE("  idx name      pri status         ctrl      prov caps");
    PR_NOTICE("  --------------------------------------------------------------");

    for (i = 0; i < state->link_num; i++) {
        netmgr_link_info_t info;

        /* One snapshot per row: each call takes and releases the lock, so the
         * PR_NOTICE() below never runs under it. A link cannot appear or
         * disappear between netmgr_init() and netmgr_deinit(), so the only way
         * this returns OPRT_NOT_FOUND is a teardown racing the dump - stop
         * rather than print a half-torn-down table. */
        if (OPRT_OK != netmgr_link_info_at(i, &info)) {
            PR_NOTICE("  (link %u vanished, netmgr is being torn down)", (unsigned int)i);
            break;
        }

        __netmgr_cli_caps_str(info.caps, caps, sizeof(caps));
        PR_NOTICE("  %s%-2u %-9s %3u %-14s %-9s %4u %s", (info.type == state->active) ? "*" : " ", (unsigned int)i,
                  info.name, (unsigned int)info.pri, NETMGR_STATUS_TO_STR(info.status), NETCONN_CTRL_TO_STR(info.ctrl),
                  (unsigned int)info.provider, caps);
    }
}

/***********************************************************
************************** usage ***************************
***********************************************************/

static void __netmgr_cli_usage(void)
{
    PR_INFO("usage:");
    PR_INFO("  netmgr                            dump links and active route");
    PR_INFO("  netmgr wifi up <ssid> [password]  join an AP, no password means open");
    PR_INFO("  netmgr wifi down                  leave the current AP");
    PR_INFO("  netmgr wifi scan                  list nearby APs");
    PR_INFO("  netmgr wired [up|down]            not supported, wired is observe-only");
    PR_INFO("  netmgr switch                     not implemented yet");
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

    if (OPRT_OK != netmgr_state_get(&state) || !state.inited) {
        PR_INFO("network not ready!");
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
        /* Manual link selection is M3. Keep this an explicit "not implemented"
         * rather than a half-working route override. */
        PR_INFO("netmgr switch: not implemented yet, manual link selection lands in M3");
    } else {
        __netmgr_cli_usage();
    }
}
