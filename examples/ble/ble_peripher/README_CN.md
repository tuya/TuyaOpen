

# BLE PERIPHERAL

##  简介

这个项目将会介绍如何使用 `tuyaos 3 ble peripheral ` 相关接口，开启蓝牙的外围设备模式（从机模式），并且手机作为中心设备（主机模式）连接此蓝牙。

* 外围设备和中心设备

外围设备和中心设备也叫从机和主机，但是从机和主机的英文，slave 和 master 在国外涉及到敏感的政治话题，所以在最近的蓝牙版本中使用外围设备和中心设备代替从机和主机。
|模式|介绍|
|----|----|
|外围设备|在蓝牙连接过程中接受建立一个物理连接请求的设备定义为外围设备（ `Peripheral` ）。|
|中心设备|在在蓝牙连接过程中发起物理连接请求的设备定义为中心设备（ `Central` ）。|

你的蓝牙耳机，进入配网模式，等待手机的连接。此时你的蓝牙耳机就是外围设备，你的手机就是中心设备。



蓝牙设备会每隔一定的时间广播一个数据包出去。在附近的蓝牙设备就能每隔一段时间收到对应的数据包。

BLE 的蓝牙广播包有四种基本类型。

|广播包|介绍|
|---|---|
|可连接、非定向的广播|这种广播，可以接受对方的扫描请求(如果对方是主动扫描)，可以接受对方的连接请求。|
|可连接、定向广播| 这种广播，不接受对方的扫描请求，只接受对方的连接请求。通常这种广播包报文仅含广播者的地址和连接发起者的地址，对方收到地址后，可以快速建立连接。|
|不可连接、非定向的广播|这种广播，不接受对方的扫描请求、不接受对方的连接请求。这个与原始的含义相近。|
|可扫描、非定向广播|这种广播，可以接受对方的扫描请求，以便对方能够通过扫描请求获取到更多的信息。但是也仅此而已，不接受对方的连接请求。|

广播数据格式如下如所示。 `LEN` 为该字段长度 （TYPE+VALUE） 的长度。 `TYPE` 为数据类型，参考 `GAP` ( `Generic Access Profile` )。

![adv scanf receice 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571037357358aab4df.png)

* 参数更新

参数更新的具体解释是指两个设备连接成功时更新两者的连接参数。这个命令只能中心设备发起。

蓝牙设备的参数往往都不一样，所以在两个设备连接成功之后主机会发送参数更新请求，更新连接间隔（ `Connection Interval` ）、超时时间、有效连接间隔等参数。

* MTU 交换

MTU 全称为 `Maximum Transmission Unit` (最大传输单元),指在一个 `PDU(Protocol Data Unit)` 协议数据单元中能传输的最大数据量，主机告诉从机，主机的 `MTU` 是多少，从机再回复主机，从机的 `MTU` 是多少。然后主从机选择使用较小的 `MTU` 值。所以在两个设备连接成功之后主机会发送 `MTU` 请求，确保通信时的 `MTU` 一致。

* 蓝牙特征值、服务和配置文件

蓝牙中有很多配置文件（ `profile` ），每一个配置文件对应一个具体的应用场景。例如有一款蓝牙的心率检测仪，这个就是一个蓝牙 `profile` 的应用场景。一个配置文件通常有一个或者多个服务组成，每个服务对应特定的功能，例如心率检测服务。每个服务包含一个或者多个特征值，例如心率检测服务中就包含了一个心率特征值。一个特征值中又包含多个 `ATT` ， `ATT` 就是一种协议，在代码里面就是一个结构体。

![profile.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655690717424d0b54677.png)


* 蓝牙状态

蓝牙的主要状态可以分为初始化、连接、广播、扫描和空闲状态。
扫描状态只有蓝牙当中心设备下才有。

![ble_status.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556929972457f36e12e.png)

## 流程介绍

![ble per process .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655694962a3d531f8f48.png)


## 运行结果
`Ble` 初始化事件。蓝牙开启广播，等待主机连接。
```c
[01-01 00:00:00 TUYA D][lr:0x84aa3] ----------bluetooth event callback-------
[01-01 00:00:00 TUYA D][lr:0x84aa3] bluetooth event is : 1
[01-01 00:00:00 TUYA D][lr:0x84aa3] init Ble/Bt stack and start advertising
```
打开手机 `nRF connect` APP，扫描名为 `TY` 的蓝牙设备，点击连接。

`BLE` 进入连接事件，读取特征值。
```c
[01-01 00:07:12 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:12 TUYA D][lr:0x4aa4f] bluetooth event is : 2
[01-01 00:07:12 TUYA D][lr:0x4aaa7] Bluetooth starts to connect...
[01-01 00:07:12 TUYA I][lr:0x820b3] char handle = 0x00
```

`BLE` 进入连接状态之后，主机会主动发起连接参数更新和 `MTU` 交换。

`BLE` 进入连接参数更新事件。
```c
[01-01 00:07:13 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:13 TUYA D][lr:0x4aa4f] bluetooth event is : 7
[01-01 00:07:13 TUYA D][lr:0x4ab13] Parameter update successfully!
[01-01 00:07:13 TUYA D][lr:0x4ab4f] Conn Param Update: Min = 45.000000 ms, Max = 45.000000 ms, Latency = 0, Sup = 6000 ms
```

`BLE` 进入 `MTU` 交换请求事件。
```c
[01-01 00:07:13 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:13 TUYA D][lr:0x4aa4f] bluetooth event is : 9
[01-01 00:07:13 TUYA D][lr:0x4ab57] MTU exchange request event.
[01-01 00:07:13 TUYA D][lr:0x4ab5f] Get Response MTU Size = 256
```

打开手机 `nRF connect` APP，主机向从机发送数据  `0x1111`。

`BLE` 进入数据接收事件。

```c
[06-10 15:34:11 TUYA D][lr:0x4bb03] ----------bluetooth event callback-------
[06-10 15:34:11 TUYA D][lr:0x4bb0b] bluetooth event is : 12
[06-10 15:34:11 TUYA D][lr:0x4bc6f] Get Client-Write Char Request
[06-10 15:34:11 TUYA D][lr:0x4bc83] client send  data[0]: 17
[06-10 15:34:11 TUYA D][lr:0x4bc83] client send  data[1]: 17
```

打开手机 `nRF connect` APP，主机断开连接，从机重新开始发广播。

`BLE` 进入断开连接事件。
```c
[06-10 15:34:57 TUYA D][lr:0x4bb03] ----------bluetooth event callback-------
[06-10 15:34:57 TUYA D][lr:0x4bb0b] bluetooth event is : 4
[06-10 15:34:57 TUYA D][lr:0x4bb9f] Bluetooth disconnect.
[06-10 15:34:57 TUYA N][lr:0xb32d3] Start Adv
```

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
