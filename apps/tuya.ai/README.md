English | [简体中文](./README_zh.md)

# tuya.ai

By utilizing tuya.ai, this connects Tuya Cloud and AI services to achieve AI voice interaction, audio/video multimodal AI interaction, intelligent control, and other functions.

## Quick Start

### 1. Create a Product and Obtain the Product PID

Refer to the documentation [Create a Product](https://tuyaopen.ai/docs/cloud/tuya-cloud/creating-new-product) to create a product on [Tuya IoT Platform](https://iot.tuya.com) and obtain the product PID.

Then replace the `TUYA_PRODUCT_ID` macro in the corresponding project's `include/tuya_config.h` file with the actual PID.

### 2. Confirm TuyaOpen License Code

TuyaOpen Framework includes:
- C Version TuyaOpen: [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- Arduino Version TuyaOpen: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode Version TuyaOpen: [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

All use TuyaOpen-specific license codes (UUID + AuthKey). Using other license codes will not allow normal connection to Tuya Cloud.

You can obtain a TuyaOpen-specific license code through the following methods:

- **Method 1:** Purchase a module with a pre-burned TuyaOpen license code. The license is already burned into the module at the factory and will not be lost. The device reads it through the `tuya_authorize_read()` interface at startup.

- **Method 2:** Purchase a **TuyaOpen license code** through [Tuya Production Platform](https://platform.tuya.com/purchase/index?type=6), then modify the `include/tuya_config.h` file in the corresponding project:

  ```c
  #define TUYA_OPENSDK_UUID    "uuidxxxxxxxxxxxxxxxx"             // Replace with actual uuid
  #define TUYA_OPENSDK_AUTHKEY "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" // Replace with actual authkey
  ```

![authorization_code](../../docs/images/en/authorization_code.png)

License reading logic in code:

```c
tuya_iot_license_t license;

if (OPRT_OK != tuya_authorize_read(&license)) {
    license.uuid    = TUYA_OPENSDK_UUID;
    license.authkey = TUYA_OPENSDK_AUTHKEY;
    PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work");
}
```

> If `tuya_authorize_read()` returns OPRT_OK, it indicates the device has a burned TuyaOpen license code; otherwise, it will use the default values defined in `tuya_config.h`.

### 3. Compile and Flash

```bash
# Navigate to the target project directory
cd apps/tuya.ai/your_chat_bot

# Select the development board configuration
tos.py config choice

# Build the project
tos.py build

# Flash firmware
tos.py flash
```

For more details, refer to the official documentation: [Project Compilation](https://tuyaopen.ai/docs/quick-start/project-compilation) | [Firmware Burning](https://tuyaopen.ai/docs/quick-start/firmware-burning)

### 4. Device Authorization

After flashing, the license code needs to be written to the device. Two methods:

- **Command authorization**: Use `tos.py monitor -b 115200` to enter the serial monitor, then input the `auth <uuid> <authkey>` command to write the license.
- **Code preset**: Modify `TUYA_OPENSDK_UUID` and `TUYA_OPENSDK_AUTHKEY` in `include/tuya_config.h` before compiling, then reflash.

For detailed steps, refer to: [Device Authorization](https://tuyaopen.ai/docs/quick-start/equipment-authorization)

### 5. Device Pairing

After using the Tuya Smart Life APP to network and activate the device, you can perform AI voice interaction.

For detailed steps, refer to: [Device Network Configuration](https://tuyaopen.ai/docs/quick-start/device-network-configuration)

> **Note:** Currently only 2.4 GHz Wi-Fi is supported. 5 GHz networks will cause pairing to fail.

---

## AI Applications

| Application | Description |
| --- | --- |
| [your_chat_bot](./your_chat_bot/) | AI intelligent chatbot with voice dialogue, emoji display, and LCD real-time chat |
| [your_serial_chat_bot](./your_serial_chat_bot/) | Serial text AI chatbot, only requires minimal core board for AI conversation |
| [your_robot_dog](./your_robot_dog/) | AI chat robot dog with facial expressions and servo-driven actions |
| [your_desk_emoji](./your_desk_emoji/) | Smart desktop emoji robot with gesture recognition and weather display |
| [your_otto_robot](./your_otto_robot/) | Otto humanoid robot with multi-functional extensions and remote control |

### Supported Chips and Platforms

| Chip/Platform | Description |
| --- | --- |
| [Tuya T5AI](https://tuyaopen.ai/docs/hardware-specific/tuya-t5/t5-ai-board/overview-t5-ai-board) | Wi-Fi + BLE 5.4 dual-mode, ARMv8-M 480MHz, 16MB RAM |
| ESP32S3 | Espressif ESP32-S3 series |
| LINUX | Raspberry Pi, DshanPi and other Linux boards |

### Supported Development Board List

| Board | Chip | Description |
| --- | --- | --- |
| TUYA_T5AI_BOARD | T5AI | [Tuya T5AI 3.5" LCD Dev Board](https://tuyaopen.ai/docs/hardware-specific/tuya-t5/t5-ai-board/overview-t5-ai-board) |
| TUYA_T5AI_EVB | T5AI | [T5AI EVB Board](https://oshwhub.com/flyingcys/t5ai_evb) |
| TUYA_T5AI_CORE | T5AI | T5AI Core Board(https://tuyaopen.ai/docs/hardware-specific/tuya-t5/t5-ai-core/overview-t5-ai-core) |
| T5AI_MINI | T5AI | T5AI Mini Dev Board |
| T5AI_MOJI_1.28 | T5AI | Moji 1.28" screen board |
| WAVESHARE_T5AI_TOUCH_AMOLED_1.75 | T5AI | Waveshare T5AI 1.75" Touch AMOLED |
| DNESP32S3_BOX | ESP32S3 | [Alientek ESP32S3 BOX](https://www.alientek.com/Product_Details/118.html) |
| ESP32S3_BREAD_COMPACT_WIFI | ESP32S3 | ESP32S3 Breadboard |
| WAVESHARE_ESP32S3_Touch_AMOLED_1.8 | ESP32S3 | [Waveshare ESP32S3 1.8" Touch AMOLED](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm) |
| XINGZHI_ESP32S3_Cube_0.96_OLED | ESP32S3 | [XingZhi 0.96" OLED Board](https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html) |
| DshanPi_A1 | LINUX | BaiWen DshanPi-A1 |
| Raspberry Pi | LINUX | Raspberry Pi |

---

## Free TuyaOpen License Code Activity

To allow developers to freely experience the TuyaOpen Framework, you can now star the following repositories on GitHub:
- [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

Send an email to `chenyisong@tuya.com` or join the QQ group `796221529` with your GitHub account screenshot to receive a free TuyaOpen Framework specific license code.

Limited to 500 units, first come, first served.

![qq_qrcode](../../docs/images/zh/qq_qrcode.png)
