# WIFI LOW POWER

## Introduction

This project will introduce how to use the `tuyaos 3 wifi low power` related interfaces to enable `wifi` low power mode.

* `wifi` low power

The radio frequency enters sleep mode and wakes up at the set `DTIM` interval to reduce the `wifi` power consumption. The `cpu` will also enter low power mode synchronously.

* `DTIM`

`DTIM` (Delivery Traffic Indication Message) is commonly used in wireless low power modes. By setting the `DTIM` interval (default is 1 `beacon`), the radio frequency is turned on periodically to send data. When `DTIM` is 1, it means that every `beacon` contains a `DTIM`; when `DTIM` is 2, it means that every two `beacons` contain one `DTIM`. Therefore, to achieve lower power consumption, the value of `DTIM` should be appropriately increased.

* `tuyaos` low power management mode

`tuyaos` provides related interfaces for low power mode and has its own low power management mode. If the low power mode is used improperly, it will cause confusion in low power management. You might have exited low power mode, but the system is still in low power mode.

`tuyaos` provides three important `wifi` low power interfaces. The specifics are shown in the following figure:
![wifi sleep three api.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165603518309369abc33d.png)

Enabling `wifi` low power will also enable `cpu` low power, so before using `wifi` low power, call the `tuya_cpu_set_lp_mode` function first. Because the related low power functions of `cpu` and `wifi` are based on the `tuya_cpu_set_lp_mode` function.

`tuyaos` has a global variable `wifi_disable_count` to manage `wifi` low power. Calling `tal_wifi_lp_disable` will synchronously call `tal_cpu_lp_disable`, and the `wifi_disable_count` variable will increment; calling `tal_wifi_lp_enable` will synchronously call `tal_cpu_lp_enable`, and the `wifi_disable_count` variable will decrement.

When `wifi_disable_count` is greater than 0, it will call the relevant interface to exit `wifi` low power mode, and also exit `cpu` low power mode.

When `wifi_disable_count` is equal to 0, it will call the relevant interface to enable `wifi` low power mode, and also enable `cpu` low power mode.

![wifi low power 3   12138 .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165603413688bf3652a37.png)

## Process Introduction

![wifi low power process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1656034364902e5e3efaf.png)

## Execution Results

After exiting `wifi` low power mode, it will synchronously exit `cpu` low power mode.
```C
*******************************tuya_os_adapt_set_cpu_lp_mode,en = 0, mode = 0
bk_wlan_mcu_ps_mode_disable()
```
After entering wifi low power mode, it will synchronously enter cpu low power mode.

```C
*******************************tuya_os_adapt_set_cpu_lp_mode,en = 1, 
bk_wlan_mcu_ps_mode_enable()
```

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com