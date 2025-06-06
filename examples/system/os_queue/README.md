# SYSTEM QUEUE

## Introduction

This example creates two threads at the start, `__queue_post_task` and `__queue_fetch_task`. The `__queue_post_task` thread releases a piece of data into the queue every 5 seconds, while the `__queue_fetch_task` thread is always blocked waiting for data to arrive in the queue, then prints the data retrieved from the queue.

## Execution Results
```C
[01-01 00:00:13 ty D][tal_thread.c:203] Thread:queue_post Exec Start. Set to Running Stat
[01-01 00:00:13 ty N][example_queue.c:51] post queue
[01-01 00:00:13 ty I][tal_thread.c:184] thread_create name:queue_post,stackDepth:1024,totalstackDepth:26112,priority:3
[01-01 00:00:13 ty D][tal_thread.c:203] Thread:queue_fetch Exec Start. Set to Running Stat
[01-01 00:00:13 ty N][example_queue.c:65] get queue, data: hello tuya

[01-01 00:00:13 ty I][tal_thread.c:184] thread_create name:queue_fetch,stackDepth:1024,totalstackDepth:27136,priority:3
[01-01 00:00:15 ty D][lr:0x8a455] feed watchdog
[01-01 00:00:18 ty N][example_queue.c:51] post queue
[01-01 00:00:18 ty N][example_queue.c:65] get queue, data: hello tuya

[01-01 00:00:23 ty N][example_queue.c:51] post queue
[01-01 00:00:23 ty N][example_queue.c:65] get queue, data: hello tuya
```
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com