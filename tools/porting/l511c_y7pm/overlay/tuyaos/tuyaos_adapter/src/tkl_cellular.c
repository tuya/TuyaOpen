/**
 * @file tkl_cellular.c
 * @brief TuyaOpen cellular TKL for the L511C module.
 *
 * TuyaOpen's tal_cellular / netconn_cellular talk to a small interface --
 * bring the data call up, report the link status, read a handful of module
 * identifiers -- while the OEM adapter exposes the wider TuyaOS cellular API
 * (base / mds / comm / vbat). This file maps one onto the other.
 *
 * The modem is on-chip, so the interface and protocol fields of
 * TKL_CELLULAR_BASE_CFG_T (meant for an external module reached over USB or
 * UART) carry no meaning here and are ignored.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tkl_cellular.h"
#include "tkl_init_cellular.h"
#include "tkl_cellular_base.h"
#include "tkl_cellular_comm.h"
#include "tkl_cellular_mds.h"
#include "tkl_cellular_vbat.h"
#include "vlog.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* The module has a single SIM on a single channel -- see
 * tkl_cellular_base_get_ability(), which always answers
 * SINGLE_SIM_SINGLE_CHANNEL. */
#define CELLULAR_SIM_ID 0

/* tkl_cellular_comm_get_sn() and tkl_cellular_comm_get_sysfw_ver() write into
 * fixed-size buffers; TuyaOpen only hands over a char *, so its buffer has to
 * be at least this large. TKL_CELLULAR_SN_LEN / TKL_CELLULAR_SW_VER_LEN in
 * tkl_cellular.h are smaller, hence the local staging buffers below. */
#define VENDOR_SN_BUF_LEN     25
#define VENDOR_SW_VER_BUF_LEN 32

/***********************************************************
***********************variable define**********************
***********************************************************/
static TKL_CELLULAR_STATUS_CHANGE_CB s_status_cb = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __mds_net_status_notify(uint8_t sim_id, TUYA_CELLULAR_MDS_NET_STATUS_E st)
{
    if (sim_id != CELLULAR_SIM_ID || s_status_cb == NULL) {
        return;
    }

    s_status_cb((TUYA_CELLULAR_MDS_NET_CONNECT == st) ? TKL_CELLULAR_LINK_UP : TKL_CELLULAR_LINK_DOWN);
}

