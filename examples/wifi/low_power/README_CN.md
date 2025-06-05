

# WIFI LOW POWER 

##  简介

这个项目将会介绍如何使用 `tuyaos 3 wifi low power ` 相关接口，开启 `wifi` 低功耗。

* `wifi` 低功耗

射频进入睡眠模式，随着设置的 `DTIM` 定时唤醒，以此来降低 `wifi` `功耗。cpu` 也会同步进入低功耗。

* `DTIM`
  

`DTIM` ( `Delivery Traffic Indication Message` )常用于无线的低功耗模式中。通过设置的 `DTIM` 间隔（默认是1个 `beacon` ），根据这个间隔，定时打开射频发送数据。`DTIM` 为1的时候，表示每个 `beacon` 中都包含 `DTIM` ，`DTIM` 为2的时候，表示每两个 `beacon` 中包含一个 `DTIM` 。所以想要更低的功耗，那就适当增大 `DTIM` 的值。


* `tuyaos` 低功耗管理模式

`tuyaos` 提供了低功耗的相关接口，并且有一套自己的低功耗管理模式。如果低功耗模式使用不当，则会造成低功耗管理混乱，你明明退出了低功耗，但却还在低功耗模式中。


`tuyaos` 提供了3个重要的 `wifi` 低功耗接口。具体说明如下图所示：


![wifi sleep three api.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165603518309369abc33d.png)

开启 `wifi` 的低功耗会同步开启 `cpu` 的低功耗，所以使用 `wifi` 低功耗之前，先调用 `tuya_cpu_set_lp_mode` 这个函数。因为 `cpu` 和 `wifi` 的相关低功耗函数都是基于 `tuya_cpu_set_lp_mode` 这个函数做逻辑的。

`tuyaos` 内部有一个全局变量 `wifi_disable_count` 用来管理 `wifi` 低功耗。调用 `tal_wifi_lp_disable`，会同步调用 `tal_cpu_lp_disable` ，同时 ` wifi_disable_count` 这个变量会 ++；调用 `tal_wifi_lp_enable` ，会同步调用 `tal_cpu_lp_enable`，` wifi_disable_count `这个变量会 --。

`wifi_disable_count` 大于0的时候，会调用相关接口退出 `wifi` 低功耗，同时也会退出 `cpu` 低功耗。

 `wifi_disable_count` 等于0的时候，会调用相关接口使能 `wifi` 低功耗，同时也会使能 `cpu` 低功耗。


![wifi low power 3   12138 .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165603413688bf3652a37.png)

## 流程介绍

![wifi low power process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1656034364902e5e3efaf.png)

## 运行结果

`wifi` 退出低功耗模式之后，会同步退出 `cpu` 低功耗模式。
```C
*******************************tuya_os_adapt_set_cpu_lp_mode,en = 0, mode = 0
bk_wlan_mcu_ps_mode_disable()
```

`wifi` 进入低功耗模式之后，会同步进入 `cpu` 低功耗模式。

```C
*******************************tuya_os_adapt_set_cpu_lp_mode,en = 1, 
bk_wlan_mcu_ps_mode_enable()
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
