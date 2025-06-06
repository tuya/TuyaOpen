.. _tos_guide:

#############
tos 使用指南
#############

tos 命令是 TuyaOpen 构建工具，支持创建、编译、配置等多种功能。

.. note::

    tos 命令位于 `TuyaOpen <https://github.com/tuya/TuyaOpen.git>`_ 根目录下，使用 shell 脚本实现，使用前先将 TuyaOpen 路径添加到系统环境变量中。


========
命令列表
========

可通过 `tos help` 查看完整命令列表。

.. code-block:: bash

    $ tos help

    Usage: tos COMMAND [ARGS]...

    Commands:
        version    - Show TOS verson
        check      - Check command and version
        new        - New project [base(default) / auduino]
        build      - Build project
        clean      - Clean project
        fullclean  - Clean project and delete build path
        menuconfig - Configuration project features
        savedef    - Saves minimal configuration file (app_default.config)
        new_platform - New platform [platform_name]
        update     - Update the platforms according to the platform_config.yaml
        help       - Help information

==========
使用示例
==========

查看版本
----------

.. code-block:: bash

    $ tos version
    2.0.0

环境检测
--------

.. code-block:: bash

    $ tos check
    Check command and version ...
    Check [bash](5.2.21) > [4.0.0]: OK.
    Check [grep](3.11
    10.42) > [3.0.0]: OK.
    Check [sed](4.9) > [4.0.0]: OK.
    Check [python3](3.12.3) > [3.6.0]: OK.
    Check [git](2.43.0) > [2.0.0]: OK.
    Check [ninja](1.11.1) > [1.6.0]: OK.
    Check [cmake](3.28.3) > [3.16.0]: OK.
    Check submodules.

.. important::
    当检测不通过时会提示所需最低版本，例如：  
    ``Please install [lcov], and version > [1.14]``  
    请根据提示安装对应依赖

创建项目
--------

1. 基础命令：

.. code-block:: bash

    $ tos new

2. 操作流程：

   1. 输入项目名称（如 ``hello_world``）
   2. 选择开发板：

   .. code-block:: bash

       $ tos menuconfig

3. 生成目录结构：

.. code-block:: text

    ├── CMakeLists.txt
    ├── app_default.config
    └── src
        └── hello_world.c

+---------------------+---------------------------------------------------+
| 文件                | 说明                                              |
+=====================+===================================================+
| CMakeLists.txt      | 项目编译配置文件                                  |
+---------------------+---------------------------------------------------+
| app_default.config  | 项目配置（需通过 ``tos savedef`` 保存差异配置）   |
+---------------------+---------------------------------------------------+
| src/                | 项目源码目录                                      |
+---------------------+---------------------------------------------------+
| src/hello_world.c   | 项目源码文件，用于存放项目源码。                  |
+---------------------+---------------------------------------------------+

项目编译
--------

.. code-block:: bash

    $ cd hello_world
    $ tos build

.. tip::
    首次编译会自动下载工具链，建议保持网络通畅

配置管理
--------

+----------------------+------------------------------------------+
| 命令                 | 功能描述                                 |
+======================+==========================================+
| ``tos menuconfig``   | 交互式配置项目                           |
+----------------------+------------------------------------------+
| ``tos clean``        | 清理编译产物                             |
+----------------------+------------------------------------------+
| ``tos fullclean``    | 深度清理（含 build 目录）                |
+----------------------+------------------------------------------+
| ``tos savedef``      | 保存配置差异到 app_default.config        |
+----------------------+------------------------------------------+
| ``tos config_choice``| 该命令会选择 config 目录中的配置文件     |
|                      | 代替 app_defalut.config 文件             |
+----------------------+------------------------------------------+
| ``tos update``       | 更新 tos 工具                            |
+----------------------+------------------------------------------+

.. note::
    该命令 ``tos savedef`` 会将menuconfig配置结果与默认值的差异内容，保存在`app_default.config`文件中。
