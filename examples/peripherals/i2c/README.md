# I2C

## Introduction

This project will introduce how to use the `i2c` related interfaces, configure `i2c`, and read the temperature alarm value of the SHT 30 temperature and humidity sensor.

* IIC Introduction
  
`IIC (Inter-Integrated Circuit)` is a serial communication bus that complies with the `IIC` protocol. Generally, it uses two lines, one is a bidirectional data line `SDA`, and the other is also a bidirectional clock line `SCL`. They are connected to a positive power voltage through a current source or an upper pull-up resistor.

* Master and Slave

The devices connected to the bus are divided into master and slave. The master is the device that generates the start condition, generates the clock signal, and generates the stop condition; the slave is the device that is addressed by the master. In this example, the device running this example is the master, and the temperature and humidity sensor is the slave.

* `IIC` 7-bit Address

In the 7-bit addressing process, the slave address starts to be transmitted after the start signal, and the first byte of this byte is the slave address, and the 8th bit is the read/write bit, where 0 represents write and 1 represents read.

![IIC 7bit addr .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16559550919f1bbf424e0.png)
  
* `IIC` 8-bit Address

Some manufacturers say it is an 8-bit address, but in fact, the read/write bit is also included, for example, write address 0x92, read address 0x93. However, the slave address is still the first 7 bits.

![IIC 8bit addr .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/1655955675507f55f126a.png)

* `IIC` 10-bit Address

10-bit addresses will occupy 2 bytes.

The first 7 bits are `1111 0xx`, the next 2 bits `xx` are the 2 most significant bits of the 10-bit address. The 8th bit of the first byte is the read/write bit. The second byte is used for multiple slave device responses.

![IIC 10 BIT addr .png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/165596230044cc3b2fc69.png)


## Process Introduction
Related function directory:
`software/TuyaOS/adapter/i2c/include/tkl_i2c.h`

![iic process 12138.png](https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16559631329b212c91f70.png)

## Running Result
i2c reads the temperature alarm value of the temperature and humidity sensor.
```
[01-01 00:06:58 TUYA D][lr:0x4a980] The alarm value you set is = 60
```

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com
