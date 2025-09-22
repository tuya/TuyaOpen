<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/c128362b-eb25-4512-b5f2-ad14aae2395c.jpg" width="100%" >
</p>

<p align="center">
  <a href="https://tuyaopen.ai/docs/quick_start/enviroment-setup">Quick Start</a> ·
  <a href="https://developer.tuya.com/en/docs/iot/ai-agent-management?id=Kdxr4v7uv4fud">Tuya AI Agent</a> ·
  <a href="https://tuyaopen.ai/docs/about-tuyaopen">Documentation</a> ·
  <a href="https://tuyaopen.ai/docs/hardware-specific/t5-ai-board/overview-t5-ai-board">Hardware Resource</a>
</p>

<p align="center">
    <a href="https://tuyaopen.ai" target="_blank">
        <img alt="Static Badge" src="https://img.shields.io/badge/Product-F04438"></a>
    <a href="https://tuyaopen.ai/pricing" target="_blank">
        <img alt="Static Badge" src="https://img.shields.io/badge/free-pricing?logo=free&color=%20%23155EEF&label=pricing&labelColor=%20%23528bff"></a>
    <a href="https://discord.gg/cbGrBjx7" target="_blank">
        <img src="https://img.shields.io/badge/Discord-Join%20Chat-5462eb?logo=discord&labelColor=%235462eb&logoColor=%23f5f5f5&color=%235462eb"
            alt="chat on Discord"></a>
    <a href="https://www.youtube.com/@tuya2023" target="_blank">
        <img src="https://img.shields.io/badge/YouTube-Subscribe-red?logo=youtube&labelColor=white"
            alt="Subscribe on YouTube"></a>
    <a href="https://x.com/tuyasmart" target="_blank">
        <img src="https://img.shields.io/twitter/follow/tuyasmart?logo=X&color=%20%23f5f5f5"
            alt="follow on X(Twitter)"></a>
    <a href="https://www.linkedin.com/company/tuya-smart/" target="_blank">
        <img src="https://custom-icon-badges.demolab.com/badge/LinkedIn-0A66C2?logo=linkedin-white&logoColor=fff"
            alt="follow on LinkedIn"></a>
    <a href="https://github.com/tuya/tuyaopen/graphs/commit-activity?branch=dev" target="_blank">
        <img alt="Commits last month (dev branch)" src="https://img.shields.io/github/commit-activity/m/tuya/tuyaopen/dev?labelColor=%2332b583&color=%2312b76a"></a>
    <a href="https://github.com/langgenius/dify/" target="_blank">
        <img alt="Issues closed" src="https://img.shields.io/github/issues-search?query=repo%3Atuya%2Ftuyaopen%20is%3Aclosed&label=issues%20closed&labelColor=%20%237d89b0&color=%20%235d6b98"></a>
</p>



<p align="center">
  <a href="./README.md"><img alt="README in English" src="https://img.shields.io/badge/English-d9d9d9"></a>
  <a href="./README_zh.md"><img alt="简体中文版自述文件" src="https://img.shields.io/badge/简体中文-d9d9d9"></a>
</p>


## Overview

TuyaOpen is an open source AI+IoT development framework designed for rapid creation of intelligent, connected devices. It supports multiple chip platforms and RTOS-like operating systems, enabling seamless integration of multimodal AI capabilities—including audio, video, and sensor data processing.

### 🚀 With TuyaOpen, you can:
- Develop hardware products featuring speech technologies such as `ASR` (Automatic Speech Recognition), `KWS` (Keyword Spotting), `TTS` (Text-to-Speech), and `STT` (Speech-to-Text)
- Integrate with leading LLMs and AI platforms, including `Deepseek`, `ChatGPT`, `Claude`, `Gemini`, and more.
- Build smart devices with `advanced multimodal AI capabilities`, including voice, vision, and sensor-based features
- Create custom products and seamlessly connect them to Tuya Cloud for `remote control`, `monitoring`, and `OTA updates`
- Develop devices compatible with `Google Home` and `Amazon Alexa`
- Design custom `Powered by Tuya` hardware
- Target a wide range of hardware applications using `Bluetooth`, `Wi-Fi`, `Ethernet`, and more
- Benefit from robust built-in `security`, `device authentication`, and `data encryption`


Whether you’re creating smart home products, industrial IoT solutions, or custom AI applications, TuyaOpen provides the tools and examples to get started quickly and scale your ideas across platforms.


