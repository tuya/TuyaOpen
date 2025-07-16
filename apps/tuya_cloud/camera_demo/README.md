# camera_demo

## Overview
A simple, cross-platform, cross-system, and multi-connection switch example. By using the Tuya App and Tuya Cloud Services, you can pair, activate, upgrade, remotely pull a video stream to watch (when away), local area pull a video stream to watch (within the same LAN)for this camera.

![](https://images.tuyacn.com/fe-static/docs/img/74af5009-43f6-439e-8089-fca2b0edbe96.jpg)



## Directory
```sh
+- camera_demo
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
* tuya_main.c: the main function of the camera_demo
* tuya_config.h: the tuya PID and license, to get the license, you need create a product on Tuya AI+IoT Platfrom following [TuyaOS quickstart](https://developer.tuya.com/en/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc)


## Supported Hardware  
The current project can run on all currently supported chips and development boards.
