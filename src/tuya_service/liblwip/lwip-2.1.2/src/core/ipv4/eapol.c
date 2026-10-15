#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/stats.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/err.h"
#include "lwip/eapol.h"
#include "ethernetif.h"

void etheapol_input(struct pbuf *pbuf, struct netif *netif)
{
    int ret, vif_index;

    if (NULL == pbuf) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Null pbuf\n", __func__));
        return;
    }

    if (NULL == netif) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Null netif\n", __func__));
        goto recv_exit;
    }

    /* Is it need check the interface up??? */
    if (!netif_is_link_up(netif)) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: vif_index %d is not up\n", __func__, vif_index));
    }

    /* Because the if index of HOSTAPD and WLAN driver start zero */
    vif_index = pbuf->if_idx - 1;

    LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Recv eapol packet if_idx %d len %d num %d vif_index %d\n", __func__, pbuf->if_idx,
                                 pbuf->len, netif->num, vif_index));

#if 0 /* jshang-for-testing */
    printf("%s: Recv eapol packet if_idx %d len %d num %d vif_index %d\r\n",
        __func__, pbuf->if_idx, pbuf->len, netif->num, vif_index);

#endif

    ret = tuya_hostap_eapol_input(vif_index, pbuf->payload, pbuf->len);
    if (ret < 0) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: call tuya_hostap_eapol_input failed\n", __func__));
    }

recv_exit:

    pbuf_free(pbuf);
}

int etheapol_output(int vif_index, unsigned char *buf, unsigned short len)
{
    int             ret;
    struct pbuf    *p;
    struct netif   *netif;
    struct eth_hdr *eth;
    unsigned char  *data;
    unsigned short  data_len;

    if (NULL == buf || 0 == len) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Null buf\n", __func__));
        return ERR_ARG;
    }

    if (vif_index > (NETIF_NUM - 1)) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Invalid vif_index %d\n", __func__, vif_index));
        return ERR_ARG;
    }

    netif = tuya_ethernetif_get_netif_by_index(vif_index);
    if (NULL == netif) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Get netif by vif_index %d failed\n", __func__, vif_index));
        return ERR_ARG;
    }

    /* Is it need check the interface up??? */
    if (!netif_is_link_up(netif)) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: vif_index %d is not up\n", __func__, vif_index));
    }

    /* Offset the ethernet header for ethernet output,
     * because of ethernet header has been added by hostapd
     */
    data     = buf + sizeof(struct eth_hdr);
    data_len = len - sizeof(struct eth_hdr);

    LWIP_DEBUGF(ETHEAPOL_DEBUG,
                ("%s: Send eapol packet via vif_index %d, len %d data_len %d\n", __func__, vif_index, len, data_len));
#if 0 /* jshang-for-testing */
    printf("%s: Send eapol packet via vif_index %d, len %d data_len %d\r\n",
        __func__, vif_index, len, data_len);
#endif

    p = pbuf_alloc(PBUF_LINK, data_len, PBUF_RAM);
    if (NULL == p) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: Malloc memory failed(data_len=%d)\n", __func__, data_len));
        return ERR_MEM;
    }

    memcpy(p->payload, data, data_len);

    p->len     = data_len;
    p->tot_len = data_len;

    eth = (struct eth_hdr *)buf;
    ret = ethernet_output(netif, p, &eth->src, &eth->dest, lwip_ntohs(eth->type));
    if (ret < 0) {
        LWIP_DEBUGF(ETHEAPOL_DEBUG, ("%s: call ethernet_output failed(ret=%d)\n", __func__, ret));
    }

    pbuf_free(p);
    return ERR_OK;
}