/**
 * @brief  init create cellular link
 *
 * @param[in]   cfg: the configure for cellular link
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tkl_cellular_init(TKL_CELLULAR_BASE_CFG_T *cfg)
{
    OPERATE_RET           rt = OPRT_OK;
    TKL_CELL_INIT_PARAM_T param;
    char                 *apn = NULL;

    memset(&param, 0, sizeof(param));
    param.fd       = -1; /* on-chip modem: no host-side port to open */
    param.sim_id   = CELLULAR_SIM_ID;
    param.sim_type = TUYA_TSIM_TYPE;

    rt = tkl_cellular_base_init(&param);
    if (OPRT_OK != rt) {
        LOGE("cellular base init failed: %d", rt);
        return rt;
    }

    rt = tkl_cellular_mds_init(CELLULAR_SIM_ID);
    if (OPRT_OK != rt) {
        LOGE("cellular mds init failed: %d", rt);
        return rt;
    }

    rt = tkl_cellular_mds_register_state_notify(CELLULAR_SIM_ID, __mds_net_status_notify);
    if (OPRT_OK != rt) {
        LOGE("register mds notify failed: %d", rt);
        return rt;
    }

    /* An empty apn leaves the network to pick the default profile. */
    if (cfg != NULL && cfg->apn[0] != '\0') {
        apn = cfg->apn;
    }

    rt = tkl_cellular_mds_pdp_active(CELLULAR_SIM_ID, apn, NULL, NULL);
    if (OPRT_OK != rt) {
        LOGE("pdp active failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

/**
 * @brief  get the link status of cellular link
 *
 * @param[out]  status: the cellular link status
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_status(TKL_CELLULAR_STAT_E *status)
{
    TUYA_CELLULAR_MDS_STATUS_E st;

    if (NULL == status) {
        return OPRT_INVALID_PARM;
    }

    /* Only an activated PDP context means there is a usable data link;
     * registration alone is not enough. */
    st      = tkl_cellular_mds_get_status(CELLULAR_SIM_ID);
    *status = (TUYA_CELLULAR_MDS_STATUS_ACTIVE == st) ? TKL_CELLULAR_LINK_UP : TKL_CELLULAR_LINK_DOWN;
    return OPRT_OK;
}

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_set_status_cb(TKL_CELLULAR_STATUS_CHANGE_CB cb)
{
    s_status_cb = cb;
    return tkl_cellular_mds_register_state_notify(CELLULAR_SIM_ID, __mds_net_status_notify);
}

/**
 * @brief  get the ip address of the cellular link
 *
 * @param[out]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_ip(NW_IP_S *ip)
{
    if (NULL == ip) {
        return OPRT_INVALID_PARM;
    }
    return tkl_cellular_mds_get_ip(CELLULAR_SIM_ID, ip);
}

/**
 * @brief  get the ipv6 address of the cellular link
 *
 * @param[in]   type: the ipv6 address type
 * @param[out]  ip: the ipv6 address
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_ipv6(NW_IP_TYPE type, NW_IP_S *ip)
{
    (void)type;
    (void)ip;
    /* The OEM mds layer only reports the IPv4 address of the context. */
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief  get the ccid of the cellular link
 *
 * @param[out]   ccid: ccid string, at least TKL_CELLULAR_CCID_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_ccid(char *ccid)
{
    if (NULL == ccid) {
        return OPRT_INVALID_PARM;
    }
    return tkl_cellular_base_get_iccid(CELLULAR_SIM_ID, ccid);
}

/**
 * @brief  get the rssi of the cellular link
 *
 * @param[out]   rssi: rssi value, in dBm
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_rssi(char *rssi)
{
    OPERATE_RET rt       = OPRT_OK;
    int         rssi_dbm = 0;

    if (NULL == rssi) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_cellular_comm_get_rssi_dBm(CELLULAR_SIM_ID, &rssi_dbm);
    if (OPRT_OK != rt) {
        return rt;
    }

    *rssi = (char)rssi_dbm;
    return OPRT_OK;
}

/**
 * @brief  get the voltage of the cellular module
 *
 * @param[out]   volt: voltage value, in mV
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_volt(uint32_t *volt)
{
    if (NULL == volt) {
        return OPRT_INVALID_PARM;
    }
    return tkl_cellular_vbat_get_voltage(volt);
}

/**
 * @brief  get the IMEI of the cellular module
 *
 * @param[out]   imei: IMEI value, at least TKL_CELLULAR_IMEI_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_imei(char *imei)
{
    if (NULL == imei) {
        return OPRT_INVALID_PARM;
    }
    return tkl_cellular_base_get_imei(CELLULAR_SIM_ID, imei);
}

/**
 * @brief  get the Serial Number of the cellular module
 *
 * @param[out]   sn: Serial Number, at least TKL_CELLULAR_SN_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_sn(char *sn)
{
    OPERATE_RET rt                     = OPRT_OK;
    char        buf[VENDOR_SN_BUF_LEN] = {0};

    if (NULL == sn) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_cellular_comm_get_sn(CELLULAR_SIM_ID, buf);
    if (OPRT_OK != rt) {
        return rt;
    }

    buf[sizeof(buf) - 1] = '\0';
    strncpy(sn, buf, TKL_CELLULAR_SN_LEN);
    sn[TKL_CELLULAR_SN_LEN] = '\0';
    return OPRT_OK;
}

/**
 * @brief  get the Software version of the cellular module
 *
 * @param[out]   ver: Software version, at least TKL_CELLULAR_SW_VER_LEN + 1
 *                    bytes
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_get_sw_ver(char *ver)
{
    OPERATE_RET rt                         = OPRT_OK;
    char        buf[VENDOR_SW_VER_BUF_LEN] = {0};

    if (NULL == ver) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_cellular_comm_get_sysfw_ver(buf);
    if (OPRT_OK != rt) {
        return rt;
    }

    buf[sizeof(buf) - 1] = '\0';
    strncpy(ver, buf, TKL_CELLULAR_SW_VER_LEN);
    ver[TKL_CELLULAR_SW_VER_LEN] = '\0';
    return OPRT_OK;
}

/**
 * @brief  start cellular mf test
 *
 * @param[in]   cfg: the configure for cellular mf test
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_mf_test_start(TKL_CELLULAR_BASE_CFG_T *cfg)
{
    (void)cfg;
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief  stop cellular mf test
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tkl_cellular_mf_test_stop(void)
{
    return OPRT_NOT_SUPPORTED;
}

static TKL_CELLULAR_DESC_T s_cellular_desc = {
    .init          = tkl_cellular_init,
    .get_status    = tkl_cellular_get_status,
    .set_status_cb = tkl_cellular_set_status_cb,
    .get_ip        = tkl_cellular_get_ip,
    .get_ipv6      = tkl_cellular_get_ipv6,
    .get_ccid      = tkl_cellular_get_ccid,
    .get_rssi      = tkl_cellular_get_rssi,
    .get_volt      = tkl_cellular_get_volt,
    .get_imei      = tkl_cellular_get_imei,
    .get_sn        = tkl_cellular_get_sn,
    .get_sw_ver    = tkl_cellular_get_sw_ver,
};

TKL_CELLULAR_DESC_T *tkl_cellular_desc_get(void)
{
    return &s_cellular_desc;
}
