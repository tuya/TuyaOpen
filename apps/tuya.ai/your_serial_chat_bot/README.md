English | [简体中文](./RAEDME_zh.md)

# your_serial_chat_bot
[your_serial_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_serial_chat_bot) is an open-source large model intelligent chatbot based on tuya.ai. It receives text conversation through UART from external devices (such as PCs), uploads the content to the cloud for AI processing, and returns the AI response to the external device through UART. **Only the minimal core board is needed to achieve AI conversation!!!**

## Supported Features

1. AI intelligent conversation via UART
2. Text-based conversation mode without voice processing
3. Real-time chat content transmission through serial port
4. Quick Bluetooth network connection to the router
5. Real-time switching of AI entity roles on the APP side

## Hardware Dependencies
1. UART communication interface
2. Network connectivity (Wi-Fi or Ethernet)

## Supported Hardware
| Model | Description | Reset Method |
| --- | --- | --- |
| TUYA T5AI_Board Development Board | [https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | Reset by restarting 3 times |
| TUYA T5AI_EVB Board | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | Reset by restarting 3 times |
| ATK T5AI Mini Board |  | Reset by restarting 3 times |
| DNESP32S3_BOX | [https://www.alientek.com/Product_Details/118.html](https://www.alientek.com/Product_Details/118.html) | Reset by restarting 3 times |
| ESP32S3_BREAD_COMPACT_WIFI |  | Reset by restarting 3 times |
| WAVESHARE_ESP32S3_Touch_AMOLED_1.8 | [https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm) | Reset by restarting 3 times |
| XINGZHI_ESP32S3_CUDE_0.96_OLED_WIFI | [https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html](https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html) | Reset by restarting 3 times |

## Working Principle
1. The external device (such as PC) sends conversation text to the embedded device through UART (115200, 8N1)
2. The embedded device receives the text and uploads it to Tuya Cloud AI service
3. Tuya Cloud AI service processes the request and returns the response
4. The embedded device receives the AI response and sends it back to the external device through UART

## UART Communication Protocol
- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None
- Data format: UTF-8 encoded text

## Compilation
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tos menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.

## Configuration Description

### Default Configuration
- UART0 is used for communication with external devices
- Conversation mode is free dialogue mode
- No AEC enabled

### General Configuration

- **Select Dialogue Mode**

  - Free Dialogue Mode

    | Macro | Type | Description |
    | ----- | ---- | ----------- |
    | ENABLE_CHAT_MODE_ASR_WAKEUP_FREE | Boolean | Device can have continuous conversation after wake-up. If no sound is detected for 30 seconds, it needs to be woken up again. |

- **UART Configuration**

  | Macro | Type | Description |
  | ----- | ---- | ----------- |
  | USER_TEXT_UART | Number | UART port used for communication, default is TUYA_UART_NUM_0 |

### Connection Diagram
```
+-----------------+      UART       +---------------------+      Network      +--------------+
| External Device | <------------> | your_serial_chat_bot | <--------------> | Tuya Cloud AI |
+-----------------+   (Text Data)  +---------------------+   (Wi-Fi/Ethernet)  +--------------+
```

## Usage Instructions
1. Connect your development board to the PC via UART
2. Open a serial terminal (e.g. PuTTY, SecureCRT) with settings 115200 baud rate
3. Send text messages to the device
4. Receive AI responses from the device
