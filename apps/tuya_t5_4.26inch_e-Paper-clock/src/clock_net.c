#include <string.h>

#include "clock_net.h"

#include "netmgr.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif

static const char *nw_ip_str_local(const NW_IP_S *ip)
{
    if (!ip) {
        return "";
    }
#if defined(ENABLE_IPv6) && (ENABLE_IPv6 == 1)
    return ip->nwipstr;
#else
    return ip->ip;
#endif
}

void clock_net_info_get(clock_net_info_t *out)
{
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->link = NETMGR_LINK_DOWN;

    netmgr_status_e status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status) == OPRT_OK) {
        out->link = status;
    }

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netconn_wifi_info_t wifi_info = {0};
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info) == OPRT_OK) {
        strncpy(out->ssid, wifi_info.ssid, sizeof(out->ssid) - 1);
        out->ssid[sizeof(out->ssid) - 1] = '\0';
    }

    NW_IP_S ip = {0};
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_IP, &ip) == OPRT_OK) {
        const char *s = nw_ip_str_local(&ip);
        strncpy(out->ip, s ? s : "", sizeof(out->ip) - 1);
        out->ip[sizeof(out->ip) - 1] = '\0';
    }
#endif
}

