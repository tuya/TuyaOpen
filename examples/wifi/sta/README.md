# WIFI STATION

## Introduction

This project will introduce how to use the `tuyaos 3 wifi station` related interfaces to enable `STA` mode and connect to the `AP` opened by a laptop.

![wifi sta and ap.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655624853b5f412495be.png)

* Introduction to `WiFi STATION`

`STATION` site, in a wireless LAN, is generally a client. For example, a mobile phone is a site, and the `WiFi` network it connects to is the `AP`.

* Introduction to `STATION` mode parameter configuration

|Parameter|Introduction|
|-|-|
|ssid|The `AP` account to connect to.|
|passwd|The `AP` password to connect to.|

## Process Introduction

![wifi station process draw.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556268007be208b1d90.png)

## Execution Results
Get the current site mode status.
```c
[01-01 00:00:00 TUYA D][lr:0x4b74d] station_stat:0
```
The computer opens the hotspot, successfully connects to the computer hotspot.
```c
[01-01 00:00:01 TUYA D][lr:0x4b6b3] -------------event callback-------------
[01-01 00:00:01 TUYA D][lr:0x4b6cd] connection succeeded!
```
The computer closes the hotspot, fails to connect to the computer hotspot.
```c
[01-01 00:02:50 TUYA D][lr:0x4b6b3] -------------event callback-------------
[01-01 00:02:50 TUYA D][lr:0x4b6cd] connection fail!
```
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com