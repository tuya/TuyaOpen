快速上手
========

安装依赖
--------

Ubuntu and Debian
^^^^^^^^^^^^^^^^^^

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

tuyeopen 仓库中包含多个子模块，tos 工具会在 **编译前检查并自动下载** 子模块，也可以使用::

    git submodule update --init

命令手工下载。

设置与编译
----------

step1. 设置环境变量
^^^^^^^^^^^^^^^^^^^
.. code-block:: bash

    cd TuyaOpen
    export PATH=$PATH:$PWD

或将 TuyaOpen 路径添加到系统环境变量中

.. code-block:: bash

    vim ~/.bashrc
    # 添加以下内容
    export PATH=$PATH:/path/to/your/TuyaOpen

.. attention::
    请将 ``/path/to/your/TuyaOpen`` 替换为实际的 TuyaOpen 目录路径。

使用 vim 添加环境变量后，输入 ``:wq`` 保存，使用 ``source ~/.bashrc`` 命令使环境变量生效。
tos 命令的详细使用方法，请参考 :doc:`tos 命令 </tos_guide/index>`。

.. note:: 
    TuyaOpen 通过 tos 命令进行编译、调试等操作，tos 命令会根据环境变量中设置的路径查找 TuyaOpen 仓库，并执行对应操作。

step2. 选择待编译项目
^^^^^^^^^^^^^^^^^^^^^

选择待编译 app 或 example，如 `apps/tuya_cloud/switch_demo <https://github.com/tuya/TuyaOpen/tree/master/apps/tuya_cloud/switch_demo>`_，并切换至对应目录。

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

更多 example 信息点击 :doc:`示例工程章节 </examples/index>` 。

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

烧录与授权
-----------

命令行烧录
^^^^^^^^^^
支持 ``tos flash`` 命令一键烧录：:doc:`CLI 烧录 <cli-flash>`

GUI 工具烧录
^^^^^^^^^^^^
``tyutool gui`` 提供完整的图形化烧录解决方案，界面采用模块化设计，集成串口调试、固件烧录、授权管理等功能模块。
目前已支持 T2/T3/T5AI/BK7231N/LN882H/ESP32 等多种芯片串口烧录，支持 windows/Linux/macOS 等操作系统，请根据运行操作系统选择对应的 GUI 烧录工具。

- windows：`tyutool_win <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.zip>`_
- Linux：`tyutool_linux.tar <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz>`_
- macOS_x86：`tyutool_mac_x86 <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz>`_
- macOS_arm64：`tyutool_mac_arm64.zip <https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz>`_

GUI 烧录教程点击 :doc:`GUI 烧录 <gui-flash>` 

.. toctree::
    :maxdepth: 1
    :glob:

    cli-flash
    gui-flash

