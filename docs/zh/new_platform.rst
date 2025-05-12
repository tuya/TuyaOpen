
########################
platform 新增与适配
########################

简介
========

`TuyaOpen <https://github.com/tuya/TuyaOpen>`_ 采用 ``platform_config.yaml`` 文件管理 platform。``platform/platform_config.yaml`` 文件包含了已经实现了通用接口的 platform 仓库信息。可在项目目录下的 ``project_build.ini`` 文件中配置 ``platform`` 及 ``chip``(如当前 platform 支持多个芯片，则需配置芯片信息)，运行 tos 编译后，目标 platform 会自动下载对应的 platform 仓库，关联到 ``TuyaOpen`` 项目中。

如果你希望将``TuyaOpen``移植到自己的芯片上，则需要进行移植适配。

移植适配 platform
====================

生成新 platform 目录
--------------------

1. 下载并进入``TuyaOpen`` 目录，并设置环境变量：

.. code-block:: bash

    $ cd TuyaOpen
    $ export PATH=$PATH:$PWD

或将 TuyaOpen 路径添加到系统环境变量中。

TuyaOpen 通过 tos 命令进行编译、调试等操作，tos 命令会根据环境变量中设置的路径查找 TuyaOpen 仓库，并执行对应操作。

tos 命令的详细使用方法，请参考 :doc:`tos 命令 <tos_guide>`。

2. 通过以下命令，生成一个新的 ``platform`` 子目录：

.. code-block:: bash

    $ tos new_platform <new-platform-name>

命令在创建新 ``platform`` 时，会自动弹出一个 ``menuconfig`` 对话框

.. code-block:: shell

    $ tos new_platform <new-platform-name>
    (1.0.0) PROJECT_VERSION (NEW)
        configure TuyaOpen  --->
            configure tuya cloud service  --->
            configure enable/disable liblwip  --->
            configure enable/disable libtflm  --->
            configure mbedtls  --->
            configure system parameter  --->
        configure board <your-board-name>  --->
            OPERATING_SYSTEM (RTOS)  --->
            ENDIAN (LITTLE_ENDIAN)  --->
            [ ] ENABLE_FILE_SYSTEM --- support filesystem (NEW)
            [ ] ENABLE_WIFI --- support wifi (NEW)
            [*] ENABLE_WIRED --- support wired (NEW)
            [ ] ENABLE_BLUETOOTH --- support BLE (NEW)  ----
            [ ] ENABLE_RTC --- support rtc (NEW)
            [ ] ENABLE_WATCHDOG --- support watchdog (NEW)
            [*] ENABLE_UART --- support uart (NEW)
            [*] ENABLE_FLASH --- support flash (NEW)  --->
            [ ] ENABLE_ADC --- support adc (NEW)
            [ ] ENABLE_PWM --- support pwm (NEW)
            [*] ENABLE_GPIO --- support gpio (NEW)
            [ ] ENABLE_I2C --- support i2c (NEW)
            [ ] ENABLE_SPI --- support spi (NEW)
            [ ] ENABLE_TIMER --- support hw timer (NEW)
            [ ] ENABLE_DISPLAY --- support GUI display (NEW)
            [ ] ENABLE_MEDIA --- support media (NEW)
            [ ] ENABLE_PM --- support power manager (NEW)
            [ ] ENABLE_STORAGE --- support storage such as SDCard (NEW)
            [ ] ENABLE_DAC --- support dac (NEW)
            [ ] ENABLE_I2S --- support i2s (NEW)
            [ ] ENABLE_WAKEUP --- support lowpower wakeup (NEW)
            [ ] ENABLE_REGISTER --- support register (NEW)
            [ ] ENABLE_PINMUX --- support pinmux (NEW)
            [ ] ENABLE_PLATFORM_AES --- support hw AES (NEW)
            [ ] ENABLE_PLATFORM_SHA256 --- support hw sha256 (NEW)
            [ ] ENABLE_PLATFORM_MD5 --- support hw md5 (NEW)
            [ ] ENABLE_PLATFORM_SHA1 --- support hw sha1 (NEW)
            [ ] ENABLE_PLATFORM_RSA --- support hw rsa (NEW)
            [ ] ENABLE_PLATFORM_ECC --- support hw ecc (NEW)

