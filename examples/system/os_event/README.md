# TuyaOS event subscription

## Introduction

In TuyaOS, there are some common events available for subscription (you can view the subscribable events in the `base_event_info.h` file). The `example_os_event.c` example demonstrates how to subscribe to events.

## Example Introduction

The event subscription interface prototype is as follows:

```c
OPERATE_RET ty_subscribe_event(const char *name, const char *desc, const EVENT_SUBSCRIBE_CB cb, SUBSCRIBE_TYPE_E type);
```
+ `name` : The name of the event, which needs to be selected from the macro definitions in the  `base_event_info.h ` file.

+ `desc`: A custom description for the subscribed event. The same event may be subscribed to in multiple places, and when unsubscribing, the desc field is needed to cancel the subscription.

+ `cb`: Subscription callback function.

+ `type`: Subscription type, the selectable types are as follows:
   ```c
  typedef uint8_t SUBSCRIBE_TYPE_E;
  #define SUBSCRIBE_TYPE_NORMAL    0  // normal type, dispatch by the subscribe order, remove when unsubscribe
  #define SUBSCRIBE_TYPE_EMERGENCY 1  // emergency type, dispatch first, remove when unsubscribe
  #define SUBSCRIBE_TYPE_ONETIME   2  // one time type, dispatch by the subscribe order, remove after first time dispath
  ```
Common subscription events are as follows:

```
#define EVENT_SDK_EARLY_INIT_OK "early.init"        // before kv flash init
#define EVENT_SDK_DB_INIT_OK    "db.init"           // before db init
#define EVENT_TIME_SYNC         "time.sync"         // time is sync
```
+ `EVENT_SDK_EARLY_INIT_OK`: This event is sent before the KV initialization. Since the initialization of KV Flash takes a long time, if some tasks need to be executed as soon as possible but do not need the KV functionality, they can be handled by subscribing to the `EVENT_SDK_EARLY_INIT_OK` event.
+ `EVENT_SDK_DB_INIT_OK`: This event is sent after the KV initialization is completed. If you need to execute quickly and use the KV functionality, you can subscribe to the `EVENT_SDK_DB_INIT_OK event`.
+ `EVENT_TIME_SYNC`: Time synchronization event.
  
## Execution Results

The KV initialization in the example is performed in `tuya_iot_init_params()`. You can [click to read](https://developer.tuya.com/en/docs/iot-device-dev/TuyaOS-iot_abi_device_init?id=Kc67dkj0mxrne#title-8-System%20Service%20Initialization%20Parameters) more about KV information.

The execution log of the `example_os_event.c` example is as follows:
```
[MEM DBG] heap init-------size:167472 addr:0x4071d0---------
[Flash]id:0xeb6015
[load]bandgap_calm=0x3e->0x3e,vddig=4->4
IP Rev: 5b41926
[bk]tx_txdesc_flush
tkl_ethernetif_init
tkl_ethernetif_init
tkl_ethernetif_output: invalid vif: 255!
tkl_ethernetif_output: invalid vif: 255!
[01-01 00:00:00 ty D][tal_thread.c:203] Thread:sys_timer Exec Start. Set to Running Stat
[01-01 00:00:00 ty I][tal_thread.c:184] thread_create name:sys_timer,stackDepth:4096,totalstackDepth:10240,priority:5
[01-01 00:00:00 ty I][tal_thread.c:184] thread_create name:wq_system,stackDepth:5120,totalstackDepth:15360,priority:3
[01-01 00:00:00 ty I][tal_thread.c:184] thread_create name:wq_highpri,stackDepth:4096,totalstackDepth:19456,priority:4
[01-01 00:00:00 ty D][example_os_event.c:45] ---> db init start
[01-01 00:00:00 ty D][tal_thread.c:203] Thread:wq_highpri Exec Start. Set to Running Stat
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:0
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:1
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:2
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:3
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:4
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:5
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:6
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:7
[01-01 00:00:00 ty D][lr:0x8880d] add new node,type:8
[01-01 00:00:00 ty D][lr:0x88957] init watchdog, interval: 60
[01-01 00:00:00 ty D][lr:0x8887f] update type:7,period:20
[01-01 00:00:00 ty D][lr:0x88985] watch_dog_interval:60, monitor_detect_interval:600
[01-01 00:00:00 ty D][tal_thread.c:203] Thread:health_monitor Exec Start. Set to Running Stat
[01-01 00:00:00 ty I][tal_thread.c:184] thread_create name:health_monitor,stackDepth:2048,totalstackDepth:21504,priority:5
[01-01 00:00:00 ty I][lr:0x7a5bb] mqc app init ...
[01-01 00:00:00 ty D][lr:0x7a4af] mq_pro:5 cnt:1
[01-01 00:00:00 ty D][lr:0x7a4af] mq_pro:31 cnt:2
[01-01 00:00:00 ty D][lr:0x7f1df] svc online log init success
[01-01 00:00:00 ty E][lr:0x7d6b5] logseq empty
[01-01 00:00:00 ty D][lr:0x76833] init fs. Path: null 
[01-01 00:00:00 ty D][lr:0x769dd] *****************kvs_init. 
[01-01 00:00:00 ty D][lr:0xd167d] protected init. addr:0x001ed000
[01-01 00:00:00 ty D][lr:0xd14ef] init protected data length 460
[01-01 00:00:00 ty N][lr:0xb94d7] key_addr: 0x1ee000   block_sz 4096
[01-01 00:00:00 ty N][lr:0xb95a5] get key:
0xcf 0xd5 0x9b 0xe7 0x5e 0x5f 0xe5 0xb9 0x65 0x54 0xf0 0xcc 0x21 0x90 0xb1 0x21 
[01-01 00:00:00 ty D][lr:0xd1699] protected verify begin
[01-01 00:00:00 ty D][lr:0xd16cf] check [gw_bi][258]
[01-01 00:00:00 ty D][lr:0xd16cf] check [gw_wsm][130]
[01-01 00:00:00 ty D][lr:0xd170b] protected verify end
[01-01 00:00:00 ty D][lr:0xb8689] begin try update kv version
[01-01 00:00:00 ty D][lr:0xb869f] pre kv version is 2
[01-01 00:00:00 ty D][lr:0xb9e37] 111 k=0  i=2 3
[01-01 00:00:00 ty D][example_os_event.c:51] ---> db init finish
```
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com