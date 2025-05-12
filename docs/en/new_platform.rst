
########################
Adding and Adapting Platforms
########################

Introduction
============

`TuyaOpen <https://github.com/tuya/TuyaOpen>`_ manages platforms using the ``platform_config.yaml`` file. The ``platform/platform_config.yaml`` file contains repository information of platforms that have implemented common interfaces. Configure ``platform`` and ``chip`` (if the platform supports multiple chips) in the project's ``project_build.ini`` file. After running tos compilation, the target platform will automatically download the corresponding platform repository and link it to the ``TuyaOpen`` project.

If you wish to port ``TuyaOpen`` to your own chip, platform adaptation is required.

Platform Adaptation
====================

Generate New Platform Directory
-------------------------------

1. Download and enter the ``TuyaOpen`` directory, then set environment variables:

.. code-block:: bash

    $ cd TuyaOpen
    $ export PATH=$PATH:$PWD

Or add the TuyaOpen path to your system environment variables.

TuyaOpen uses the tos command for compilation and debugging operations. The tos command locates the TuyaOpen repository through the path set in environment variables.

For detailed usage of the tos command, refer to :doc:`tos Guide <tos_guide>`.

2. Generate a new platform subdirectory:

.. code-block:: bash

    $ tos new_platform <new-platform-name>

This command will automatically launch a ``menuconfig`` dialog during platform creation:

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

- Configure default software features in ``TuyaOpen`` based on new platform capabilities
- Configure default hardware features in ``configure board <your-board-name>``

After configuration, save changes (shortcut ``S``) and exit (shortcut ``Q``) to generate the default configuration ``default.config``

3. The ``tos new_platform`` command generates the new platform directory and creates TKL interface layer code based on menuconfig selections.

Log output during generation:

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

The message ``generate code finished!`` indicates successful template generation.

.. note::

    Log details may vary slightly depending on configuration options

The generated directory structure:

.. code-block:: bash

    - platform
        + t2
        + ubuntu
        - <new-platform-name>
            + <new-platform-sdk>       # Create manually for chip SDK
            + toolchain                # Create manually for toolchain
            + tuyaos                   # TuyaOS adaptation layer
                - tuyaos_adapter       # Interface source code
                    - include
                    - src        
            - Kconfig                   # Configurable items
            - default.config            # Default platform configuration
            - platform_config.cmake     # Adaptation layer paths
            - toolchain_file.cmake      # Compilation tool paths
            - build_example.sh          # Build script

Completing Adaptation
=====================

Kconfig Configuration
---------------------
Modify the ``<your-board-name>`` in Kconfig to match your platform name:

.. code-block:: bash

    menu "configure board <your-board-name>"
        ...
        endmenu

platform_config.cmake
---------------------
This file defines adaptation layer paths. Normally no modification needed:

.. code-block:: bash

    list_subdirectories(PLATFORM_PUBINC ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)

toolchain_file.cmake
--------------------
1. Set toolchain paths:

.. code-block:: bash

    set(TOOLCHAIN_DIR "${PLATFORM_PATH}/toolchain/<your-toolchain-name>")
    set(TOOLCHAIN_PRE "<your-toolchain-prefix>")

2. Set compilation flags:

.. code-block:: bash

    set(CMAKE_C_FLAGS "<your-compiler-c-flags>")

3. Implement ``build_example.sh`` for SDK compilation and firmware generation.

Updating platform_config.yaml
-----------------------------
Add new platform entry:

.. code-block:: bash

    - name: t3
      repo: https://github.com/tuya/TuyaOpen-platform-t3
      commit: master

For multi-chip platforms:

.. code-block:: bash

    - name: new_platform
      repo: https://github.com/xxxx/new_platform
      commit: master
      chip: 
        - chip1
        - chip2
        - chip3

Compilation
-----------
Update ``project_build.ini``:

.. code-block:: bash

    [project:sample_project_<new-platform-name>]
    platform = <new-platform-name>

Then compile:

.. code-block:: bash

    $ cd examples/get-started/sample_project
    $ tos build

Interface Implementation
========================
Implement empty functions in ``tuyaos/tuyaos_adapter/src/``. Refer to:
- `TuyaOS Linux Porting Guide <https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_linux?id=Kcrwrf72ciez5#title-1-Adapt-RTC>`_
- `RTOS Porting Guide <https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_rtos?id=Kcrwraf21847l#title-1-Adapt-entry-point>`_

.. warning::
    Peripheral interfaces are not mandatory but recommended for full functionality

.. note::
    Implement network drivers for external NIC configurations

Example Projects
================
TuyaOpen provides various examples:

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
    └── wifi
        ├── ap
        ├── low_power
        ├── scan
        └── sta

Testing
========
Refer to test documentation for comprehensive validation:
`Test Case Documentation <https://drive.weixin.qq.com/s?k=AGQAugfWAAkb5lIvFsAEgAwQZJALE>`_

Submission
==========
Submit adapted platforms via Pull Requests:
- Process reference: :doc:`Contribution Guide <contribute_guide>`
- Coding standards: :doc:`Code Style Guide <code_style_guide>`