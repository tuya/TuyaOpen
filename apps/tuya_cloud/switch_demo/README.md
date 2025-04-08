# switch_demo

## Overview
A simple, cross-platform, cross-system, and multi-connection switch example. By using the Tuya App and Tuya Cloud Services, you can pair, activate, upgrade, remotely control (when away), local area network control (within the same LAN), and Bluetooth control (when no network is available) for this switch.

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
