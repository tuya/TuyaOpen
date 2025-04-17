# IoT Application

The Tuya Cloud Application is an application provided by the Tuya AI+IoT platform, which allows developers to quickly implement features such as remote control and device management.

`switch_demo` demonstrates a simple, cross-platform, cross-system switch example that supports multiple connections. Through the Tuya APP and Tuya Cloud Service, this switch can be remotely controlled(when away), local area network control (within the same LAN), and Bluetooth control (when no network is available) for this switch.

![](https://images.tuyacn.com/fe-static/docs/img/0e155d73-1042-4d9f-8886-024d89ad16b2.png)


## Directory
```
+- switch_demo
    +- libqrencode
    +- src
        -- cli_cmd.c
        -- qrencode_print.c
        -- tuya_main.c
        -- tuya_config.h
    -- CMakeLists.txt
    -- README_CN.md
    -- README.md
```
* libqrencode: a open souce libirary for QRCode display
* qrencode_print.c: print the QRCode in screen or serial tools
* cli_cmd.c: cli cmmand which used to operater the swith_demo
* tuya_main.c: the main function of the switch_demo
* tuya_config.h: the tuya PID and license, to get the license, you need create a product on Tuya AI+IoT Platfrom following [TuyaOS quickstart](https://developer.tuya.com/en/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc)


## Supported Hardware  
The current project can run on all currently supported chips and development boards.


## Compilation
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tso menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.