### TuyaOpen SDK Framework
<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/25713212-9840-4cf5-889c-6f55476a59f9.jpg" width="80%" >
</p>

---


### Supported Target Platforms
| Name                  | Support Status | Introduction                                                 | Debug log serial port |
| --------------------- | -------------- | ------------------------------------------------------------ | --------------------- |
| Ubuntu                | Supported      | Can be run directly on Linux hosts such as ubuntu.           |                       |
| Tuya T2                    | Supported      | Supported Module List: [T2-U](https://developer.tuya.com/en/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq) | Uart2/115200          |
| Tuya T3                    | Supported      | Supported Module List: [T3-U](https://developer.tuya.com/en/docs/iot/T3-U-Module-Datasheet?id=Kdd4pzscwf0il) [T3-U-IPEX](https://developer.tuya.com/en/docs/iot/T3-U-IPEX-Module-Datasheet?id=Kdn8r7wgc24pt) [T3-2S](https://developer.tuya.com/en/docs/iot/T3-2S-Module-Datasheet?id=Ke4h1uh9ect1s) [T3-3S](https://developer.tuya.com/en/docs/iot/T3-3S-Module-Datasheet?id=Kdhkyow9fuplc) [T3-E2](https://developer.tuya.com/en/docs/iot/T3-E2-Module-Datasheet?id=Kdirs4kx3uotg) etc. | Uart1/460800          |
| Tuya T5                  | Supported      | Supported Module List: [T5-E1](https://developer.tuya.com/en/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi) [T5-E1-IPEX](https://developer.tuya.com/en/docs/iot/T5-E1-IPEX-Module-Datasheet?id=Kdskxvxe835tq) etc. | Uart1/460800          |
| ESP32/ESP32C3/ESP32S3 | Supported      |                                                              | Uart0/115200          |
| LN882H                | Supported      |                                                              | Uart1/921600          |
| BK7231N               | Supported      | Supported Module List:  [CBU](https://developer.tuya.com/en/docs/iot/cbu-module-datasheet?id=Ka07pykl5dk4u)  [CB3S](https://developer.tuya.com/en/docs/iot/cb3s?id=Kai94mec0s076) [CB3L](https://developer.tuya.com/en/docs/iot/cb3l-module-datasheet?id=Kai51ngmrh3qm) [CB3SE](https://developer.tuya.com/en/docs/iot/CB3SE-Module-Datasheet?id=Kanoiluul7nl2) [CB2S](https://developer.tuya.com/en/docs/iot/cb2s-module-datasheet?id=Kafgfsa2aaypq) [CB2L](https://developer.tuya.com/en/docs/iot/cb2l-module-datasheet?id=Kai2eku1m3pyl) [CB1S](https://developer.tuya.com/en/docs/iot/cb1s-module-datasheet?id=Kaij1abmwyjq2) [CBLC5](https://developer.tuya.com/en/docs/iot/cblc5-module-datasheet?id=Ka07iqyusq1wm) [CBLC9](https://developer.tuya.com/en/docs/iot/cblc9-module-datasheet?id=Ka42cqnj9r0i5) [CB8P](https://developer.tuya.com/en/docs/iot/cb8p-module-datasheet?id=Kahvig14r1yk9) etc. | Uart2/115200          |

# Documentation

For more TuyaOpen-related documentation, please refer to the [TuyaOpen Developer Guide](https://tuyaopen.ai/docs/about-tuyaopen).

## License

Distributed under the Apache License Version 2.0. For more information, see `LICENSE`.

## Contribute Code

If you are interested in the TuyaOpen and wish to contribute to its development and become a code contributor, please first read the [Contribution Guide](https://tuyaopen.ai/docs/contribute/contribute-guide).

## Disclaimer and Liability Clause

Users should be clearly aware that this project may contain submodules developed by third parties. These submodules may be updated independently of this project. Considering that the frequency of updates for these submodules is uncontrollable, this project cannot guarantee that these submodules are always the latest version. Therefore, if users encounter problems related to submodules when using this project, it is recommended to update them as needed or submit an issue to this project.

If users decide to use this project for commercial purposes, they should fully recognize the potential functional and security risks involved. In this case, users should bear all responsibility for any functional and security issues, perform comprehensive functional and safety tests to ensure that it meets specific business needs. Our company does not accept any liability for direct, indirect, special, incidental, or punitive damages caused by the user's use of this project or its submodules.

## Related Links

- Arduino for TuyaOpen: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode for tuyaopen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)
