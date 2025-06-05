# SYSTEM SW TIMER

## Introduction

This project will introduce how to use the `tuyaos 3 system sw timer` related interfaces to create a software timer, which will be closed and released after a delay of 5 times.

The software timer is controlled by the kernel, it does not require hardware support, and is independent of the underlying hardware.

## Execution Results
The timer is closed after a delay of 5 times.
```c
[01-01 00:00:07 ty D][example_sw_timer.c:59] sw timer start
[01-01 00:00:10 ty N][example_sw_timer.c:43] --- tal sw timer callback
[01-01 00:00:13 ty N][example_sw_timer.c:43] --- tal sw timer callback
[01-01 00:00:15 ty D][lr:0x8a455] feed watchdog
[01-01 00:00:16 ty N][example_sw_timer.c:43] --- tal sw timer callback
[01-01 00:00:19 ty N][example_sw_timer.c:43] --- tal sw timer callback
[01-01 00:00:22 ty N][example_sw_timer.c:43] --- tal sw timer callback
[01-01 00:00:22 ty N][example_sw_timer.c:46] stop and delete software timer
```
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com