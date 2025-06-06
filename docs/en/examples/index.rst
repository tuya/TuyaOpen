Example Projects
===========================

The TuyaOpen provides a variety of sample projects to facilitate developers in quickly getting started and understanding the usage of the TuyaOpen.

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

================================================
Select the project to be compiled
================================================

The compilation method for examples is consistent with that of apps. Use the `tos config_choice` command to select the target platform or board, then compile with `tos build`.  


Each sample project includes a README.md file that provides detailed instructions on configuring, compiling, and running the project.


========================
Compile the example
========================
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tos menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.