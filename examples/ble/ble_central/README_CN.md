

# BLE CENTRAL

##  简介

这个项目将会介绍如何使用 `tuyaos 3 ble central ` 相关接口，开启蓝牙的中心设备模式（主机模式）,并扫描当前环境的蓝牙信息。

* 外围设备和中心设备

外围设备和中心设备也叫从机和主机，但是从机和主机的英文，slave 和 master 在国外涉及到敏感的政治话题，所以在最近的蓝牙版本中使用外围设备和中心设备代替从机和主机。
|模式|介绍|
|----|----|
|外围设备|在蓝牙连接过程中接受建立一个物理连接请求的设备定义为外围设备（ `Peripheral` ）。|
|中心设备|在在蓝牙连接过程中发起物理连接请求的设备定义为中心设备（ `Central` ）。|

你的蓝牙耳机，进入配网模式，等待手机的连接。此时你的蓝牙耳机就是外围设备，你的手机就是中心设备。

* 蓝牙设备的地址
  
在以前蓝牙的 `Mac` 地址都是独一无二的。和电脑的 `Mac` 地址一样，需要花钱向 `IEEE` 购买，这唯一的地址就是 `Public Device Address`。

由于 `BLE` 的出现，导致 `Public Device Address` 地址不够用。因为 `BLE` 设备远远大于传统蓝牙，而且低功耗蓝牙非常讲究成本， `Public Device Address` 的申请费也是一笔不小的开销。为了解决这些问题， `BLE` 协议新增了一种地址 `Random Device Address` 即设备地址不是固定分配
的，而是在设备设备启动后随机生成的。

根据不同的目的， `Random Device Address` 分为 `Static Device Address`
和 `Private Device Address` 两类。`Static Device Address` 是设备在上电时随机生成的地址，这个地址是最常用的，当我们扫描 `BLE` 地址的时候，我们会发现地址的最高两位基本上都是 “11”。此地址有如下几个重要特征：
1. 最高两个 bit 为“11”。
2. 剩余的 46bits 是一个随机数，不能全部为 0，也不能全部为 1。
3. 在一个上电周期内保持不变。
4. 下一次上电的时候可以改变。但不是强制的，因此也可以保持不变。如果改变，上次保存的连接等信息，
将不再有效。

根据不同使用场景，`Private Device Address` 又分为 `Non-resolvable private address` 和 `Resolvable private address`。`Non-resolvable private address` 和 `Static Device Address` 类似，但是`Non-resolvable private address` 的地址会每隔一段时间动态变化，更新的事件由 `GAP` 规定，一般为15分钟。由于地址每隔一段时间变化，连接一段时间就自己断开了，所以应用场景并不是很清晰，在实际产品开发中一般不使用这类地址。

`Resolvable private address` 这类地址可以防止被未知设备扫描和追踪。这类地址通过一个随机数和一个称作 `IRK(identity resolving key)`的密码生成，此地址有如下几个重要特征：
1. 高位24bits 是随机数部分，其中最高两个 bit 为“10”，用于标识地址类型。
2. 低位24bits 是随机数和 `IRK` 经过 `hash` 运算得到的 `hash` 值，运算的公式为 `hash = ah(IRK, prand)`。
3. 当蓝牙设备扫描到该类型的蓝牙地址后，会使用保存在本机的 `IRK` ，和该地址中的 `prand` ，进行同样的 `hash` 运算，并将运算结果和地址中的 `hash` 字段比较，相同的时候，才进行后续的操作。这个过程称作 `解析（resolve）`
4. 此类地址定时更新。哪怕在广播、扫描、已连接等过程中，也可能改变。
5. `Resolvable private address` 不能单独使用，因此需要使用该类型的地址的话，设备要同时具备 `Public
Device Address` 或者 `Static Device Address` 中的一种。

![ble addr 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655707049daaca34aa64.png)


* 蓝牙广播包
  
用户定义广播包和扫描回应包最多能有31个字节，每个包分为有效数据和无效数数两部分。
1. 有效数据部分，由若干个 `AD Structure` 单元组成。`LEN` 为该字段长度；`TYPE` 为数据类型，参考 `GAP` ( `Generic Access Profile` )；`DATA` 表示广播的内容。他们三者之间遵循有一个公式，用于拆分不同的 `AD Structure`， `LEN` = `TYPE` 的长度 + `DATA` 的长度。
2. 无效数据部分，广播包必须有31字节，如果有效数据部分达不到31字节，剩下所有补0。

![adv scanf receice 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571037357358aab4df.png)

* RSSI

`RSSI` 即接收信号强度（`Received Signal Strength Indicator`）。蓝牙的信号强度往往都是负数，实际上 `BLE` 的无线信号通常为 `mW` 级别，所以对它进行了极化，转化为 `dBm` 而已，并不表示信号是负的。`1mW` 就是 `0dBm`，小于 `1mW` 就是负数的 `dBm` 数。




## 流程介绍
![ble central process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655711933d7589ae9af8.png)

## 运行结果
打印蓝牙设备地址和地址类型。绝大多数的设备的地址类型都是随机设备地址中的静态设备地址。十进制的“14”对应二进制的“1110”，最高两位为“11”。
```c
[01-01 00:00:58 TUYA D][lr:0x4aab3] ----------bluetooth event callback-------
[01-01 00:00:58 TUYA D][lr:0x4aabb] bluetooth event is : 5
Scanf device peer addr:   14  82  134  82  129  59 
[01-01 00:00:58 TUYA D][lr:0x4aaf3] Peer addr type is random address
```

打印蓝牙信号强度。
```c
[01-01 00:00:58 TUYA D][lr:0x4ab1d] Received Signal Strength Indicator : -64
```
接收到的广播数据包。
```c
[01-01 00:00:06 TUYA D][lr:0x4ab23] Advertise packet data length : 31
Advertise packet data:
0x05,0x09,0x31,0x32,0x33,0x34,0x02,0x0A,0x08,0x15,0xFF,0x41,0x50,0x50,0x4C,0x45,0x06,0x00,0x01,0x09,0x32,0x02,0xA1,0x59,0x36,0x5B,0x9C,0x8F,0xFA,0x9A,0x7D
```
我们根据广播包的公式 `len` = `type` + `data`, 将广播包拆解为三个 `AD Structure`。
广播包的最高位0x05就是第一个 `AD Structure` 的长度。广播包的类型可以登录蓝牙官方网站查阅相关文档。0x09的含义是完整的设备名称 `UTF-8` 编码。我们分析数据，0x31,0x32,0x33,0x34 按照 `UTF-8` 编码，就是1234。所以第一个 `AD Structure` 告诉我们这个设备的名称叫1234。

![adv strruct 111111.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571934440ff10d7039.png)


第二个 `AD Structure`， `type` 加上 `data` 只有两个。0x0A是发射功率，数据是0x08。所以第二个`AD Structure`告诉我们，蓝牙的发射功率是8。

![adv struct 2222222.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655719879f410e516a62.png)

第三个 `AD Structure`，长度为0x15，类型为0xFF,这个类型比较特殊，是蓝牙厂商的自定义数据。剩余的数据就是厂商自定义数据。

![adv struct33333.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165572034447281e62ee5.png)

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
