/**
 * @file netconn_table.c
 * @brief The link registry: the one file in netmgr that names technologies.
 *
 * netmgr.c used to carry one `#ifdef ENABLE_<TECH>` block per technology, each
 * declaring `extern netmgr_conn_<tech>_t s_netmgr_<tech>` and registering it by
 * hand. That knowledge lives here now, as a table of netconn_desc_t rows, and
 * netmgr.c walks the table without knowing what a wifi is.
 *
 * This translation unit is therefore the ONLY one in the module allowed to carry
 * `#ifdef ENABLE_<TECH>`: a row may only exist when its driver is linked, and
 * whether a driver is linked is exactly what those macros say (see
 * src/tuya_cloud_service/CMakeLists.txt, which gates netconn_wifi.c,
 * netconn_wired.c and netconn_cellular.c on the matching CONFIG_ENABLE_*).
 *
 * The guards are spelled `#if defined(X) && (X == 1)` rather than `#ifdef X`, to
 * match netconn_cellular.c's own guard around s_netmgr_cellular. netmgr.c used
 * `#ifdef ENABLE_CELLULAR` while the driver compiles its instance out unless
 * ENABLE_CELLULAR is 1, so an explicit `ENABLE_CELLULAR 0` produced an undefined
 * reference. Matching the driver removes that mismatch.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netconn_registry.h"
#include "tal_api.h"

/* For TAL_NET_PROVIDER_DEFAULT, the provider every in-tree driver starts at. */
#include "tal_network_register.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
extern netmgr_conn_wifi_t s_netmgr_wifi;
#define NETCONN_TABLE_HAS_WIFI 1
#else
#define NETCONN_TABLE_HAS_WIFI 0
#endif

#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
extern netmgr_conn_wired_t s_netmgr_wired;
#define NETCONN_TABLE_HAS_WIRED 1
#else
#define NETCONN_TABLE_HAS_WIRED 0
#endif

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "netconn_cellular.h"
extern netmgr_conn_cellular_t s_netmgr_cellular;
#define NETCONN_TABLE_HAS_CELLULAR 1
#else
#define NETCONN_TABLE_HAS_CELLULAR 0
#endif

#define NETCONN_TABLE_HAS_ANY (NETCONN_TABLE_HAS_WIRED || NETCONN_TABLE_HAS_CELLULAR || NETCONN_TABLE_HAS_WIFI)

/***********************************************************
********************* attribute masks **********************
***********************************************************/

/* Every mask below is a bit-for-bit transcription of the `switch (cmd)` in the
 * driver it describes: one bit per `case` arm that does NOT return
 * OPRT_NOT_SUPPORTED. netmgr screens against these before dispatching, so a bit
 * missing here turns a command the driver accepts today into
 * OPRT_NOT_SUPPORTED. Re-derive them from the driver, do not guess.
 *
 * Two transcription rules worth stating, because both come up below:
 *
 *   - an arm that only breaks and falls through to `return OPRT_OK` still counts
 *     as supported, even when it does nothing;
 *   - an arm that explicitly answers OPRT_NOT_SUPPORTED does not count -
 *     netconn_cellular_get()'s NETCONN_CMD_MAC. Screening it out one layer up
 *     hands the same code to the same caller.
 *
 * NETCONN_CMD_CLOSE is where the transcription has to agree with the row's .ctrl
 * level, so it is called out here. Only a NETCONN_CTRL_MANAGED link can honour
 * the command, and only wifi carries the bit. wired (OBSERVE) and cellular
 * (SUSTAINED) leave it out deliberately: neither TAL layer exposes any way to
 * bring the link down, so the OPRT_OK they used to answer with was a lie told to
 * tuya_iot_destroy(). The OPRT_NOT_SUPPORTED this screen returns instead is less
 * convenient and more true.
 */

#if NETCONN_TABLE_HAS_WIFI
/* netconn_wifi_set(): PRI, IP, MAC, SSID_PSWD, COUNTRYCODE, NETCFG, CLOSE,
 * RESET, RECONN_TABLE. No STATUS, no SET_STATUS_CB. */
