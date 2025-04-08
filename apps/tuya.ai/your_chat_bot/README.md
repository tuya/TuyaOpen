English | [简体中文](./RAEDME_zh.md)

# your_chat_bot
[your_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot) is an open-source large model intelligent chatbot based on tuya.ai. It collects voice through a microphone, performs speech recognition, and enables conversation, interaction, and banter. You can also see real-time chat content on the screen.

## Supported Features

1. Intelligent Conversation
2. Button Wake-up, Turn-based Conversation
3. Real-time display of chat content on LCD, real-time viewing of chat content on the APP
4. Quick connection to the router via Bluetooth
5. Real-time switching of AI character roles on the APP

![](../../../docs/images/apps/your_chat_bot.png)

## Hardware Dependencies
1. Audio capture
2. Audio playback

## Supported Hardware
| Model | Description | Reset Method |
| --- | --- | --- |
| TUYA T5AI_Board Development Board | [https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | Reset by restarting 3 times |
| TUYA T5AI_EVB Board | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | Reset by restarting 3 times |

## Compilation
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tso menuconfig` command to make changes.
3. Run the `tso build` command to compile the project.
