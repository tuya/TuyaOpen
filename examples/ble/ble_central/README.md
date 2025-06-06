# BLE CENTRAL

## Introduction

This project will introduce how to use the `tuyaos 3 ble central` related interfaces to enable Bluetooth Central Device Mode (Host Mode), and scan the Bluetooth information in the current environment.

* Peripheral and Central Devices

Peripheral and central devices are also known as slave and master devices. However, the terms slave and master are sensitive political topics abroad, so in recent Bluetooth versions, peripheral and central devices are used instead of slave and master.
|Mode|Introduction|
|----|------------|
|Peripheral Device|A device that accepts a request to establish a physical connection during the Bluetooth connection process is defined as a peripheral device (`Peripheral`).|
|Central Device|A device that initiates a physical connection request during the Bluetooth connection process is defined as a central device (`Central`).|

Your Bluetooth headset, entering the networking mode, waiting for the connection from the phone. At this time, your Bluetooth headset is a peripheral device, and your phone is a central device.

* Bluetooth Device Address

Previously, Bluetooth `Mac` addresses were unique. Like computer `Mac` addresses, they required purchasing from `IEEE`, and this unique address is the `Public Device Address`.
Due to the emergence of `BLE`, `Public Device Address` addresses were not enough. Because `BLE` devices far exceed traditional Bluetooth, and low-power Bluetooth is very cost-conscious, the application fee for `Public Device Address` is also a significant expense. To solve these problems, the `BLE` protocol added a new type of address, `Random Device Address`, meaning the device address is not fixedly allocated but is randomly generated after the device starts.

Depending on the purpose, `Random Device Address` is divided into `Static Device Address` and `Private Device Address` two categories. `Static Device Address` is the address randomly generated when the device is powered on, and this address is the most commonly used. When we scan `BLE` addresses, we will find that the highest two bits are basically "11". This address has the following important features:
1. The top two bits are "11".
2. The remaining 46 bits are a random number, which cannot be all 0s or all 1s.
3. It remains unchanged within a power cycle.
4. It can change the next time it is powered on. However, this is not mandatory, so it can also remain unchanged. If it changes, the previously saved connection and other information will no longer be valid.
Depending on different usage scenarios, `Private Device Address` is further divided into `Non-resolvable private address` and `Resolvable private address`. `Non-resolvable private address` is similar to `Static Device Address`, but the `Non-resolvable private address` changes dynamically every once in a while, with the update event specified by `GAP`, generally 15 minutes. Since the address changes every once in a while, causing the connection to disconnect after a period, its application scenario is not very clear, and this type of address is generally not used in actual product development.
`Resolvable private address` can prevent being scanned and tracked by unknown devices. This type of address is generated through a random number and a password called `IRK(identity resolving key)`, with the following important features:
1. The high 24 bits are the random part, among which the top two bits are "10", used to identify the address type.
2. The low 24 bits are the `hash` value obtained through the `hash` operation of the random number and `IRK`, with the formula `hash = ah(IRK, prand)`.
3. When a Bluetooth device scans this type of Bluetooth address, it will use the `IRK` saved in the device and the `prand` in the address to perform the same `hash` operation, and compare the operation result with the `hash` field in the address. Only when they match, the subsequent operations are performed. This process is called `resolving`.
4. This type of address is updated periodically. Even during broadcasting, scanning, or being connected, it may change.
5. `Resolvable private address` cannot be used alone, so if you need to use this type of address, the device must also have one of `Public Device Address` or `Static Device Address`.

![ble addr 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655707049daaca34aa64.png)

* Bluetooth Advertising Packet
Users can define advertising packets and scan response packets with up to 31 bytes, each packet is divided into valid data and invalid data parts.
1. For the valid data part, it consists of several `AD Structure` units. `LEN` is the length of this field; `TYPE` is the data type, refer to `GAP` (`Generic Access Profile`); `DATA` represents the content of the advertisement. They follow a formula for splitting different `AD Structures`, `LEN` = length of `TYPE` + length of `DATA`.
2. For the invalid data part, the advertising packet must have 31 bytes. If the valid data part does not reach 31 bytes, the rest are filled with 0s.

![adv scanf receice 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571037357358aab4df.png)

* RSSI

`RSSI` stands for Received Signal Strength Indicator. The signal strength of Bluetooth is often negative, in fact, `BLE` wireless signals are usually at the `mW` level, so it has been polarized, converted to `dBm`, and does not indicate that the signal is negative. `1mW` is `0dBm`, less than `1mW` is a negative `dBm` number.

## Process Introduction
![ble central process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655711933d7589ae9af8.png)

## Running Results
Print the Bluetooth device address and address type. The vast majority of devices' address types are static device addresses among random device addresses. The decimal "14" corresponds to the binary "1110", with the highest two bits being "11".
```c
[01-01 00:00:58 TUYA D][lr:0x4aab3] ----------bluetooth event callback-------
[01-01 00:00:58 TUYA D][lr:0x4aabb] bluetooth event is : 5
Scanf device peer addr:   14  82  134  82  129  59 
[01-01 00:00:58 TUYA D][lr:0x4aaf3] Peer addr type is random address
```

Printing Bluetooth signal strength.
```c
[01-01 00:00:58 TUYA D][lr:0x4ab1d] Received Signal Strength Indicator : -64
```
Received broadcast packet.
```c
[01-01 00:00:06 TUYA D][lr:0x4ab23] Advertise packet data length : 31
Advertise packet data:
0x05,0x09,0x31,0x32,0x33,0x34,0x02,0x0A,0x08,0x15,0xFF,0x41,0x50,0x50,0x4C,0x45,0x06,0x00,0x01,0x09,0x32,0x02,0xA1,0x59,0x36,0x5B,0x9C,0x8F,0xFA,0x9A,0x7D
```

Based on the broadcast packet formula len = type + data, we break down the broadcast packet into three AD Structures. The highest bit of the broadcast packet, 0x05, is the length of the first AD Structure. The type of the broadcast packet can be checked on the official Bluetooth website. The meaning of 0x09 is the complete device name in UTF-8 encoding. Analyzing the data, 0x31,0x32,0x33,0x34 in UTF-8 encoding translates to 1234. So, the first AD Structure tells us the name of this device is 1234.

![adv strruct 111111.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571934440ff10d7039.png)

The second AD Structure, type plus data only has two. 0x0A is the transmission power, and the data is 0x08. Thus, the second AD Structure tells us the Bluetooth transmission power is 8.

![adv struct 2222222.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655719879f410e516a62.png)

The third AD Structure, with a length of 0x15 and type 0xFF, is quite special as it represents the manufacturer's custom data. The remaining data is the manufacturer's custom data.

![adv struct33333.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165572034447281e62ee5.png)

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com