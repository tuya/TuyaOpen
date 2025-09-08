English | [简体中文](./RAEDME_zh.md)

# your_desk_emoji
[your_desk_emoji](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_desk_emoji) is an intelligent desk emoji robot based on tuya.ai. It combines AI conversation, gesture recognition, servo control, and emoji display to create an interactive desk companion.

**Note: Switching between TUYA AI V1.0 and V2.0 requires removing the device and clearing the data on the APP before use.**

## Supported Features

1. AI intelligent conversation
2. Button wake-up/Voice wake-up, turn-based dialogue, supports voice interruption (hardware support required)
3. Emmo emoji display with blink animations
4. Gesture recognition using PAJ7620 sensor
5. Servo motor control for interactive movements
6. Supports LCD for displaying real-time chat content and supports viewing chat content in real-time on the APP side
7. Quick Bluetooth network connection to the router
8. Real-time switching of AI entity roles on the APP side


![](../../../docs/images/apps/your_chat_bot.png)

## Hardware Dependencies
1. Audio capture
2. Audio playback
3. I2C interface for gesture sensor (PAJ7620)
4. PWM interface for servo motor control
5. Display interface for emoji animations

## Supported Hardware
| Model | Description | Reset Method |
| --- | --- | --- |
| TUYA T5AI_Board Development Board | [https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | Reset by restarting 3 times |
| TUYA T5AI_EVB Board | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | Reset by restarting 3 times |

## Compilation
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tos menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.