#define NETCONN_WIFI_SET_MASK                                                                                          \
    (NETCONN_ATTR_BIT(NETCONN_CMD_PRI) | NETCONN_ATTR_BIT(NETCONN_CMD_IP) | NETCONN_ATTR_BIT(NETCONN_CMD_MAC) |        \
     NETCONN_ATTR_BIT(NETCONN_CMD_SSID_PSWD) | NETCONN_ATTR_BIT(NETCONN_CMD_COUNTRYCODE) |                             \
     NETCONN_ATTR_BIT(NETCONN_CMD_NETCFG) | NETCONN_ATTR_BIT(NETCONN_CMD_CLOSE) |                                      \
     NETCONN_ATTR_BIT(NETCONN_CMD_RESET) | NETCONN_ATTR_BIT(NETCONN_CMD_RECONN_TABLE))

/* netconn_wifi_get(): PRI, MAC, SSID_PSWD, COUNTRYCODE, IP, NETCFG, STATUS.
 * No SET_STATUS_CB, CLOSE, RESET or RECONN_TABLE. */
#define NETCONN_WIFI_GET_MASK                                                                                          \
    (NETCONN_ATTR_BIT(NETCONN_CMD_PRI) | NETCONN_ATTR_BIT(NETCONN_CMD_IP) | NETCONN_ATTR_BIT(NETCONN_CMD_MAC) |        \
     NETCONN_ATTR_BIT(NETCONN_CMD_STATUS) | NETCONN_ATTR_BIT(NETCONN_CMD_SSID_PSWD) |                                  \
     NETCONN_ATTR_BIT(NETCONN_CMD_COUNTRYCODE) | NETCONN_ATTR_BIT(NETCONN_CMD_NETCFG))

/* SoftAP provisioning is unconditional: ap_netcfg is compiled in alongside the
 * wifi driver. BLE provisioning only when a BLE netcfg backend is actually
 * linked, which is the same condition netconn_wifi.c puts around ble_netcfg. */
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
#define NETCONN_WIFI_CAPS (NETCONN_CAP_LAN | NETCONN_CAP_NETCFG_AP | NETCONN_CAP_NETCFG_BLE)
#else
#define NETCONN_WIFI_CAPS (NETCONN_CAP_LAN | NETCONN_CAP_NETCFG_AP)
#endif
#endif /* NETCONN_TABLE_HAS_WIFI */

#if NETCONN_TABLE_HAS_WIRED
/* netconn_wired_set(): PRI, IP, MAC. Everything else hits the default arm. */
#define NETCONN_WIRED_SET_MASK                                                                                         \
    (NETCONN_ATTR_BIT(NETCONN_CMD_PRI) | NETCONN_ATTR_BIT(NETCONN_CMD_IP) | NETCONN_ATTR_BIT(NETCONN_CMD_MAC))

/* netconn_wired_get(): PRI, IP, MAC, STATUS. No CLOSE: closing a link is not an
 * attribute a getter can read, and no caller in the tree ever asked for it. */
#define NETCONN_WIRED_GET_MASK                                                                                         \
    (NETCONN_ATTR_BIT(NETCONN_CMD_PRI) | NETCONN_ATTR_BIT(NETCONN_CMD_IP) | NETCONN_ATTR_BIT(NETCONN_CMD_MAC) |        \
     NETCONN_ATTR_BIT(NETCONN_CMD_STATUS))
#endif /* NETCONN_TABLE_HAS_WIRED */

#if NETCONN_TABLE_HAS_CELLULAR
/* netconn_cellular_set(): PRI only. No CLOSE: tal_cellular.h has
 * tal_cellular_init() but no deinit, and no connect/disconnect pair, so nothing
 * this driver can call brings the bearer down. That is what the row's
 * NETCONN_CTRL_SUSTAINED records. */
#define NETCONN_CELLULAR_SET_MASK (NETCONN_ATTR_BIT(NETCONN_CMD_PRI))

/* netconn_cellular_get(): PRI, STATUS, IP. MAC is an explicit
 * OPRT_NOT_SUPPORTED arm, so it stays out. */
