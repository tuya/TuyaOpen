

# KV COMMON

##  简介

这个项目将会介绍如何使用 `tuyaos 3 database kv` 常用接口，进行读写等操作。

* `KV` 相关知识

1. `kv` 组件可以让 `flash` 等存储器成为一个小型的 `KV` 数据库（Key-Value），非常适用于智能家居、物联网等需要断电存储功能的产品。

2. `cache` （缓存）。如今 `CPU` 的运行速度远大于内存速度。所以 `CPU` 每次要进行读写时都要等待内存。 `cache` 的出现就解决了这个问题。 `CPU` 可以直接把数据写入 `cache` ，然后就可以干其他事情了，`cache` 则会在合适的时机将数据写入内存。

3. `Mgr` 用于记录所在 `Block` 的基本信息和 `Page` 的使用情况。

4. `Key` 用于记录对应的 `Value` 所在的 `Page` 的存储信息，是查找每条数据的唯一关键字。
   
5. `Value` 所在的 `page` 存储着 `Key` 对应的数据。

* `KV` 的存储

假设在存储器中有1块这样的内存，在内存中有专门的1块空间用于 `KV` 存储。 `Key` 只会占用1个 `page` ，而 `Value` 是可连续/不连续的存储，也可以跨越 `Block/Page` 存储。

![KV3.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165535882961009261a7d.png)

* `KV` 的读

![kvreadread.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165563761695d67b1c5f3.png)

* `KV` 的写

![kvwritewrite.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556383226cb7240b80d.png)

## 流程介绍

![kV流程.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16577810904bb10cfb168.png)

## 运行结果
打印存储相关信息。
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
判断键是否在数据库中。
```c
[01-01 00:00:05 TUYA D][lr:0x4a7bb] key is in the database
```

读取数据并打印。
```c
[01-01 00:00:05 TUYA D][lr:0x4a7db] read len 3
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[0] = 1
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[1] = 2
[01-01 00:00:05 TUYA D][lr:0x4a7ed] out_value[2] = 3
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
