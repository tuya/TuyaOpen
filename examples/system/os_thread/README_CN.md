

# SYSTEM THREAD

##  简介

这个项目将会介绍如何使用 `tal` 层相关接口，任务创建，删除任务等。

**这里需要注意的是，使用 `tal_thread_delete` 删除线程时最好是在运行线程内进行删除，不要使用异步的方式进行删除。您可能会发现您调用并执行了 `tal_thread_delete` 函数后，要删除的线程仍在运行，这是因为只有在线程内的任务执行完成后才会真正的去删除您要删除的函数，并不是在您调用 `tal_thread_delete` 函数后立刻就删除。**

## 运行结果
任务执行5次之后删除任务。
```c
[01-01 00:00:24 ty D][tal_thread.c:203] Thread:example_task Exec Start. Set to Running Stat
[01-01 00:00:24 ty N][example_system_thread.c:44] example task is run...
[01-01 00:00:24 ty N][example_system_thread.c:46] this is example task
[01-01 00:00:24 ty I][tal_thread.c:184] thread_create name:example_task,stackDepth:2048,totalstackDepth:27136,priority:3
[01-01 00:00:26 ty N][example_system_thread.c:46] this is example task
[01-01 00:00:28 ty N][example_system_thread.c:46] this is example task
[01-01 00:00:30 ty N][example_system_thread.c:46] this is example task
[01-01 00:00:32 ty N][example_system_thread.c:46] this is example task
[01-01 00:00:36 ty N][example_system_thread.c:56] example task will delete
[01-01 00:00:36 ty D][tal_thread.c:244] Del Thrd:example_task
[01-01 00:00:36 ty D][tal_thread.c:219] Thread:example_task Exec Finish. Set to Del Stat
[01-01 00:00:36 ty D][tal_thread.c:95] del list not empty...deleting 0x42439c, next 0x42fd88
[01-01 00:00:36 ty D][tal_thread.c:103] Final Free Thread:example_task, is_self:1
[01-01 00:00:36 ty D][tal_thread.c:107] delay to delete thread self
[01-01 00:00:36 ty D][tal_thread.c:118] finally delete thread self
[01-01 00:00:36 ty D][tal_thread.c:73] real delete thread:0x0x42e350
```


## 技术支持
您可以通过以下方法获得涂鸦的支持:
* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos)
