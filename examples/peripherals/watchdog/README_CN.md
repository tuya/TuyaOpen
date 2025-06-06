

# WATCHDOG

##  简介

这个项目将会介绍如何使用 `tuyaos 3 watchdog ` 相关接口，开启看门狗。

* 看门狗简介

看门狗（ `watchdog` ），可以理解为一个有计数功能的定时器。一般有一个输入叫喂狗，一个输出连接着芯片的复位口。如果芯片工作正常，每隔一段时间输出一个信号给喂狗口，看门狗的计数清零。如果芯片跑飞或其他原因导致超过规定时间没有喂狗，那么看门狗就会输出一个信号给复位口，复位芯片。

![watdog 12 138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655736855a38b645e4a9.png)

## 流程介绍

![watdog process .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16557369822cb8027f90d.png)

## 运行结果
由于我们设置的喂狗时间是10s，但是此例程的硬件环境是 `BK7231N`，它是每60s秒喂一次狗，所以芯片会一直重启。

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
