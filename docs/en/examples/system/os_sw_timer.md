

# SYSTEM SW TIMER

##  简介

这个项目将会介绍如何使用 `tuyaos 3 system sw timer` 相关接口，创建一个软件定时器，延时时间到5次之后关闭并释放定时器。

软件定时器通过内核控制，它不需要硬件支持，与底层硬件器无关。

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

title OS SW TIMER

start
#pink:入口函数: tuya_app_main;

#palegreen:初始化阶段;
:初始化日志系统;
:初始化定时器框架 \n tal_sw_timer_init;

#skyblue:定时器配置;
:创建定时器对象 \n tal_sw_timer_create;
:启动周期定时器 \n tal_sw_timer_start;

#powderblue:定时事件处理;
while (触发次数 < 5) is (否)
    :定时器回调执行 \n __timer_cb;
    :计数器递增;
endwhile (是)

#palegreen:资源释放;
:停止定时器 \n tal_sw_timer_stop;
:删除定时器 \n tal_sw_timer_delete;

#pink:结束;
end
@enduml

```

## 运行结果
定时器延时时间到5次之后关闭定时器。
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


## 技术支持
您可以通过以下方法获得涂鸦的支持:
* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos)
