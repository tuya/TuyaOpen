# SYSTEM SEMAPHORE

## Introduction

This example creates two threads at the beginning, `__sema_post_task` and `__sema_wait_task`. The `__sema_post_task` thread releases a semaphore every 5 seconds, while the `__sema_wait_task` thread is always blocked, waiting for the semaphore to arrive.

## Execution Results

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
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com