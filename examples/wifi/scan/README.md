# WIFI SCAN

## Introduction

This project will introduce how to use the `tuyaos 3 wifi scan` related interfaces to scan the current `WiFi` environment and print the `WiFi` channel numbers and usernames.

* Common `WiFi` Information Parameters

|`WiFi` Information|Description|
|---|---|
|channel|Channel number|
|rssi|Signal strength|
|bssid|MAC address|
|ssid|WiFi name|
|s_len|Length of the WiFi name|
|security|Environment type|

## Process Introduction

![wifi scanf process draw.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556274379f941cc7b92.png)

## Execution Results
Scanned 46 `WiFi` signals.
```c
[01-01 00:00:02 TUYA D][lr:0x4b6cb] Scanf to 46 wifi signals
```
Display some of the WiFi channel numbers and usernames.
```c
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:13, ssid:6F-S-15-04-2.4G
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:13, ssid:6F-S-14-14
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:1, ssid:Aatrox
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:11, ssid:6F-S-15-02
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:13, ssid:6F-S-15-03
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:6, ssid:6F-S-14-08
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:6, ssid:Tuya-Test
[01-01 00:00:02 TUYA D][lr:0x4b6ed] channel:8, ssid:6F-S-14-03
......
```

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com
