.. TuyaOpen 开发指南 documentation master file, created by
   sphinx-quickstart on Fri Nov 22 16:36:54 2024.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.


TuyaOpen 开发指南
===============================

.. raw:: html

    <p style="text-align: left;">
        <a class="reference external" href="https://docs.tuyaopen.io/en/master/index.html">
            [English]
        </a>
    </p>

.. _lvgl_landing_page:

.. figure:: ../images/TuyaOpen.png


TuyaOpen 是一个面向 AIoT 行业的开源、开放的开发框架，基于成熟的商业级 IoT 系统 TuyaOS 构建而成。它继承了跨平台、跨系统、组件化和安全合规等核心特性，并经过全球亿级设备和百万级用户的验证。

TuyaOpen 集成了端侧 AI 推理引擎，支持涂鸦云智能体中枢，支持端云融合的多模态 AI 能力。开发者可以无缝调用国内合规的大模型（如 DeepSeek、千问、豆包）或灵活对接全球顶尖的 AI 服务（如 ChatGPT、Claude、Gemini）。通过多样化的工具生态，开发者能够实现文字和语音对话、图片生成、视频生成等多种 AI 功能。

此外，TuyaOpen 支持行业内主流的开源软硬件生态，开发者可以轻松地将项目移植和部署到任意芯片或开发板上。这使得开发者能够快速体验 AI 技术带来的创新，并加速产品开发周期。

========================
支持 Platform 列表
========================
+------------------------+------------------------+
| Platform               | Support status         |
+========================+========================+
| Ubuntu                 | Supported              | 
+------------------------+------------------------+
| T2                     | Supported              |  
+------------------------+------------------------+
| T3                     | Supported              |  
+------------------------+------------------------+
| T5AI                   | Supported              |  
+------------------------+------------------------+
| ESP32/ESP32C3/ESP32S3  | Supported              |  
+------------------------+------------------------+
| LN882H                 | Supported              |  
+------------------------+------------------------+
| BK7231N                | Supported              |  
+------------------------+------------------------+

========================
贡献代码
========================

如果您对 TuyaOpen 感兴趣，并希望参与 TuyaOpen 的开发并成为代码贡献者，请先参阅 :doc:`贡献指南 </contribute_guide/index>`。

========================
TuyaOpen 相关链接
========================
- C 版 TuyaOpen: `https://github.com/tuya/TuyaOpen <https://github.com/tuya/TuyaOpen>`_
- Arduino 版 TuyaOpen: `https://github.com/tuya/arduino-TuyaOpen <https://github.com/tuya/arduino-TuyaOpen>`_
- Luanode 版 TuyaOpen: `https://github.com/tuya/luanode-TuyaOpen <https://github.com/tuya/luanode-TuyaOpen>`_

----------------
gitee 镜像
----------------
- C 版 TuyaOpen: `https://gitee.com/tuya-open/TuyaOpen <https://gitee.com/tuya-open/TuyaOpen>`_
- Arduino 版 TuyaOpen: `https://gitee.com/tuya-open/arduino-TuyaOpen <https://gitee.com/tuya-open/arduino-TuyaOpen>`_
- Luanode 版 TuyaOpen: `https://gitee.com/tuya-open/luanode-TuyaOpen <https://gitee.com/tuya-open/luanode-TuyaOpen>`_

目录

.. toctree::
   :maxdepth: 1
   :glob:

   quick_start/index
   tos_guide/index
   examples/index
   applications/index
   peripherals/index
   new_platform/index
   new_board/index
   contribute_guide/index
   code_style_guide/index
   faq/index
