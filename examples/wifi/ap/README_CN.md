

# WiFi AP

##  简介

当调用 `example_wifi_ap` 示例时，会先创建一个 AP 热点，默认热点名称（SSID）为 "my-wifi"，热点密码为"12345678"。热点创建成功后会进行 UDP 广播。

## 流程介绍

`example_wifi_ap` 示例的流程为：**WiFi 初始化 -> WiFi 模式设置为 AP 模式 -> 启动 AP -> 每 5s 进行一次 UDP 广播**。

在 `example_wifi_ap` 示例中最为重要的代码片段便是： `WF_AP_CFG_IF_S` 结构体了，下面便是对于 `WF_AP_CFG_IF_S` 结构体的一个详细介绍：

```c
typedef struct {
    UCHAR_T ssid[WIFI_SSID_LEN+1];       ///< ssid
    UCHAR_T s_len;                       ///< len of ssid
    UCHAR_T passwd[WIFI_PASSWD_LEN+1];   ///< passwd
    UCHAR_T p_len;                       ///< len of passwd
    UCHAR_T chan;                        ///< channel. default:6
    WF_AP_AUTH_MODE_E md;                ///< encryption type
    UCHAR_T ssid_hidden;                 ///< ssid hidden  default:0
    UCHAR_T max_conn;                    ///< max sta connect nums default:1
    USHORT_T ms_interval;                ///< broadcast interval default:100
    NW_IP_S ip;                          ///< ip info for ap mode
}WF_AP_CFG_IF_S; 
```

+ `WF_AP_CFG_IF_S` 中的 `ssid` 是用来存储 AP 热点名称的，最大长度为 32 （WIFI_SSID_LEN==32）。`s_len` 则是用来存放热点名称的实际长度。

+ `passwd` 是用来存储 AP 热点密码的，最大长度为 64 （WIFI_SSID_LEN==64）。`p_len` 则是用来存放热点密码的实际长度。

+ `chan` 热点使用的信道，默认为 6。

+ `md` 热点使用的加密类型，有下列几种加密类型：

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

    其中 `WAAM_OPEN`  表示不加密。

+ `ssid_hidden` 是否隐藏该热点，0：不隐藏；1：隐藏。

+ `max_conn` 热点支持最大连接数。

+ `ms_interval` AP 发送 Becaon 帧的间隔时间，单位：ms，默认 100ms。

+ `ip` ap 模式的 WiFi 信息，`NW_IP_S` 的结构体原型如下：

    ```c
    typedef struct {
        char ip[16];    /* ip addr:  xxx.xxx.xxx.xxx  */
        char mask[16];  /* net mask: xxx.xxx.xxx.xxx  */
        char gw[16];    /* gateway:  xxx.xxx.xxx.xxx  */
    }NW_IP_S;
    ```

    `NW_IP_S` 中的 `ip` 为创建的 AP 热点的 IP 地址，`mask` 是网络掩码，`gw` 表示网关地址。

## 运行结果

当 `example_wifi_ap` 示例开始执行后，可以使用电脑连接开发板创建 AP 热点，然后通过网络调试工具查看是否可以接收到 UDP 广播包。

下面是开始执行 `example_wifi_ap` ，电脑连接到 AP 热点的日志信息：

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

下图为通过电脑上的网络调试助手（NetAssist）接收到 UDP 广播数据的截图：

![udp_recv](https://images.tuyacn.com/fe-static/docs/img/572c01bb-2929-4413-9692-95a4e1779ed6.png)

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
