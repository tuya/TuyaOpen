# PWM

## 简介

这个项目将会介绍如何使用 `tuyaos 3 pwm ` 相关接口，设置 `pwm` 波形。

* `PWM` 简介

脉冲宽度调制( `PWM` )，是英文`“Pulse Width Modulation”`的缩写。是通过将有效的电信号分散成离散形式从而来降低电信号所传递的平均功率的一种方式。

* 占空比、频率、周期

周期（T）：T = Ton + Toff

频率（f）：f = 1/T

占空比（D）：Ton/（Ton+Toff）

![pwm jiehao 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165596468101cf5b911aa.png)

## 流程介绍

```plantuml
@startuml
skinparam ActivityFontColor #black
skinparam ActivityBackgroundColor #white
skinparam ActivityArrowColor #black
skinparam ActivityFontName "Microsoft YaHei"
skinparam TitleFontName "Microsoft YaHei"
skinparam defaultFontName "Microsoft YaHei"
skinparam defaultFontSize 14
skinparam titleFontSize 20

title PWM
start

#pink:入口函数：tuya_app_main;
#palegreen:pwm初始化：tkl_pwm_init;
#palegreen:repeat:修改占空比：tkl_pwm_duty_set;
    #palegreen:修改PWM频率:tkl_pwm_frequency_set;
    #palegreen:开启PWM：tkl_pwm_start;
repeat while(<color : red>任务执行5次?) is (yes) not (no);
#palegreen:关闭PWM：tkl_pwm_stop;
#pink:结束;
@enduml
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
