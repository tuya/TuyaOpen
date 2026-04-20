<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/c128362b-eb25-4512-b5f2-ad14aae2395c.jpg" width="100%" >
</p>

<p align="center">
  <a href="https://tuyaopen.ai/zh/docs/quick-start/enviroment-setup">快速开始</a> ·
  <a href="https://developer.tuya.com/cn/docs/iot/ai-agent-management?id=Kdxr4v7uv4fud">涂鸦 AI Agent</a> ·
  <a href="https://tuyaopen.ai/zh/docs/about-tuyaopen">文档中心</a> ·
  <a href="https://tuyaopen.ai/zh/docs/hardware-specific/t5-ai-board/overview-t5-ai-board">硬件资源</a>
</p>

<p align="center">
    <a href="https://github.com/tuya/TuyaOpen/actions/workflows/check-build-apps.yml" target="_blank">
        <img src="https://github.com/tuya/TuyaOpen/actions/workflows/check-build-apps.yml/badge.svg"
            alt="TuyaOpen Check Build"></a>
    <a href="https://tuyaopen.ai" target="_blank">
        <img alt="Static Badge" src="https://img.shields.io/badge/Product-F04438"></a>
    <a href="https://tuyaopen.ai/zh/pricing" target="_blank">
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


## 概述
TuyaOpen 赋能下一代 AI 智能体硬件：以灵活跨平台 C/C++ SDK 支持 涂鸦T系列 WIFI/蓝牙芯片、树莓派、ESP32 等设备，搭配涂鸦云低延迟多模态 AI（拖拽工作流），集成顶尖模型，简化开放式 AI-IoT 生态搭建。

