# SYSTEM THREAD

## Introduction

This project will introduce how to use the `tal` layer related interfaces for task creation, task deletion, etc.

**It is important to note that when using `tal_thread_delete` to delete a thread, it is best to do so within the running thread itself, rather than using an asynchronous method to delete it. You may find that after calling and executing the `tal_thread_delete` function, the thread you want to delete is still running. This is because the function you want to delete will only be truly deleted after the task within the thread is completed, not immediately after you call the `tal_thread_delete` function.**

## Execution Results
The task is deleted after being executed 5 times.
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
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com