# TuyaOS event subscribe

##  简介

在 TuyaOS 存在一些常用的事件可供订阅（可以在 `base_event_info.h` 文件查看可以订阅的事件），`example_os_event.c` 示例演示了如何对进行事件订阅。

## 示例介绍

事件订阅接口原型如下：

```c
OPERATE_RET ty_subscribe_event(const char *name, const char *desc, const EVENT_SUBSCRIBE_CB cb, SUBSCRIBE_TYPE_E type);
```

+ `name` ：为事件的名称，需要在 `base_event_info.h` 文件中的宏定义中进行选择。

+ `desc`：对于订阅事件的自定义描述。同一个事件可能会在多个地方进行订阅，当取消订阅时就需要配合`desc` 字段进行订阅取消。

+ `cb`：订阅回调函数。

+ `type`：订阅类型，可以选择的类型如下：

  ```c
  typedef uint8_t SUBSCRIBE_TYPE_E;
  #define SUBSCRIBE_TYPE_NORMAL    0  // normal type, dispatch by the subscribe order, remove when unsubscribe
  #define SUBSCRIBE_TYPE_EMERGENCY 1  // emergency type, dispatch first, remove when unsubscribe
  #define SUBSCRIBE_TYPE_ONETIME   2  // one time type, dispatch by the subscribe order, remove after first time dispath
  ```

常用的订阅事件如下：

```
#define EVENT_SDK_EARLY_INIT_OK "early.init"        // before kv flash init
#define EVENT_SDK_DB_INIT_OK    "db.init"           // before db init
#define EVENT_TIME_SYNC         "time.sync"         // time is sync
```

+ `EVENT_SDK_EARLY_INIT_OK`：在进行 KV 初始化前会发送该事件。KV Flash 初始化需要的时间较长，当有些任务需要尽早执行但又用不到 KV 功能，就可以可以通过订阅 `EVENT_SDK_EARLY_INIT_OK` 事件进行处理。
+ `EVENT_SDK_DB_INIT_OK`：在 KV 初始化完成后发送该事件。需要快速执行又要用到 KV 功能可以订阅 `EVENT_SDK_DB_INIT_OK` 事件。
+ `EVENT_TIME_SYNC`：时间同步事件。

## 运行结果

示例中的 KV 初始化是在 `tuya_iot_init_params()` 中进行初始化的，可以[点击阅读](https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-iot_abi_device_init?id=Kc67dkj0mxrne#title-8-%E7%B3%BB%E7%BB%9F%E6%9C%8D%E5%8A%A1%E5%88%9D%E5%A7%8B%E5%8C%96%E5%8F%82%E6%95%B0)了解更多关于 KV 的信息。

`example_os_event.c` 示例的执行日志如下：

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

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