![TuyaOpen One Pager](https://images.tuyacn.com/fe-static/docs/img/207eb9a0-5583-4f04-a71d-1917cbefc5ba.png)

### 🚀 使用 TuyaOpen，你可以：
- 开发具备语音技术的硬件产品，如 `ASR`（Automatic Speech Recognition）、`KWS`（Keyword Spotting）、`TTS`（Text-to-Speech）、`STT`（Speech-to-Text）
- 集成主流 LLMs 及 AI 平台，包括 `Deepseek`、`ChatGPT`、`Claude`、`Gemini` 等
- 构建具备 `多模态AI能力` 的智能设备，包括文本、语音、视觉和基于传感器的功能
- 创建自定义产品，并无缝连接至涂鸦云，实现 `远程控制`、`监控` 和 `OTA 升级`
- 开发兼容 `Google Home` 和 `Amazon Alexa` 的设备
- 设计自定义的 `Powered by Tuya` 硬件
- 支持广泛的硬件应用，包括 `蓝牙`、`Wi-Fi`、`以太网` 等多种连接方式
- 受益于强大的内置 `安全性`、`设备认证` 和 `数据加密` 能力

无论你是在开发智能家居产品、工业 IoT 解决方案，还是定制 AI 应用，TuyaOpen 都能为你提供快速入门和跨平台扩展的工具与示例。



## 系统组成
<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/220c9d84-d5f1-4976-b910-b63e415e9e03.png" width="80%" >
</p>

### TuyaOpen SDK 详细框架栈
<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/25713212-9840-4cf5-889c-6f55476a59f9.jpg" width="80%" >
</p>

---

### 支持的目标平台
| Name                  | Support Status | Introduction                                                 | Debug log serial port |
| --------------------- | -------------- | ------------------------------------------------------------ | --------------------- |
| Ubuntu                | Supported      | 可直接运行于如 ubuntu 等 Linux 主机。                        |                       |
| Tuya T2               | Supported      | 支持的模块列表: [T2-U](https://developer.tuya.com/en/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq) | Uart2/115200          |
| Tuya T3               | Supported      | 支持的模块列表: [T3-U](https://developer.tuya.com/en/docs/iot/T3-U-Module-Datasheet?id=Kdd4pzscwf0il) [T3-U-IPEX](https://developer.tuya.com/en/docs/iot/T3-U-IPEX-Module-Datasheet?id=Kdn8r7wgc24pt) [T3-2S](https://developer.tuya.com/en/docs/iot/T3-2S-Module-Datasheet?id=Ke4h1uh9ect1s) [T3-3S](https://developer.tuya.com/en/docs/iot/T3-3S-Module-Datasheet?id=Kdhkyow9fuplc) [T3-E2](https://developer.tuya.com/en/docs/iot/T3-E2-Module-Datasheet?id=Kdirs4kx3uotg) 等 | Uart1/460800          |
| Tuya T5               | Supported      | 支持的模块列表: [T5-E1](https://developer.tuya.com/en/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi) [T5-E1-IPEX](https://developer.tuya.com/en/docs/iot/T5-E1-IPEX-Module-Datasheet?id=Kdskxvxe835tq) 等 | Uart1/460800          |
| ESP32/ESP32C3/ESP32S3 | Supported      |                                                              | Uart0/115200          |
| LN882H                | Supported      |                                                              | Uart1/921600          |
| BK7231N               | Supported      | 支持的模块列表: [CBU](https://developer.tuya.com/en/docs/iot/cbu-module-datasheet?id=Ka07pykl5dk4u) [CB3S](https://developer.tuya.com/en/docs/iot/cb3s?id=Kai94mec0s076) [CB3L](https://developer.tuya.com/en/docs/iot/cb3l-module-datasheet?id=Kai51ngmrh3qm) [CB3SE](https://developer.tuya.com/en/docs/iot/CB3SE-Module-Datasheet?id=Kanoiluul7nl2) [CB2S](https://developer.tuya.com/en/docs/iot/cb2s-module-datasheet?id=Kafgfsa2aaypq) [CB2L](https://developer.tuya.com/en/docs/iot/cb2l-module-datasheet?id=Kai2eku1m3pyl) [CB1S](https://developer.tuya.com/en/docs/iot/cb1s-module-datasheet?id=Kaij1abmwyjq2) [CBLC5](https://developer.tuya.com/en/docs/iot/cblc5-module-datasheet?id=Ka07iqyusq1wm) [CBLC9](https://developer.tuya.com/en/docs/iot/cblc9-module-datasheet?id=Ka42cqnj9r0i5) [CB8P](https://developer.tuya.com/en/docs/iot/cb8p-module-datasheet?id=Kahvig14r1yk9) 等 | Uart2/115200          |

# 开发者文档

更多 TuyaOpen 相关文档，请参考 [TuyaOpen 开发者指南](https://tuyaopen.ai/docs/about-tuyaopen)。

## 许可证

本项目基于 Apache License Version 2.0 发布。更多信息请参见 `LICENSE`。



## 代码贡献

如果你对 TuyaOpen 感兴趣，并希望参与开发成为代码贡献者，请先阅读 [贡献指南](https://tuyaopen.ai/docs/contribute/contribute-guide)。

## 免责声明

用户需明确知晓，本项目可能包含由第三方开发的子模块。这些子模块可能会独立于本项目进行更新。鉴于这些子模块的更新频率不可控，本项目无法保证其始终为最新版本。因此，若用户在使用本项目过程中遇到与子模块相关的问题，建议根据需要自行更新，或向本项目提交 issue。

如用户决定将本项目用于商业用途，应充分认识到其中可能存在的功能和安全风险。在此情况下，用户应对所有功能和安全问题自行承担责任，并进行全面的功能和安全性测试，以确保其满足特定业务需求。本公司不对因用户使用本项目或其子模块而导致的任何直接、间接、特殊、偶发或惩罚性损害承担责任。

## 相关链接

- Arduino for TuyaOpen: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode for TuyaOpen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)
- **TuyaOpen Dev Skills**（Cursor AI 开发技能：环境、编译、烧录、设备授权等）：[github.com/tuya/TuyaOpen-dev-skills](https://github.com/tuya/TuyaOpen-dev-skills) — 可在 Cursor 中以 Remote Rule 导入或克隆仓库：`https://github.com/tuya/TuyaOpen-dev-skills.git`
