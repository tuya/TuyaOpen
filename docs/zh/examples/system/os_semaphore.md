
# SYSTEM SEMAPHORE

##  简介

该示例会在开始的时候创建两个线程， `__sema_post_task` 和 `__sema_wait_task` ，`__sema_post_task` 线程会每隔 5s 就释放一个信号量，`__sema_wait_task` 线程一直阻塞着等待着信号量的到来。

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
title OS　SEMAPHORE

start
#pink:入口函数: tuya_app_main;

#palegreen:初始化阶段;
:初始化日志系统;
:创建信号量 \n tal_semaphore_create_init;

fork
    #skyblue:生产者线程;
    :释放信号量 \n tal_semaphore_post;
    :休眠2秒;
    -> 循环执行;

fork again
    #powderblue:消费者线程;
    :等待信号量 \n tal_semaphore_wait;
    :处理信号事件;
    -> 持续等待;
end fork

#palegreen:资源释放;
:删除线程;
:销毁信号量;

#pink:结束;
end
@enduml


```

## 运行结果

```c
[01-01 00:11:05 ty D][tal_thread.c:203] Thread:sema_post Exec Start. Set to Running Stat
[01-01 00:11:05 ty N][example_semaphore.c:48] post semaphore
[01-01 00:11:05 ty I][tal_thread.c:184] thread_create name:sema_post,stackDepth:1024,totalstackDepth:26112,priority:3
[01-01 00:11:05 ty D][tal_thread.c:203] Thread:sema_wait Exec Start. Set to Running Stat
[01-01 00:11:05 ty N][example_semaphore.c:59] get semaphore
[01-01 00:11:05 ty I][tal_thread.c:184] thread_create name:sema_wait,stackDepth:1024,totalstackDepth:27136,priority:3
[01-01 00:11:10 ty N][example_semaphore.c:48] post semaphore
[01-01 00:11:10 ty N][example_semaphore.c:59] get semaphore
[01-01 00:11:15 ty N][example_semaphore.c:48] post semaphore
[01-01 00:11:15 ty N][example_semaphore.c:59] get semaphore
```

## 技术支持
您可以通过以下方法获得涂鸦的支持:
* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos)
