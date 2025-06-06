# TIMER

## Introduction

This project will introduce how to use the `tuyaos 3 timer` related interfaces to create a timer, which will be closed and released after a delay of 5 times.

## Process Introduction

![timer process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655966813530a00a4c0c.png)

## Execution Results

After the timer interrupt is triggered 5 times, the timer is stopped.

```c
[01-01 00:00:08 TUYA N][lr:0x70ab9] timer 0 is start
[01-01 00:00:08 TUYA D][lr:0x70ad5] -------------Timer Callback--------------
[01-01 00:00:09 TUYA D][lr:0x70ad5] -------------Timer Callback--------------
[01-01 00:00:09 TUYA D][lr:0x70ad5] -------------Timer Callback--------------
[01-01 00:00:10 TUYA D][lr:0x70ad5] -------------Timer Callback--------------
[01-01 00:00:10 TUYA D][lr:0x70ad5] -------------Timer Callback--------------
[01-01 00:00:10 TUYA N][lr:0x70af7] timer 0 is stop
```

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com