# KV COMMON

## Introduction

This project will introduce how to use the common interfaces of `tuyaos 3 database kv` for operations such as reading and writing.

* `KV` Related Knowledge

1. The `kv` component allows storage devices like `flash` to become a small `KV` database (Key-Value), which is very suitable for smart home, IoT, and other products that require power-off storage functions.

2. `cache` (Cache). Nowadays, the running speed of `CPU` is much higher than that of memory. So every time the `CPU` needs to read or write, it has to wait for the memory. The appearance of `cache` solves this problem. The `CPU` can directly write data into the `cache`, and then do other things, while the `cache` will write the data into memory at the appropriate time.

3. `Mgr` is used to record the basic information of the `Block` it belongs to and the usage of `Page`.

4. `Key` is used to record the storage information of the `Page` where the corresponding `Value` is located, and is the unique keyword for finding each piece of data.

5. The `page` where `Value` is located stores the data corresponding to the `Key`.

* `KV` Storage

Assuming there is such a memory in the storage device, there is a special space in the memory for `KV` storage. A `Key` will only occupy one `page`, while `Value` can be stored continuously/discontinuously, and can also span `Block/Page`.
![KV3.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165535882961009261a7d.png)

* Reading `KV`

![kvreadread.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165563761695d67b1c5f3.png)

* Writing `KV`

![kvwritewrite.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556383226cb7240b80d.png)

## Process Introduction

![kV流程.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16577810904bb10cfb168.png)
## Execution Results
Print storage-related information.
```c
[01-01 00:00:05 TUYA D][lr:0x4a767] start flash : 1ef000
[01-01 00:00:05 TUYA D][lr:0x4a76f] flash size : e000
[01-01 00:00:05 TUYA D][lr:0x4a777] flash block/sector size : 1000
[01-01 00:00:05 TUYA D][lr:0x4a77f] swap flash start address : 1fd000
[01-01 00:00:05 TUYA D][lr:0x4a787] swap flash size  : 3000
[01-01 00:00:05 TUYA D][lr:0x4a78f] key restore addr  : 1ee000
[01-01 00:00:05 TUYA D][lr:0x4a797] protected addr  : 1ed000
[01-01 00:00:05 TUYA D][lr:0x4a79f] flash physical block size  : 00
```
Determine if the key is in the database.
```c
[01-01 00:00:05 TUYA D][lr:0x4a7bb] key is in the database
```
Read data and print.
```c
[01-01 00:00:05 TUYA D][lr:0x4a7db] read len 3
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[0] = 1
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[1] = 2
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[2] = 3
```
## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com