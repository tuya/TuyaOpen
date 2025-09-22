# SYSTEM MUTEX

## Introduction

This example creates two threads, `__mutex_task1` and `__mutex_task2`, both of which perform read and write operations on the `temp_value` variable.

* Mutex

In `RTOS`, a mutex is essentially a binary semaphore with priority inheritance.

Each thread should lock before accessing a shared resource. After locking, other threads trying to access this shared resource will enter a blocked state.

Locking and unlocking of a mutex come in pairs; a thread must unlock promptly after it finishes accessing the shared resource.

If multiple threads access a shared resource without using a mutex, time-related errors can occur, leading to data inconsistency.

## Execution Results
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
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com