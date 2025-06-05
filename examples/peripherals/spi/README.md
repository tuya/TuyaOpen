# SPI

## Introduction

This project will introduce how to use the `tuyaos 3 spi` related interfaces for master to send and slave to receive.

* `SPI` Introduction

`SPI` (`Serial Peripheral Interface`), is a high-speed, full-duplex communication bus.

* `SPI` Modes

`SPI` has four operating modes, each described by two parameters known as clock polarity `CPOL` and clock phase `CPHA`.

`CPOL` = 0 means `SCK` is 0 when idle;

`CPOL` = 1 means `SCK` is 1 when idle;

`CPHA` = 0 means data is valid on the first edge of `SCK`;

`CPHA` = 1 means data is valid on the second edge of `SCK`;

* Master and Slave

The device providing the clock is the master (`Master`), and the device receiving the clock is the slave (`Slave`). The read and write operations of the `SPI` interface are initiated by the master. When multiple slave devices are present, they are managed through their respective chip select signals.

## Process Introduction

![spi 2 mosi process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655966377b125939e412.png)

## Execution Results

The master uses SPI to send `"hello world"`.

## Technical Support

You can obtain Tuya's support through the following methods:
- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com