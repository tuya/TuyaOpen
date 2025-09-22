# WiFi AP

## Introduction

When the `example_wifi_ap` example is called, it first creates an AP hotspot, with the default hotspot name (SSID) being "my-wifi" and the hotspot password being "12345678". After the hotspot is successfully created, it will broadcast UDP.

## Process Introduction

The process of the `example_wifi_ap` example is: **WiFi Initialization -> Set WiFi Mode to AP Mode -> Start AP -> Broadcast UDP every 5s**.

The most important code snippet in the `example_wifi_ap` example is the `WF_AP_CFG_IF_S` structure, which is detailed below:

```c
typedef struct {
    UCHAR_T ssid[WIFI_SSID_LEN+1];       ///< ssid
    UCHAR_T s_len;                       ///< length of ssid
    UCHAR_T passwd[WIFI_PASSWD_LEN+1];   ///< password
    UCHAR_T p_len;                       ///< length of password
    UCHAR_T chan;                        ///< channel, default:6
    WF_AP_AUTH_MODE_E md;                ///< encryption type
    UCHAR_T ssid_hidden;                 ///< ssid hidden, default:0
    UCHAR_T max_conn;                    ///< max sta connect numbers, default:1
    USHORT_T ms_interval;                ///< broadcast interval, default:100
    NW_IP_S ip;                          ///< ip info for ap mode
}WF_AP_CFG_IF_S;
```
+ `WF_AP_CFG_IF_S`'s `ssid` is used to store the AP hotspot name, with a maximum length of 32 (`WIFI_SSID_LEN==32`). `s_len` is used to store the actual length of the hotspot name.

+ `passwd` is used to store the AP hotspot password, with a maximum length of 64 (`WIFI_PASSWD_LEN==64`). `p_len` is used to store the actual length of the hotspot password.

+ `chan` is the channel used by the hotspot, default is 6.

+ `md` is the encryption type used by the hotspot, with the following encryption types available:
    ```c
    typedef enum
    {
        WAAM_OPEN = 0,      ///< open
        WAAM_WEP,           ///< WEP
        WAAM_WPA_PSK,       ///< WPA—PSK
        WAAM_WPA2_PSK,      ///< WPA2—PSK
        WAAM_WPA_WPA2_PSK,  ///< WPA/WPA2
        WAAM_WPA_WPA3_SAE,
        WAAM_UNKNOWN,       //unknown
    }WF_AP_AUTH_MODE_E;
    ```
    `WAAM_OPEN` means no encryption.   
+ `ssid_hidden` indicates whether the hotspot is hidden, 0: not hidden; 1: hidden.

+ `max_conn` is the maximum number of connections supported by the hotspot.

+ `ms_interval` is the interval at which the AP sends Beacon frames, in milliseconds, default 100ms.

+ `ip` is the WiFi information in AP mode, the structure prototype of `NW_IP_S` is as follows:
  
    ```c
    typedef struct {
        char ip[16];    /* ip addr:  xxx.xxx.xxx.xxx  */
        char mask[16];  /* net mask: xxx.xxx.xxx.xxx  */
        char gw[16];    /* gateway:  xxx.xxx.xxx.xxx  */
    }NW_IP_S;
    ```
    `NW_IP_S`'s `ip` is the IP address of the created AP hotspot, `mask` is the network mask, and `gw` represents the gateway address.

## Execution Results

After the `example_wifi_ap` example starts executing, you can use a computer to connect to the development board's created AP hotspot, and then use a network debugging tool to check if UDP broadcast packets can be received.

Below is the log information when `example_wifi_ap` starts executing and the computer connects to the AP hotspot:

```
[01-01 00:04:50 ty N][example_wifi_ap.c:86] ------ wifi ap example start ------
ap net info ip: 192.168.1.123, mask: 255.255.255.0, gw: 192.168.1.1
ssid:my-wifi, key:12345678, channel: 5
[saap]MM_RESET_REQ
[bk]tx_txdesc_flush
[saap]ME_CONFIG_REQ
[saap]ME_CHAN_CONFIG_REQ
[saap]MM_START_REQ
[csa]csa_in_progress[0:0]-clear
hapd_intf_add_vif,type:3, s:0, id:0
apm start with vif:0
me_set_ps_disable:940 0 0 1 0 0
------beacon_int_set:100 TU
set_active param 0
[msg]APM_STOP_CFM
update_ongoing_1_bcn_update
vif_idx:0, ch_idx:0, bcmc_idx:1
update_ongoing_1_bcn_update
netif_is_added: 0x3f6278
netif_is_added: 0x3f6230
net_wlan_add_netif already exist!, vif_idx:0
mac c8:47:8c: 0: 0: 1
net_wlan_add_netif done!, vif_idx:0
uap_ip_start

configuring uap(with IP)def netif is sta
[01-01 00:04:52 ty D][example_wifi_ap.c:124] ip_addr:-1062731397
[01-01 00:04:55 ty D][lr:0x88651] feed watchdog
[01-01 00:04:57 ty D][example_wifi_ap.c:64] send data:hello world
temp_code:59 - adc_code:281 - adc_trend:[19]:290->[20]:280
[01-01 00:05:02 ty D][example_wifi_ap.c:64] send data:hello world
[01-01 00:05:07 ty D][example_wifi_ap.c:64] send data:hello world
[01-01 00:05:12 ty D][example_wifi_ap.c:64] send data:hello world
[01-01 00:05:15 ty D][lr:0x88651] feed watchdog
[01-01 00:05:17 ty D][example_wifi_ap.c:64] send data:hello world
[01-01 00:05:22 ty D][example_wifi_ap.c:64] send data:hello world
[01-01 00:05:27 ty D][example_wifi_ap.c:64] send data:hello world
hapd_intf_sta_add:1, vif:0
rc_init: station_id=0 format_mod=0 pre_type=0 short_gi=0 max_bw=0
rc_init: nss_max=0 mcs_max=255 r_idx_min=0 r_idx_max=11 no_samples=10
WPA: TK 2e13506d30b71ba714cd342aa297384a
sta_mgmt_add_key
ctrl_port_hdl:1
[01-01 00:05:32 ty D][example_wifi_ap.c:64] send data:hello world
```
Below is a screenshot of receiving UDP broadcast data through the network debugging assistant (NetAssist) on the computer:

![udp_recv](https://images.tuyacn.com/fe-static/docs/img/572c01bb-2929-4413-9692-95a4e1779ed6.png)

## Technical Support

You can obtain Tuya's support through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com