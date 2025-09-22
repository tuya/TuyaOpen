# Tuya T5AI Version Otto Robot Making Tutorial Guide

## Project Overview

Otto Robot is an open-source humanoid robot platform that supports a variety of functional extensions. This guide will help you quickly build and configure your own Otto Robot and control it remotely via the Tuya Smart APP.

## Demonstration Video

- **Scan the QR code to watch the video**

![img](https://images.tuyacn.com/fe-static/docs/img/0c52d686-4a60-43e5-a7c2-39a977deb204.png)

## I. Material List

The following hardware materials are required to make the Otto Robot:

### 1. Shell

- **Model**: Otto Robot 3D-printed body shell  
- **Purchase Link**: [Xianyu]

### 2. Servos

- **Model**: SG90 180-degree servo  
- **Purchase Channel**: Taobao

### 3. Display Screen

- **Model**: ST7789 1.54-inch screen  
- **Purchase Channel**: Taobao

### 4. Development Board

- **Model**: T5 mini development board  
- **Purchase Method**: Taobao

## II. Hardware Wiring Diagram

| Hardware Device | Peripheral | T5 Pin | Pin Function      |
|-----------------|------------|--------|-------------------|
| Screen          | SCL        | P14    | SPI0 clock        |
|                 | CS         | P13    | SPI0 chip select  |
|                 | SDA        | P16    | SPI0 data         |
|                 | RST        | P19    | Screen reset      |
|                 | DC         | P17    | Data/command select |
|                 | BLK        | Not connected | Screen backlight |
| Servo           | PWM0       | P18    | Left leg servo    |
|                 | PWM1       | P24    | Right leg servo   |
|                 | PWM2       | P32    | Left foot servo   |
|                 | PWM3       | P34    | Right foot servo  |
|                 | PWM4       | P36    | Left hand servo   |
|                 | PWM5       | P9     | Right hand servo  |

## III. Software Design

### 1. Code Download

- **Main Repository**: [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)

### 2. Development Documentation

- **Documentation Address**: [Tuya Development Documentation](https://tuyaopen.ai/) (Read the documentation carefully first)

### 3. Configuration Modification

- **UUID Acquisition**: Visit the [Tuya Open Repository](https://github.com/tuya/TuyaOpen/tree/master), click the "Star" button in the upper right corner, join the group to obtain the UUID.
- **Configure T5 mini Development Board Pins**: In `apps/tuya.ai/your_otto_robot/`, use the command `tos menuconfig` and follow the instructions below to select the configuration.

### Screen Rotation 90 Degrees Modification

![img](./imgs/screen-rotate.png)

### After completing the above configuration, save and run `tos build`. Only after compiling will the `platform/T5AI/tuyaos/tuyaos_adapter/src/driver/tkl_pwm.c` file be pulled down.

1. Locate the `ty_to_bk_pwm(TUYA_PWM_NUM_E ch_id)` interface and modify the PWM mapping table inside it as follows:
   ```c
   pwm_chan_t ty_to_bk_pwm(TUYA_PWM_NUM_E ch_id)
   {
       pwm_chan_t pwm = PWM_ID_MAX;
       switch(ch_id) {
           case TUYA_PWM_NUM_0:
               pwm = PWM_ID_0;
           break;
           case TUYA_PWM_NUM_1:
               pwm = PWM_ID_4;
           break;
           case TUYA_PWM_NUM_2:
               pwm = PWM_ID_6;
           break;
           case TUYA_PWM_NUM_3:
               pwm = PWM_ID_8;
           break;
           case TUYA_PWM_NUM_4:
               pwm = PWM_ID_10;
           break;
           case TUYA_PWM_NUM_5: // Modify this line
               pwm = PWM_ID_3;
           break;

           default:
           break;
       }

       return pwm;
   }
   ```
2. Modify `#define TUYA_PWM_ID_MAX 6`

Remember to recompile: `tos.py build`

### 4. Community Support

- **Enterprise WeChat Group**

![img](https://cdn.nlark.com/yuque/0/2025/jpeg/55332580/1747998771203-5ac06211-d6ce-424d-99f9-b431804ebc80.jpeg?x-oss-process=image%2Fformat%2Cwebp)

- **QQ Group**: [Join Tuya AI Development Group](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai) (Click "Star" to get an authorization code)

![img](https://cdn.nlark.com/yuque/0/2025/png/55332580/1747998833234-310a2deb-5b01-4ebe-8e85-0b58f3b568f0.png)

## IV. Firmware Flashing Guide

### 1. Flashing Preparation (Refer to Chapter II)

1. Download the latest firmware `.bin` file.
2. Download the flashing tool or use the `tos` command in a Linux environment.
3. Connect the T5 Mini development board using a Type-C data cable.

### 2. Flashing Steps

1. Open the flashing tool.
2. Select the correct COM port.
3. Set the chip type to T5.
4. Set the flashing address to `0x0`.
5. Select the downloaded firmware file.
6. Click "Start" to begin flashing.

## V. Control Effect Confirmation

### 1. AI Motion Control

1. Download the Tuya Smart APP.
2. In the upper right corner of the APP, add a sub-device and select "Robot".
3. Enter the control interface, and you can control the robot via the APP to achieve:

   - Move left and right  
   - Move forward and backward  

4. Use voice commands to control Otto Robot's movement (wake word: "Hello, Tuya").

### 2. AI Chat

1. Use voice commands to initiate a chat (wake word: "Hello, Tuya").

### 3. Feature List

- Supports basic walking actions  
- Supports voice command control  
- Displays status information on the screen  
- Supports video recognition (planned for the future)

## VI. Resource Support

- **Technical Exchange**: Join the Tuya AI Development WeChat & QQ groups to get technical support.
- **Community Sharing**: Feel free to share your project experience on GitHub or the Tuya Developer Community.

Wish you success in building your own smart Otto Robot!

## VII. Acknowledgments

This project thanks the following open-source authors for their support:

1. [txp666]

This project thanks the following open-source projects for their support:

1. OttoDIYLib

This project thanks the following open-source communities for their support:

1. JLCEDA