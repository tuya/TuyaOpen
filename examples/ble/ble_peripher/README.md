# BLE PERIPHERAL

## Introduction

This project will introduce how to use the `tuyaos 3 ble peripheral` related interfaces to enable Bluetooth peripheral mode (slave mode), with the phone acting as the central device (master mode) connecting to this Bluetooth.

* Peripheral and Central Devices

Peripheral and central devices are also known as slave and master, respectively. However, the terms slave and master are sensitive political topics abroad, so in recent Bluetooth versions, the terms peripheral and central devices are used instead.
|Mode|Introduction|
|----|------------|
|Peripheral Device|In the Bluetooth connection process, the device that accepts a request to establish a physical connection is defined as a peripheral device (`Peripheral`).|
|Central Device|In the Bluetooth connection process, the device that initiates a physical connection request is defined as a central device (`Central`).|

Your Bluetooth headset, entering pairing mode, waiting for the phone's connection. At this time, your Bluetooth headset is the peripheral device, and your phone is the central device.

Bluetooth devices broadcast a data packet at regular intervals. Nearby Bluetooth devices can receive the corresponding data packet at intervals.

There are four basic types of BLE Bluetooth broadcast packets.
|Broadcast Packet|Introduction|
|----------------|-------------|
|Connectable, undirected advertising|This type of broadcast can accept scan requests from the other party (if the other party is actively scanning) and can accept connection requests from the other party.|
|Connectable, directed advertising|This type of broadcast does not accept scan requests from the other party, only connection requests. Usually, this broadcast packet only contains the broadcaster's address and the initiator's address, allowing for quick connection establishment upon receiving the address.|
|Non-connectable, undirected advertising|This type of broadcast does not accept scan requests or connection requests from the other party, similar to the original meaning.|
|Scannable, undirected advertising|This type of broadcast can accept scan requests from the other party, allowing the other party to obtain more information through the scan request. However, it does not accept connection requests from the other party.|

The broadcast data format is shown below. `LEN` is the length of the field (`TYPE+VALUE`). `TYPE` is the data type, referring to `GAP` (`Generic Access Profile`).

![adv scanf receice 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165571037357358aab4df.png)

* Parameter Update

The specific explanation of parameter update refers to updating the connection parameters between two devices when they are successfully connected. This command can only be initiated by the central device.
Bluetooth devices often have different parameters, so after two devices are successfully connected, the host will send a parameter update request to update the connection interval (`Connection Interval`), timeout, effective connection interval, etc.

* MTU Exchange

MTU stands for `Maximum Transmission Unit`, which refers to the maximum amount of data that can be transmitted in a `PDU (Protocol Data Unit)` protocol data unit. The host tells the slave what the host's `MTU` is, and the slave replies with its `MTU`. Then, the master and slave choose to use the smaller `MTU` value. Therefore, after two devices are successfully connected, the host will send an `MTU` request to ensure consistent `MTU` during communication.

* Bluetooth Characteristics, Services, and Profiles

There are many profiles in Bluetooth, each corresponding to a specific application scenario. For example, a Bluetooth heart rate monitor is an application scenario of a Bluetooth `profile`. A profile usually consists of one or more services, each corresponding to a specific function, such as a heart rate monitoring service. Each service contains one or more characteristics, such as a heart rate characteristic in the heart rate monitoring service. A characteristic contains multiple `ATTs`, `ATT` is a protocol, represented as a structure in the code.
![profile.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655690717424d0b54677.png)

* Bluetooth States

The main states of Bluetooth can be divided into initialization, connection, advertising, scanning, and idle states.
The scanning state is only available when Bluetooth is in the central device mode.

![ble_status.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16556929972457f36e12e.png)

## Process Introduction
Related function directory:
`software/TuyaOS/include/components/tal_bluetooth/include/tal_bluetooth.h`

![ble per process .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655694962a3d531f8f48.png)

## Execution Results
BLE initialization event. Bluetooth starts advertising, waiting for the host to connect.
```c
[01-01 00:00:00 TUYA D][lr:0x84aa3] ----------bluetooth event callback-------
[01-01 00:00:00 TUYA D][lr:0x84aa3] bluetooth event is : 1
[01-01 00:00:00 TUYA D][lr:0x84aa3] init Ble/Bt stack and start advertising
```
Open the nRF connect APP on your phone, scan for the Bluetooth device named TY, and click to connect. BLE enters the connection event, reading the characteristic value.

```c
[01-01 00:07:12 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:12 TUYA D][lr:0x4aa4f] bluetooth event is : 2
[01-01 00:07:12 TUYA D][lr:0x4aaa7] Bluetooth starts to connect...
[01-01 00:07:12 TUYA I][lr:0x820b3] char handle = 0x00

```
After BLE enters the connection state, the host will actively initiate a connection parameter update and MTU exchange.

BLE enters the connection parameter update event.

```c
[01-01 00:07:13 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:13 TUYA D][lr:0x4aa4f] bluetooth event is : 7
[01-01 00:07:13 TUYA D][lr:0x4ab13] Parameter update successfully!
[01-01 00:07:13 TUYA D][lr:0x4ab4f] Conn Param Update: Min = 45.000000 ms, Max = 45.000000 ms, Latency = 0, Sup = 6000 ms
```

BLE enters the MTU exchange request event.

```c
[01-01 00:07:13 TUYA D][lr:0x4aa47] ----------bluetooth event callback-------
[01-01 00:07:13 TUYA D][lr:0x4aa4f] bluetooth event is : 9
[01-01 00:07:13 TUYA D][lr:0x4ab57] MTU exchange request event.
[01-01 00:07:13 TUYA D][lr:0x4ab5f] Get Response MTU Size = 256
```

Open the nRF connect APP on your phone, the host sends data 0x1111 to the slave.

BLE enters the data reception event.

```c
[06-10 15:34:11 TUYA D][lr:0x4bb03] ----------bluetooth event callback-------
[06-10 15:34:11 TUYA D][lr:0x4bb0b] bluetooth event is : 12
[06-10 15:34:11 TUYA D][lr:0x4bc6f] Get Client-Write Char Request
[06-10 15:34:11 TUYA D][lr:0x4bc83] client send  data[0]: 17
[06-10 15:34:11 TUYA D][lr:0x4bc83] client send  data[1]: 17
```

Open the nRF connect APP on your phone, the host disconnects, and the slave starts advertising again. BLE enters the disconnection event.

```c
[06-10 15:34:57 TUYA D][lr:0x4bb03] ----------bluetooth event callback-------
[06-10 15:34:57 TUYA D][lr:0x4bb0b] bluetooth event is : 4
[06-10 15:34:57 TUYA D][lr:0x4bb9f] Bluetooth disconnect.
[06-10 15:34:57 TUYA N][lr:0xb32d3] Start Adv
```

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com