.. TuyaOpen Development Guide documentation master file, created by
   sphinx-quickstart on Fri Nov 22 16:36:54 2024.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

TuyaOpen Development Guide
===============================


.. figure:: ../images/TuyaOpen.png

TuyaOpen is an open source AI+IoT development framework that supports cross-chip platforms and operating systems. It is designed based on a universal southbound interface and supports communication protocols such as Bluetooth, Wi-Fi, and Ethernet. It provides core functionalities for AI+IoT development, including pairing, activation, control, and upgrading.
The sdk has robust security and compliance capabilities, including device authentication, data encryption, and communication encryption, meeting data compliance requirements in various countries and regions worldwide.

AI+IoT products developed using the TuyaOpen, if utilizing the functionality of the tuya_cloud_service component, can make use of the powerful ecosystem provided by the Tuya APP and cloud services, and achieve interoperability with Power By Tuya devices.

At the same time, the TuyaOpen will continuously expand, providing more cloud platform integration features, as well as voice, video, and facial recognition capabilities.

====================================
Supported Platform List
====================================
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
Contribute Code
========================

If you are interested in the TuyaOpen and wish to contribute to its development and become a code contributor, please first read the `Contribution Guide <https://github.com/tuya/TuyaOpen/blob/master/docs/en/contribute_guide.md>`_.


====================================
Related Links of TuyaOpen
====================================

- C for TuyaOpen: `https://github.com/tuya/TuyaOpen <https://github.com/tuya/TuyaOpen>`_
- Arduino for TuyaOpen: `https://github.com/tuya/arduino-TuyaOpen <https://github.com/tuya/arduino-TuyaOpen>`_
- Luanode for TuyaOpen: `https://github.com/tuya/luanode-TuyaOpen <https://github.com/tuya/luanode-TuyaOpen>`_

----------------
gitee Mirrors
----------------
- C for TuyaOpen: `https://gitee.com/tuya-open/TuyaOpen <https://gitee.com/tuya-open/TuyaOpen>`_
- Arduino for TuyaOpen: `https://gitee.com/tuya-open/arduino-TuyaOpen <https://gitee.com/tuya-open/arduino-TuyaOpen>`_
- Luanode for TuyaOpen: `https://gitee.com/tuya-open/luanode-TuyaOpen <https://gitee.com/tuya-open/luanode-TuyaOpen>`_

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