示例工程
===========================

TuyaOpen 提供了丰富的示例工程，方便开发者快速上手，了解 TuyaOpen 的使用。

.. code-block:: bash

    $ examples
    ├── ble
    │   ├── ble_central
    │   └── ble_peripher
    ├── get-started
    │   └── sample_project
    ├── peripherals
    │   ├── adc
    │   ├── gpio
    │   ├── i2c
    │   ├── pwm
    │   ├── spi
    │   ├── timer
    │   └── watchdog
    ├── protocols
    │   ├── http_client
    │   ├── https_client
    │   ├── mqtt
    │   ├── tcp_client
    │   └── tcp_server
    ├── system
    │   ├── os_event
    │   ├── os_kv
    │   ├── os_mutex
    │   ├── os_queue
    │   ├── os_semaphore
    │   ├── os_sw_timer
    │   └── os_thread
    └── wifi
        ├── ap
        ├── low_power
        ├── scan
        └── sta

========================
选择待编译项目
========================

example 编译方式与 app 编译方式一致，使用 `tos config_choice` 命令选择编译目标平台或目标板，然后使用 `tos build` 编译。


每个示例工程下对应有 README.md 文件，详细介绍了示例工程的配置、编译、运行等操作。

========================
编译示例
========================
1. 运行 `tos config_choice` 命令， 选择当前运行的开发板或 platform。
2. 如需修改配置，请先运行 `tos menuconfig` 命令修改配置。
3. 运行 `tos build` 命令，编译工程。