#define NETCONN_CELLULAR_GET_MASK                                                                                      \
    (NETCONN_ATTR_BIT(NETCONN_CMD_PRI) | NETCONN_ATTR_BIT(NETCONN_CMD_STATUS) | NETCONN_ATTR_BIT(NETCONN_CMD_IP))
#endif /* NETCONN_TABLE_HAS_CELLULAR */

/***********************************************************
********************* the default table ********************
***********************************************************/

#if NETCONN_TABLE_HAS_ANY

/* Row order is wired, cellular, wifi - byte for byte the order netmgr_init()
 * registered them in before M2. The three in-tree default priorities differ
 * (2/0/1) so today's priority-sorted list comes out the same whatever the
 * registration order is; two links given the SAME priority are ordered by
 * registration alone, because __netmgr_conn_register() inserts before the first
 * strictly-lower priority and so keeps equals in arrival order. Preserving the
 * order is what makes M2 provably behaviour neutral rather than probably
 * behaviour neutral.
 */
static const netconn_desc_t s_netconn_default_table[] = {
#if NETCONN_TABLE_HAS_WIRED
    {
        .name = "wired",
        .type = NETCONN_WIRED,
        /* LAN direct connection makes sense on ethernet: it is one of the two
         * types __tuya_lan_init_tm_cb() tests for today. Not metered, and no
         * provisioning of its own - wired activates over mqtt_bind. */
        .caps        = NETCONN_CAP_LAN,
        .ctrl        = NETCONN_CTRL_OBSERVE,
        .default_pri = 2,
        .provider    = TAL_NET_PROVIDER_DEFAULT,
        .set_mask    = NETCONN_WIRED_SET_MASK,
        .get_mask    = NETCONN_WIRED_GET_MASK,
        .conn        = &s_netmgr_wired.base,
    },
#endif
#if NETCONN_TABLE_HAS_CELLULAR
    {
        .name = "cellular",
        .type = NETCONN_CELLULAR,
        /* Billed by volume, and LAN discovery over a 4G bearer is meaningless.
         *
         * Read this as intent, not as behaviour: nothing consumes the bit. The
         * previous wording said "M3 replaces that test with these bits", and both
         * halves of that are now wrong - M3 keyed the LAN gate on the ACTIVE
         * link's NETCONN_CAP_LAN rather than on this bit, and the
         * `#if !defined(ENABLE_CELLULAR)` around the LAN timer that it promised to
         * replace was deleted outright. So the LAN half of the intent is served,
         * by a different bit; the metered half is not served at all, because
         * netmgr_policy.c never reads caps. See NETCONN_CAP_METERED in
         * netconn_registry.h, which records the gap and what closing it needs. */
        .caps        = NETCONN_CAP_METERED,
        .ctrl        = NETCONN_CTRL_SUSTAINED,
        .default_pri = 0,
        .provider    = TAL_NET_PROVIDER_DEFAULT,
        .set_mask    = NETCONN_CELLULAR_SET_MASK,
        .get_mask    = NETCONN_CELLULAR_GET_MASK,
        .conn        = &s_netmgr_cellular.base,
    },
#endif
#if NETCONN_TABLE_HAS_WIFI
    {
        .name        = "wifi",
        .type        = NETCONN_WIFI,
        .caps        = NETCONN_WIFI_CAPS,
        .ctrl        = NETCONN_CTRL_MANAGED,
        .default_pri = 1,
        .provider    = TAL_NET_PROVIDER_DEFAULT,
        .set_mask    = NETCONN_WIFI_SET_MASK,
        .get_mask    = NETCONN_WIFI_GET_MASK,
        .conn        = &s_netmgr_wifi.base,
    },
#endif
};

#define NETCONN_DEFAULT_TABLE_NUM ((uint32_t)(sizeof(s_netconn_default_table) / sizeof(s_netconn_default_table[0])))

#else /* !NETCONN_TABLE_HAS_ANY */

/* A build with no link driver at all is legal - netmgr_init() reports it and
 * fails - and an empty array initialiser is not ISO C, so there simply is no
 * default table in that configuration. */
#define s_netconn_default_table   ((const netconn_desc_t *)NULL)
#define NETCONN_DEFAULT_TABLE_NUM ((uint32_t)0)

#endif /* NETCONN_TABLE_HAS_ANY */

