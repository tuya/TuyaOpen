# JOYSTICK

## Introduction

This example demonstrates how to use the joystick driver component of TuyaOpen to implement the following features:

* Joystick direction detection (Up/Down/Left/Right)
* Button event handling (Press/Long press)
* Joystick long press detection (Long Up/Long Down/Long Left/Long Right)
* ADC analog signal acquisition and processing

## Process Introduction
![joystick-flowchart](https://images.tuyacn.com/fe-static/docs/img/112d5b26-be73-4178-b828-a8c74b6633d1.png)

## Device Connection
![joystick](https://images.tuyacn.com/fe-static/docs/img/6627c749-40d3-4eb6-a26b-d9d34a7567b0.png)

## Operation Results

```c
/* Joystick static */
[01-01 00:00:01 ty D][example_joystick.c:178] raw  ADC0 value_X = 4148, value_Y = 4070
[01-01 00:00:01 ty D][example_joystick.c:180] cali ADC0 value_X = 0, value_Y = 0
```
```c
/* Joystick movement */
[01-01 00:02:59 ty D][example_joystick.c:178] raw  ADC0 value_X = 2484, value_Y = 3658
[01-01 00:02:59 ty D][example_joystick.c:180] cali ADC0 value_X = 1, value_Y = 3
```
```c
/* Short press and long press button events */
[01-01 00:03:38 ty N][example_joystick.c:55] app_joystick: press down
[01-01 00:03:42 ty N][example_joystick.c:59] app_joystick: press long hold
```
```c
/* Direction events */
[01-01 00:05:10 ty N][example_joystick.c:63] app_joystick: up
[01-01 00:05:10 ty N][example_joystick.c:79] app_joystick: long up
[01-01 00:05:11 ty N][example_joystick.c:75] app_joystick: down
[01-01 00:05:11 ty N][example_joystick.c:83] app_joystick: long down
[01-01 00:05:12 ty N][example_joystick.c:67] app_joystick: left
[01-01 00:05:12 ty N][example_joystick.c:87] app_joystick: long left
[01-01 00:05:13 ty N][example_joystick.c:71] app_joystick: right
[01-01 00:05:13 ty N][example_joystick.c:91] app_joystick: long right
```

> Note: Direction events will only trigger once, while long press events will trigger continuously

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com

