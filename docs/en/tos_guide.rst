.. _tos_guide:

#############
tos User Guide
#############

The tos command is TuyaOpen's build tool, supporting various functions like project creation, compilation, and configuration.

.. note::

    The tos command is located in the root directory of `TuyaOpen <https://github.com/tuya/TuyaOpen.git>`_, implemented as a shell script. Add the TuyaOpen path to the system environment variables before use.

========
Command List
========

View complete command list via `tos help`.

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
        set_example  - Set examples from platform
        new_platform - New platform [platform_name]
        update     - Update the platforms according to the platform_config.yaml
        help       - Help information

==========
Usage Examples
==========

Check Version
----------

.. code-block:: bash

    $ tos version
    2.0.0

Environment Check
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
    When checks fail, the minimum required version will be prompted. For example:  
    ``Please install [lcov], and version > [1.14]``  
    Please install corresponding dependencies as prompted.

Create Project
--------

1. Basic command:

.. code-block:: bash

    $ tos new

2. Workflow:

   1. Enter project name (e.g. ``hello_world``)
   2. Select development board:

   .. code-block:: bash

       $ tos menuconfig

3. Generated directory structure:

.. code-block:: text

    ├── CMakeLists.txt
    ├── app_default.config
    └── src
        └── hello_world.c

+---------------------+-------------------------------------------------+
| File                | Description                                     |
+=====================+=================================================+
| CMakeLists.txt      | Project compilation configuration file         |
+---------------------+-------------------------------------------------+
| app_default.config  | Project configuration (save differences via ``tos savedef``)|
+---------------------+-------------------------------------------------+
| src/                | Project source code directory                   |
+---------------------+-------------------------------------------------+
| src/hello_world.c   | Project source code file for storing project source. |
+---------------------+-------------------------------------------------+

Project Compilation
--------

.. code-block:: bash

    $ cd hello_world
    $ tos build

.. tip::
    The toolchain will be automatically downloaded during first compilation. Ensure stable network connection.

Configuration Management
--------

+----------------------+------------------------------------------+
| Command              | Function Description                     |
+======================+==========================================+
| ``tos menuconfig``   | Interactive project configuration       |
+----------------------+------------------------------------------+
| ``tos clean``        | Clean build artifacts                    |
+----------------------+------------------------------------------+
| ``tos fullclean``    | Deep clean (including build directory)   |
+----------------------+------------------------------------------+
| ``tos savedef``      | Save configuration differences to app_default.config |
+----------------------+------------------------------------------+
| ``tos config_choice``|Selects config files from config directory to replace app_defalut.config|
+----------------------+------------------------------------------+
| ``tos set_example``  | Sets example demonstrations for different chip platforms (modifies `examples` directory)|
+----------------------+------------------------------------------+
| ``tos update``       | Update tos tool                          |
+----------------------+------------------------------------------+
                             
.. note::
    The ``tos savedef`` command saves the differences between menuconfig results and default values in the `app_default.config` file.