/***********************************************************
********************* the active table *********************
***********************************************************/

/* The board override, or NULL for "use the default". Written once from
 * board_register_hardware(), read from netmgr_init() and from
 * netconn_registry_find(). No lock: the header requires set_table() to run
 * before netmgr_init(), and the table is immutable from then on. The latch below
 * enforces that ordering rather than trusting it. */
static const netconn_desc_t *s_table       = NULL;
static uint32_t              s_table_num   = 0;
static BOOL_T                s_table_taken = FALSE;

/**
 * @brief Resolve the table in force: the board override, else the default.
 */
static void __registry_active(const netconn_desc_t **table, uint32_t *num)
{
    if (NULL != s_table) {
        *table = s_table;
        *num   = s_table_num;
    } else {
        *table = s_netconn_default_table;
        *num   = NETCONN_DEFAULT_TABLE_NUM;
    }
}

const netconn_desc_t *netconn_registry_get_table(uint32_t *count)
{
    const netconn_desc_t *table = NULL;
    uint32_t              num   = 0;

    __registry_active(&table, &num);

    /* netmgr_init() is the only caller and it calls this exactly once, so
     * handing the table out is the moment registration begins. Latching here is
     * what lets netconn_registry_set_table() reject a late override without
     * netmgr having to report back into the registry. */
    s_table_taken = TRUE;

    /* An empty table is "no links", and the header promises NULL for that, so a
     * caller never has to test both. */
    if (NULL == table || 0 == num) {
        if (NULL != count) {
            *count = 0;
        }
        return NULL;
    }

    if (NULL != count) {
        *count = num;
    }

    return table;
}

OPERATE_RET netconn_registry_set_table(const netconn_desc_t *table, uint32_t count)
{
    uint32_t i = 0;
    uint32_t j = 0;

    if (NULL == table || 0 == count) {
        PR_ERR("netmgr registry set table failed, table is empty");
        return OPRT_INVALID_PARM;
    }

    /* Silently reverting to the defaults is the failure mode this call exists to
     * avoid - see the weak-symbol note in the header - so a late override is an
     * error, not a no-op. */
    if (s_table_taken) {
        PR_ERR("netmgr registry set table failed, links already registered");
        return OPRT_COM_ERROR;
    }

    /* Validated here rather than tripped over row by row later: a malformed
     * board table is a build-time mistake and belongs to the call that
     * installed it. */
    for (i = 0; i < count; i++) {
        if (NULL == table[i].name || NULL == table[i].conn) {
            PR_ERR("netmgr registry set table failed, row %d incomplete", i);
            return OPRT_INVALID_PARM;
        }
        if (NETCONN_AUTO == table[i].type) {
            PR_ERR("netmgr registry set table failed, row %d is NETCONN_AUTO", i);
            return OPRT_INVALID_PARM;
        }
        for (j = 0; j < i; j++) {
            if (table[j].type == table[i].type) {
                PR_ERR("netmgr registry set table failed, rows %d and %d share a type", j, i);
                return OPRT_INVALID_PARM;
            }
        }
    }

    /* Nothing is copied: the header makes the board own storage that outlives
     * netmgr, which a `static const` array in the board's own translation unit
     * satisfies for free. */
    s_table     = table;
    s_table_num = count;

    PR_DEBUG("netmgr registry table overridden, %d links", count);

    return OPRT_OK;
}

const netconn_desc_t *netconn_registry_find(netmgr_type_e type)
{
    const netconn_desc_t *table = NULL;
    uint32_t              num   = 0;
    uint32_t              i     = 0;

    /* NETCONN_AUTO is a selection request, not a link. Resolving it needs
     * netmgr's live state, which the registry does not have. */
    if (NETCONN_AUTO == type) {
        return NULL;
    }

    __registry_active(&table, &num);
    if (NULL == table) {
        return NULL;
    }

    /* Deliberately does not latch s_table_taken: this is a read-only lookup that
     * netmgr, the CLI and M3's policy layer all make, and it must not decide
     * whether a board override is still allowed. */
    for (i = 0; i < num; i++) {
        if (table[i].type == type) {
            return &table[i];
        }
    }

    return NULL;
}
