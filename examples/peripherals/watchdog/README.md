# WATCHDOG

## Introduction

This project will introduce how to use the `tuyaos 3 watchdog` related interfaces to enable the watchdog.

* Watchdog Introduction

A watchdog (`watchdog`) can be understood as a timer with counting capability. Typically, it has an input called "feeding the dog" and an output connected to the chip's reset port. If the chip works normally, it outputs a signal to the "feeding the dog" input at regular intervals, resetting the watchdog's count to zero. If the chip malfunctions or for other reasons fails to "feed the dog" within the specified time, the watchdog will output a signal to the reset port, resetting the chip.

![watdog 12 138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655736855a38b645e4a9.png)

## Process Introduction

![watdog process .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16557369822cb8027f90d.png)

## Execution Results
The example requests a 60-second watchdog timeout and refreshes the watchdog before the actual timeout expires. After the configured number of refreshes, it stops refreshing so the watchdog resets the device.

## Technical Support

You can obtain Tuya's support through the following methods:

- TuyaOS Forum: https://www.tuyaos.com
- Developer Center: https://developer.tuya.com
- Help Center: https://support.tuya.com/help
- Technical Support Ticket Center: https://service.console.tuya.com