- 在 ``TuyaOpen`` 中根据新添加 platform 支持软件功能配置进行 TuyaOpen 默认软件功能配置
- 在 ``configure board <your-board-name>`` 中根据新添加 platform 支持的硬件功能进行默认硬件功能配置

在完成配置之后，保存配置【快捷键``S``】、退出【快捷键``Q``】，将自动生成默认的功能配置 ``default.config``

3. 生成新 platform 目录

``tos new_platform`` 命令会根据配置生成新 platform 目录，并根据 ``menuconfig`` 中配置自动生成 ``tkl`` 接口层相关代码。

过程中有如下日志输出:

.. code-block:: text

    ..............
        make ability: system
            new file: tkl_sleep.c
            new file: tkl_memory.c
            new file: tkl_output.c
            new file: tkl_semaphore.c
            new file: tkl_queue.c
            new file: tkl_system.c
            new file: tkl_fs.c
            new file: tkl_ota.c
            new file: tkl_thread.c
            new file: tkl_mutex.c
        make ability: uart
            new file: tkl_uart.c
        make ability: security
    generate code finished!

看到 ``generate code finished!`` 就说明模版已生成成功。

.. note::

    配置功能不同，日志会稍有差异，供参考

完成生成之后，会在 platform 下得到一个如下的新目录：

.. code-block:: bash

    - platform
        + t2
        + ubuntu
        - <new-platform-name>
            + <new-platform-sdk>       # 需要自行创建，存放芯片相关 SDK
            + toolchain                # 需要自行创建，存放工具链
            + tuyaos                   # 存放 tuyaos 适配层
                - tuyaos_adapter       # 存放 tuyaos 适配层接口源代码
                    - include
                    - src        
            - Kconfig                   # 期望支持的可配置项，可以修改
            - default.config            # platform 默认支持的功能，可以修改
            - platform_config.cmake     # 适配层源码路径
            - toolchain_file.cmake      # TuyaOpen 编译工具路径和编译选项
            - build_example.sh          # 编译脚本  

完成编译适配
================

完成模版生成之后，可以按照需要对模版进行一些修改，并完成一些脚本的实现，确保编译流程能够正常进行。

Kconfig
----------

Kconfig 文件中定义了新 platform 支持的功能，请根据当前新增 platform 名称，修改首次生成的 Kconfig 文件中 ``menu`` 中的 ``<your-board-name>`` 名称。同事也可以根据实际硬件情况新增/删除相关配置。

.. code-block:: bash

    menu "configure board <your-board-name>"
        ...

        endmenu

platform_config.cmake
-------------------------

该文件为 TuyaOpen 编译依赖 platform 相关适配文件及头文件路径，一般不需要修改。

.. code-block:: bash

    list_subdirectories(PLATFORM_PUBINC ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)

TuyaOpen 采用 cmake 编译，``list_subdirectories`` 函数会将指定目录下所有子目录添加到 PLATFORM_PUBINC 变量中，供 TuyaOpen 编译时使用。

如需要添加其他路径，可参考如下方式：

.. code-block:: bash

    list_subdirectories(PLATFORM_PUBINC_1 ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)
    set(PLATFORM_PUBINC_2 
        ${PLATFORM_PATH}/TuyaOpen/build/config
    )

    set(PLATFORM_PUBINC 
        ${PLATFORM_PUBINC_1}
        ${PLATFORM_PUBINC_2})

toolchain_file.cmake
-----------------------

1. 设置编译工具路径及 toolchain 前缀，可以参考现有的 ``t2`` 和 ``ubuntu`` 目录下的文件。

.. code-block:: bash

    set(TOOLCHAIN_DIR "${PLATFORM_PATH}/toolchain/<your-toolchain-name>")
    set(TOOLCHAIN_PRE "<your-toolchain-prefix>")

2. 根据新增 platform 编译参数，正确设置 TuyaOpen 编译选项。

.. code-block:: bash

    set(CMAKE_C_FLAGS "<your-compiler-c-flags>")

