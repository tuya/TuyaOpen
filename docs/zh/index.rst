.. TuyaOpen 开发指南 documentation master file, created by
   sphinx-quickstart on Fri Nov 22 16:36:54 2024.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

TuyaOpen 开发指南
===============================


.. figure:: ../images/TuyaOpen.png

TuyaOpen 是一款跨芯片平台、操作系统的 AI+IoT 开发框架。它基于通用南向接口设计，支持 Bluetooth、Wi-Fi、Ethernet 等通信协议，提供了物联网开发的核心功能，包括配网，激活，控制，升级等；它具备强大的安全合规能力，包括设备认证、数据加密、通信加密等，满足全球各个国家和地区的数据合规需求。

基于 TuyaOpen 开发的 AI+IoT 产品，如果使用 tuya_cloud_service 组件的功能，就可以使用涂鸦APP、云服务提供的强大生态能力，并与 Power By Tuya 设备互联互通。

同时 TuyaOpen 将不断拓展，提供更多云平台接入功能，及语音、视频、人脸识别等功能。

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

如果您对 TuyaOpen 感兴趣，并希望参与 TuyaOpen 的开发并成为代码贡献者，请先参阅 `贡献指南 <https://github.com/tuya/TuyaOpen/blob/master/docs/zh/contribute_guide.md>`_。

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

.. toctree::
   :maxdepth: 1
   :glob:

   quick_start.md
   tos_guide.md
   applications/application.rst
   peripherals/peripherals.rst
   new_platform.md
   new_board.md
   contribute_guide.md
   code_style_guide.md