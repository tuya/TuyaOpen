

# SYSTEM MUTEX

##  简介

该示例会创建两个线程 `__mutex_task1` 和 `__mutex_task2` ，这两个线程都会对 `temp_value` 变量进行读写操作。

* 互斥量

 `RTOS` 下的互斥量实际上就是一个带有优先级继承的二值信号量。

 每个线程访问共享资源前都应该上锁，上锁之后，其他线程在访问这个共享资源，就会进入阻塞态。

 互斥量的上锁和解锁是成对出现的，线程在结束对共享资源的访问之后要及时解锁。

如果多个线程访问共享资源不使用互斥量，则会发生与时间有关的错误，造成数据不一致。


## 运行结果

```c
[01-01 00:00:12 ty D][tal_thread.c:203] Thread:example_mutex1 Exec Start. Set to Running Stat
[01-01 00:00:12 ty N][example_mutex.c:51] task1 temp_value: 2
[01-01 00:00:12 ty I][tal_thread.c:184] thread_create name:example_mutex1,stackDepth:1024,totalstackDepth:26112,priority:3
[01-01 00:00:12 ty D][tal_thread.c:203] Thread:example_mutex2 Exec Start. Set to Running Stat
[01-01 00:00:12 ty N][example_mutex.c:64] task2 temp_value: 1
[01-01 00:00:12 ty I][tal_thread.c:184] thread_create name:example_mutex2,stackDepth:1024,totalstackDepth:27136,priority:3
[01-01 00:00:15 ty D][lr:0x8a455] feed watchdog
[01-01 00:00:17 ty N][example_mutex.c:51] task1 temp_value: 3
[01-01 00:00:17 ty N][example_mutex.c:64] task2 temp_value: 2
[01-01 00:00:22 ty N][example_mutex.c:51] task1 temp_value: 4
[01-01 00:00:22 ty N][example_mutex.c:64] task2 temp_value: 3
[01-01 00:00:27 ty N][example_mutex.c:51] task1 temp_value: 5
[01-01 00:00:27 ty N][example_mutex.c:64] task2 temp_value: 4
[01-01 00:00:32 ty N][example_mutex.c:51] task1 temp_value: 6
[01-01 00:00:32 ty N][example_mutex.c:64] task2 temp_value: 5
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:
* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos)
