

# JOYSTICK

## 简介

本示例演示如何使用 TuyaOpen 的摇杆驱动组件，实现以下功能：

* 摇杆方向检测（上/下/左/右）
* 按钮事件处理（按下/长按）
* 摇杆长按检测（长上/长下/长左/长右）
* ADC模拟量采集与处理

## 流程介绍

![joystick-flowchart](https://images.tuyacn.com/fe-static/docs/img/3798c7bc-e2a1-4bd8-ac19-9f656c59538f.png)

## 设备连接
![joystick](https://images.tuyacn.com/fe-static/docs/img/6627c749-40d3-4eb6-a26b-d9d34a7567b0.png)

## 运行结果

```c
/* 摇杆静止 */
[01-01 00:00:01 ty D][example_joystick.c:178] raw  ADC0 value_X = 4148, value_Y = 4070
[01-01 00:00:01 ty D][example_joystick.c:180] cali ADC0 value_X = 0, value_Y = 0

/* 摇杆移动 */
[01-01 00:02:59 ty D][example_joystick.c:178] raw  ADC0 value_X = 2484, value_Y = 3658
[01-01 00:02:59 ty D][example_joystick.c:180] cali ADC0 value_X = 1, value_Y = 3
```

```c
/* 短按、长按按键事件 */
[01-01 00:03:38 ty N][example_joystick.c:55] app_joystick: press down
[01-01 00:03:42 ty N][example_joystick.c:59] app_joystick: press long hold
```

```c
/* 摇杆方向事件 */
[01-01 00:05:10 ty N][example_joystick.c:63] app_joystick: up
[01-01 00:05:10 ty N][example_joystick.c:79] app_joystick: long up
[01-01 00:05:11 ty N][example_joystick.c:75] app_joystick: down
[01-01 00:05:11 ty N][example_joystick.c:83] app_joystick: long down
[01-01 00:05:12 ty N][example_joystick.c:67] app_joystick: left
[01-01 00:05:12 ty N][example_joystick.c:87] app_joystick: long left
[01-01 00:05:13 ty N][example_joystick.c:71] app_joystick: right
[01-01 00:05:13 ty N][example_joystick.c:91] app_joystick: long right
```

> 注意：摇杆方向事件只会触发一次，摇杆长按事件会持续触发

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
