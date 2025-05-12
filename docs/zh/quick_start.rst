快速上手
========

安装依赖
--------

Ubuntu and Debian
^^^^^^^^^^^^^^^^^

.. code-block:: bash

    sudo apt-get install lcov cmake-curses-gui build-essential ninja-build wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev

macOS
^^^^^
请运行::

    tos check

检查系统依赖，并根据提示安装依赖。

.. note::
    v1.1.0 版本之后，我们采用了 ninja 作为构建工具来加快编译速度，如遇到编译错误请安装 ninja。

克隆仓库
--------

.. code-block:: bash

    git clone https://github.com/tuya/TuyaOpen.git

tuyeopen 仓库中包含多个子模块，tos 工具会在编译前检查并自动下载子模块，也可以使用::

    git submodule update --init

命令手工下载。

设置与编译
----------

step1. 设置环境变量
^^^^^^^^^^^^^^^^^^^
.. code-block:: bash

    cd TuyaOpen
    export PATH=$PATH:$PWD

或将 TuyaOpen 路径添加到系统环境变量中。

TuyaOpen 通过 tos 命令进行编译、调试等操作，tos 命令会根据环境变量中设置的路径查找 TuyaOpen 仓库，并执行对应操作。

tos 命令的详细使用方法，请参考 :ref:`tos 命令 <tos_guide>`。

step2. 选择待编译项目
^^^^^^^^^^^^^^^^^^^^^
- **方式1**：编译 example

选择待编译 example，可使用命令::

    tos set_example

根据平台完成选择，目录 ``examples`` 会修改为对应平台的示例。

更多 example 信息点击 :ref:`示例工程章节 <examples>` 。

- **方式2**：编译 app

选择待编译 app，如 `apps/tuya_cloud/switch_demo <https://github.com/tuya/TuyaOpen/tree/master/apps/tuya_cloud/switch_demo>`_，并切换至对应目录。

使用::

    tos config_choice

命令选择编译目标平台或目标板。

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

``tos config_choice`` 命令读取项目下 ``config`` 目录中的配置文件，并会生成当前工程的配置文件 ``app_default.config``。

.. important::
    运行 ``tos config_choice`` 切换 config 后，tos 命令会自动清除当前工程下已经编译生成的编译中间文件

step3. 编译
^^^^^^^^^^^
选择当前编译的 examples 或 apps 对应工程，运行如下命令编译：

.. code-block:: bash

    cd apps/tuya_cloud/switch_demo
    tos build

编译完成后目标文件位于当前编译项目 ``.build/<project>/bin`` 目录下，如 ``apps/tuya_cloud/switch_demo/.build/bin`` 目录。

编译后的目标文件包括：

- switch_demo_QIO_1.0.0.bin：包括 boot 在内的完整固件，用于烧录。
- switch_demo_UA_1.0.0.bin：未包括 boot 的应用固件，使用该文件需根据不同的 platform/chip 烧录该 bin 至对应的地址，否则可能无法正常运行。
- switch_demo_UG_1.0.0.bin：用于 OTA 升级的 bin 文件，无法直接烧录后运行。

项目名称默认为目录名称，项目版本默认为 ``1.0.0``，可通过 ``tos menuconfig`` 配置中修改。

step4. menuconfig 配置
^^^^^^^^^^^^^^^^^^^^^^
如需要修改项目的配置，选择需配置的 examples 或 apps 对应工程，在对应工程目录下运行如下命令进行菜单化配置：

.. code-block:: bash

    cd apps/tuya_cloud/switch_demo
    tos menuconfig

配置当前工程，配置完成后保存退出，编译工程。

.. important::
    运行 ``tos menuconfig`` 切换芯片或开发板后，tos 命令会自动清除当前工程下已经编译生成的编译中间文件

烧录
----

GUI 工具烧录
^^^^^^^^^^^^
tyutool gui 烧录工具已支持 T2/T3/T5AI/BK7231N/LN882H/ESP32 等多种芯片串口烧录，支持 windows/Linux/macOS 等操作系统，请根据运行操作系统选择对应的 GUI 烧录工具。

- windows：`tyutool_win <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.zip>`_
- Linux：`tyutool_linux.tar <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz>`_
- macOS_x86：`tyutool_mac_x86 <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz>`_
- macOS_arm64：`tyutool_mac_arm64.zip <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz>`_

命令行烧录
^^^^^^^^^^
可通过 tos flash 命令一键烧录

1. 在 Linux 环境下需要先使用如下命令设置串口权限，否则运行会报错：

.. code-block:: bash

    sudo usermod -aG dialout $USER

设置完成后需重启系统方可生效。

2. 在需要编译完成后的项目中运行 ``tos flash`` 命令一键烧录，``tos flash`` 会根据当前运行的环境自动下载对应的 tyutool 工具，并自动烧录：

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
    烧录过程中需要根据芯片实际情况进入 boot 后才可以进行串口烧录。
    烧录过程中如果串口没有响应，请检查串口是否正确选择，或串口是否被其他程序占用。

3. tos flash 烧录工具正在不断新增支持新的芯片型号，v1.8.0 之前的版本不支持自动升级工具，后续版本在启动时会检测升级并提示升级。
可通过::

    tos flash --version

查询版本情况：

.. code-block:: bash

    tyuTool, version 1.8.3

v1.8.0 之前版本需要手工运行以下升级命令升级：

.. code-block:: bash

    tos flash upgrade