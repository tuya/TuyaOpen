# lvgl_demo

## Introduction
This project is a demo of the LVGL library, which is a lightweight graphics library for embedded systems. It provides a simple and efficient way to create graphical user interfaces (GUIs) for embedded systems. The library is designed to be easy to use and efficient, making it ideal for use in resource-constrained environments.

## Supported Platforms and Interfaces
- [] T3: SPI
- [x] T5AI: RGB/8080/SPI
- [ ] ESP32

## Supported Drivers
### Screen
- SPI
    - [x] ST7789 
    - [x] ILI9341
    - [x] GC9A01

- RGB
    - [x] ILI9488

### Touch
- I2C
    - [x] GT911
    - [x] CST816
    - [x] GT1511

### Rotary Encoder

## Supported Development Board List
| Development Board | Screen Interface and Driver | Touch Interface and Driver | Touch Pins | Notes |
| -------- | -------- | -------- | -------- | -------- |
| T5AI_Board | RGB565/ILI9488 | I2C/GT1511 | SCL(P13)/SDA(P15) | [https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) |


> More driver adaptations and testing in progress...

## Usage Process
1. Run `tos menuconfig` to configure the project.

2. Configure the corresponding screen/touch/rotary encoder drivers.

3. Configure the corresponding GPIO pins.

4. Compile the project.

5. Burn and run.