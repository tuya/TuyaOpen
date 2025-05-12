.. _new_board:

Adding and Adapting Boards
===========================

Definition
----------

In TuyaOpen, a "board" refers to a specially designed hardware collection for specific domain development. It includes not only the core board but also various sensors, actuators, and expansion modules, aiming to provide developers with a complete and convenient development platform to accelerate product development or facilitate research. Each board is bound to a specific platform.

Board Naming
------------

Mandatory Fields
~~~~~~~~~~~~~~~
- Manufacturer name: Represents the board manufacturer (e.g. "TUYA" for Tuya's boards)
- Chip name: Platform name (e.g. "T2", "T5AI")

Optional Fields
~~~~~~~~~~~~~~~
- Key features: Indicate special functionalities (e.g. "LCD", "CAM")
- Series and version: For products with series variations
- Other identifiers: Additional hardware markers

Naming Rules
~~~~~~~~~~~~
1. UPPERCASE letters only
2. Fields separated by underscores
3. Order: Manufacturer_CoreBoardName_OptionalFields

Example: ``TUYA_T5AI_BOARD``

Adding New Board
----------------

Prerequisite: Follow the :doc:`README <./README_zh>` to ensure proper environment setup.

Create Directory
~~~~~~~~~~~~~~~~
Navigate to the corresponding platform directory and create a new board subdirectory:

.. code-block:: bash

    cd boards/T5AI
    mkdir -p TUYA_T5AI_EVB

Add Kconfig File
~~~~~~~~~~~~~~~~
Each board requires a Kconfig file for peripheral configuration:

.. code-block:: bash

    cd TUAY_T5AI_EVB
    vim Kconfig

Create an empty Kconfig file if no special configuration needed:

.. code-block:: bash

    :wq

Add Board to Selection List
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Modify the platform's Kconfig file to include the new board:

.. code-block:: bash

    cd ..
    vim Kconfig

Add board configuration:

.. code-block:: bash

    config BOARD_CHOICE_TUYA_T5AI_EVB
        bool "TUYA_T5AI_EVB"
    if (BOARD_CHOICE_T5AI_EVB)
    rsource "./TUYA_T5AI_EVB/Kconfig"
    endif

Save changes:

.. code-block:: bash

    :wq

Application Adaptation
----------------------

Navigate to target application:

.. code-block:: bash

    cd ../..
    cd apps/tuya.ai/your_chat_bot

Configure and build:

.. code-block:: bash

    tos menuconfig  # Select TUYA_BOARD_EVB
    tos build

After verification, save configuration:

.. code-block:: bash

    tos savedef
    cp app_default.config ./config/TUYA_T5AI_EVB.config

Future developers can select this configuration via:

.. code-block:: bash

    tos config_choice

Contribution
------------

Submit your board via Pull Requests following the :doc:`Contribution Guide <contribute_guide>` to share it with the community.