

# WIFI STATION

##  简介

这个项目将会介绍如何使用 `tuyaos 3 wifi station ` 相关接口，开启 `STA` 模式，并连接笔记本电脑开的 `AP`。

![wifi sta and ap.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655624853b5f412495be.png)


* `WiFi STATION` 相关介绍

`STATION` 站点，在无线局域网中一般为客户端。例如手机就是一个站点，手机连接的 `WiFi` 网络就是 `AP`。

* `STATION` 模式参数配置介绍
  
|参数|介绍|
|-|-|
|ssid|连接的 `AP` 账号。|
|passwd|连接的 `AP` 密码。|

## 流程介绍

![wifi station process draw.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556268007be208b1d90.png)

## 运行结果
获取当前站点模式状态。
```c
[01-01 00:00:00 TUYA D][lr:0x4b74d] station_stat:0
```
电脑打开热点，成功连接电脑热点。
```c
[01-01 00:00:01 TUYA D][lr:0x4b6b3] -------------event callback-------------
[01-01 00:00:01 TUYA D][lr:0x4b6cd] connection succeeded!
```

电脑关闭热点，连接电脑热点失败。
```c
[01-01 00:02:50 TUYA D][lr:0x4b6b3] -------------event callback-------------
[01-01 00:02:50 TUYA D][lr:0x4b6cd] connection fail!
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