3. build_example.sh
需要在这里完成原厂 SDK 的编译以及链接生成各种用途的固件，可以参考现有的 ``t2`` 和 ``ubuntu`` 目录下的文件。（注意，注释掉的那行，在``linux``系统下可以直接打开，配合默认的``Makefile``可以直接编译可执行文件。）

修改platform_config.yaml
---------------------------

在 ``platform/platform_config.yaml`` 文件中新增一个 ``<new-platform-name>``。如当前项目是本地项目，可无需填写``repo``和``commit``，如该项目通过 ``git`` 管理，则可以按实际情况填写

格式如下：

.. code-block:: bash

    - name: t3
      repo: https://github.com/tuya/TuyaOpen-platform-t3
      commit: master

如新增 platform 支持多 chip ，则可按如下格式填写：

.. code-block:: bash

    - name: new_platform
      repo: https://github.com/xxxx/new_platform
      commit: master
      chip: 
        - chip1
        - chip2
        - chip3

编译
--------

按照 :ref:`README_zh <../README_zh.md>` 文档的指引，在待编译测试的项目中 project_build.ini 修改/新增当前 platform 的名称，并进行一次编译。

.. code-block:: bash

    [project:sample_project_<new-platform-name>]
    platform = <new-platform-name>

推荐 project 名为 ``当前项目名_platform名`` 或 ``当前项目名_chip名``。

.. code-block:: bash

    $ cd examples/get-started/sample_project
    $ tos build

正常情况下，完成编译适配后，可直接编译通过。未编译成功请根据错误提示进行适配修改。

修改 default.config 默认值
------------------------------

default.config 是生成移植模版的时候，根据选择生成的默认配置，用于项目首次编译默认配置。

default.config 和 Kconfig 一起配合组成 TuyaOpen 菜单化配置功能，并自动生成 ``using.config`` 、``using.cmake``、 ``tuya_kconfig.h`` 文件，在 TuyaOpen 编译和源代码中被使用。

如需修改默认配置，请在对应项目目录下执行 ``make menuconfig`` 修改功能配置，并将相关修改手工合并至 default.config。

完成接口适配
================

生成移植模版的时候，在 ``tuyaos/tuyaos_adapter/src/`` 目录下生成了一些空函数，需要对这些空函数进行实现，才能保证功能正常运行。

因为``TuyaOpen`` 使用的和 ``TuyaOS`` 完全一致的底层接口，可以按照 `TuyaOS移植指南 <https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_linux?id=Kcrwrf72ciez5#title-1-适配-RTC>`_ 和 `RTOS移植指南 <https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_rtos?id=Kcrwraf21847l#title-1-适配程序入口>`_ 进行适配。

具体接口的介绍请参考 ``TuyaOpen`` 目录 ``./tools/porting/docs/`` 目录下的文档。

.. warning::

    外设的接口不是必须要适配的，开发者完全可以使用原厂的接口；为方便开发者使用更多 TuyaOpen 相关的功能，建议适配。

.. note::

    网络相关的接口，如果是外接网卡结构的，需要自行实现网卡驱动，实现 socket 相关操作以及相关 ``wifi``、``wired`` 操作

示例程序
============

完成适配之后，可以结合示例程序进行编译、调试。``TuyaOpen`` 提供了丰富的示例程序：

.. code-block:: text

    TuyaOpen
    ├── ai
    │   └── llm_demo
    │   └── tflm
    │       └── hello_world
    │       └── micro_speech
    │       └── person_detection
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

测试功能
============

请按照以下测试用例测试，同时根据自己产品的功能进行全覆盖的功能、性能测试：

`测试用例文档 <https://drive.weixin.qq.com/s?k=AGQAugfWAAkb5lIvFsAEgAwQZJALE>`_

提交
========

完成适配并通过测试后，欢迎提交 Push Requests 提交代码，将新适配的 platform 对应的 ``platform_config.yaml`` 提交至 `TuyaOpen仓库 <https://github.com/tuya/TuyaOpen>`_：

- Push Requests 流程可参考 :doc:`贡献指南 <contribute_guide>`
- 编程规范可参考 :doc:`编码风格指南 <code_style_guide>`