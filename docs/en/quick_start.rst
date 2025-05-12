Quick Start
===========

Install Dependencies
--------------------

Ubuntu and Debian
^^^^^^^^^^^^^^^^^

.. code-block:: bash

    sudo apt-get install lcov cmake-curses-gui build-essential ninja-build wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev

macOS
^^^^^
Run::

    tos check

Check system dependencies and install missing components as prompted.

.. note::
    Since version v1.1.0, we have adopted ninja as the build tool to accelerate compilation speed. If you encounter compilation errors, please install ninja.

Clone Repository
----------------

.. code-block:: bash

    git clone https://github.com/tuya/TuyaOpen.git

The tuyeopen repository contains multiple submodules. The tos tool will automatically check and download submodules before compilation. Alternatively, you can manually download them using::

    git submodule update --init

Setup and Compilation
---------------------

step1. Set Environment Variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code-block:: bash

    cd TuyaOpen
    export PATH=$PATH:$PWD

Or add the TuyaOpen path to your system environment variables.

TuyaOpen uses the tos command for compilation and debugging operations. The tos command locates the TuyaOpen repository through the path set in environment variables.

For detailed usage of tos command, please refer to :ref:`tos command <tos_guide>`.

step2. Select Project to Compile
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- **Method 1**: Compile Example

To select an example for compilation, use the command::

    tos set_example

This will modify the ``examples`` directory according to the selected platform.

For more example information, see :ref:`Example Projects <examples>`.

- **Method 2**: Compile App

Select an app to compile, such as `apps/tuya_cloud/switch_demo <https://github.com/tuya/TuyaOpen/tree/master/apps/tuya_cloud/switch_demo>`, and navigate to its directory.

Use::

    tos config_choice

to select target platform or development board.

.. code-block:: bash

    [TuyaOpen/apps/tuya_cloud/switch_demo/config] is empty.
    Using boards default config file.
    ========================
    Configs
      1. BK7231X.config
      2. ESP32-C3.config
      3. ESP32.config
      4. ESP32-S3.config
      5. LN882H.config
      6. T2.config
      7. T3.config
      8. T5AI.config
      9. Ubuntu.config
    ------------------------
    Please select: 

The ``tos config_choice`` command reads configuration files from the project's ``config`` directory and generates the current project's configuration file ``app_default.config``.

.. important::
    After switching config with ``tos config_choice``, the tos command will automatically clear compiled intermediate files in the current project.

step3. Compile
^^^^^^^^^^^^^^
Navigate to the target example or app directory and run:

.. code-block:: bash

    cd apps/tuya_cloud/switch_demo
    tos build

Compiled binaries will be located in ``.build/<project>/bin`` directory, e.g. ``apps/tuya_cloud/switch_demo/.build/bin``.

The compiled files include:

- switch_demo_QIO_1.0.0.bin: Complete firmware including bootloader for flashing
- switch_demo_UA_1.0.0.bin: Application firmware without bootloader (requires platform/chip-specific address flashing)
- switch_demo_UG_1.0.0.bin: OTA upgrade file (cannot run directly after flashing)

Project name defaults to directory name, and version defaults to ``1.0.0``. These can be modified via ``tos menuconfig``.

step4. menuconfig Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
To modify project configuration, navigate to the target directory and run:

.. code-block:: bash

    cd apps/tuya_cloud/switch_demo
    tos menuconfig

Configure the project, save changes, and recompile.

.. important::
    Switching chips or development boards via ``tos menuconfig`` will automatically clear compiled intermediate files.

Flashing
--------

GUI Tool Flashing
^^^^^^^^^^^^^^^^^
tyutool gui supports serial flashing for multiple chips (T2/T3/T5AI/BK7231N/LN882H/ESP32) and works on Windows/Linux/macOS:

- Windows：`tyutool_win <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.zip>`_
- Linux：`tyutool_linux.tar <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz>`_
- macOS_x86：`tyutool_mac_x86 <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz>`_
- macOS_arm64：`tyutool_mac_arm64.zip <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz>`_

Command Line Flashing
^^^^^^^^^^^^^^^^^^^^^
Use ``tos flash`` for one-step flashing:

1. On Linux, set serial port permissions first:

.. code-block:: bash

    sudo usermod -aG dialout $USER

System reboot required after configuration.

2. Run flashing command in compiled project directory:

.. code-block:: bash

    cd apps/tuya_cloud/switch_demo
    tos flash
    tyutool params:
    [INFO]: tyut_logger init done.
    [INFO]: Run Tuya Uart Tool.
    [INFO]: Use default baudrate: [921600]
    [INFO]: Use default start address: [0x00]
    --------------------
    1. /dev/ttyS0
    2. /dev/ttyACM0
    3. /dev/ttyACM1
    ^^^^^^^^^^^^^^^^^^^^
    Select serial port: 3
    [INFO]: Waiting Reset ...
    [INFO]: unprotect flash OK.
    [INFO]: sync baudrate 921600 success
    Erasing: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100% 4 bytes/s   0:00:04 / 0:00:00
    [INFO]: Erase flash success
    Writing: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100% 16 bytes/s   0:00:18 / 0:00:00
    [INFO]: Write flash success
    [INFO]: CRC check success
    [INFO]: Reboot done
    [INFO]: Flash write success.

.. attention::
    The chip needs to enter boot mode before flashing via serial port.
    If serial port is unresponsive, check port selection or port occupancy.

3. For versions prior to v1.8.0 (no auto-update), manually upgrade with:

.. code-block:: bash

    tos flash upgrade

Check version with:

.. code-block:: bash

    tos flash --version

Output example:

.. code-block:: bash

    tyuTool, version 1.8.3