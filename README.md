<p align="center">
<img src="docs/images/TuyaOpen.png" width="60%" >
</p>

English | [简体中文](README_zh.md)

## Overview
TuyaOpen is an open source AI+IoT development framework that supports cross-chip platforms and operating systems. It is designed based on a universal southbound interface and supports communication protocols such as Bluetooth, Wi-Fi, and Ethernet. It provides core functionalities for AI+IoT development, including pairing, activation, control, and upgrading.
The sdk has robust security and compliance capabilities, including device authentication, data encryption, and communication encryption, meeting data compliance requirements in various countries and regions worldwide.

AI+IoT products developed using the TuyaOpen, if utilizing the functionality of the tuya_cloud_service component, can make use of the powerful ecosystem provided by the Tuya APP and cloud services, and achieve interoperability with Power By Tuya devices.

At the same time, the TuyaOpen will continuously expand, providing more cloud platform integration features, as well as voice, video, and facial recognition capabilities.

## Getting Start

### Prerequisites
Ubuntu and Debian

```sh
$ sudo apt-get install lcov cmake-curses-gui build-essential ninja-build wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev
```

> Note: After version v1.1.0, we adopted ninja as the build tool to speed up compilation. If you encounter compilation errors, please install ninja.

### Clone the repository

```sh
$ git clone https://github.com/tuya/TuyaOpen.git
```
The tuyeopen repository contains multiple submodules. The tos tool will check and automatically download the submodules before compilation, or you can manually download them using the command `git submodule update --init`.

## Setup and Compilation

### step1. Setting Environment Variables
```sh
$ cd TuyaOpen
$ export PATH=$PATH:$PWD
```
Or add the TuyaOpen path to your system environment variables.


TuyaOpen can be compiled and debugged using the tos command, which will search for the TuyaOpen repository based on the path set in the environment variables and execute the corresponding operations.

For detailed usage of the tos command, please refer to [tos command](./docs/en/tos_guide.md).

### step2. Select the corresponding example
Use the command `tos set_example` to complete the selection according to the platform, and the directory `examples` will be modified to the corresponding platform's examples.

### step3. Compilation
Select the corresponding project for the current compilation in examples or apps, and then run the following command to compile:
```sh
$ cd examples/get-started/sample_project
$ tos build
```
After compilation, the target files will be located in the `examples/get-started/sample_project/.build/t2/bin/t2_1.0.0` directory.

### Configuration 
To configure the selected examples or apps project, run the following command in the corresponding project directory for menu-driven configuration:
```sh
$ cd examples/get-started/sample_project
$ tos menuconfig
```
Configure the current project, save and exit after configuration, and then compile the project.

### Supported platform list
| Name  | Support Status | Introduction | Debug log serial port |
| ---- | ---- | ---- | ---- |
| Ubuntu | Supported  | Can be run directly on Linux hosts such as ubuntu. | |
| T2 |  Supported  | Supported Module List: [T2-U](https://developer.tuya.com/en/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq) | Uart2/115200 |
| T3 |  Supported  | Supported Module List: [T3-U](https://developer.tuya.com/en/docs/iot/T3-U-Module-Datasheet?id=Kdd4pzscwf0il) [T3-U-IPEX](https://developer.tuya.com/en/docs/iot/T3-U-IPEX-Module-Datasheet?id=Kdn8r7wgc24pt) [T3-2S](https://developer.tuya.com/en/docs/iot/T3-2S-Module-Datasheet?id=Ke4h1uh9ect1s) [T3-3S](https://developer.tuya.com/en/docs/iot/T3-3S-Module-Datasheet?id=Kdhkyow9fuplc) [T3-E2](https://developer.tuya.com/en/docs/iot/T3-E2-Module-Datasheet?id=Kdirs4kx3uotg) etc. | Uart1/460800 |
| T5AI | Supported | Supported Module List: [T5-E1](https://developer.tuya.com/en/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi) [T5-E1-IPEX](https://developer.tuya.com/en/docs/iot/T5-E1-IPEX-Module-Datasheet?id=Kdskxvxe835tq) etc. | Uart1/460800 |
| ESP32/ESP32C3/ESP32S3 | Supported | | Uart0/115200 |
| LN882H | Supported |  | Uart1/921600 |
| BK7231N | Supported | Supported Module List:  [CBU](https://developer.tuya.com/en/docs/iot/cbu-module-datasheet?id=Ka07pykl5dk4u)  [CB3S](https://developer.tuya.com/en/docs/iot/cb3s?id=Kai94mec0s076) [CB3L](https://developer.tuya.com/en/docs/iot/cb3l-module-datasheet?id=Kai51ngmrh3qm) [CB3SE](https://developer.tuya.com/en/docs/iot/CB3SE-Module-Datasheet?id=Kanoiluul7nl2) [CB2S](https://developer.tuya.com/en/docs/iot/cb2s-module-datasheet?id=Kafgfsa2aaypq) [CB2L](https://developer.tuya.com/en/docs/iot/cb2l-module-datasheet?id=Kai2eku1m3pyl) [CB1S](https://developer.tuya.com/en/docs/iot/cb1s-module-datasheet?id=Kaij1abmwyjq2) [CBLC5](https://developer.tuya.com/en/docs/iot/cblc5-module-datasheet?id=Ka07iqyusq1wm) [CBLC9](https://developer.tuya.com/en/docs/iot/cblc9-module-datasheet?id=Ka42cqnj9r0i5) [CB8P](https://developer.tuya.com/en/docs/iot/cb8p-module-datasheet?id=Kahvig14r1yk9) etc. | Uart2/115200 |
| raspberry pico-w | In Development, to be released in Nov 2024 | | |

## Flashing

### GUI Tool Flashing
The `tyutool gui` flashing tool supports serial port flashing for multiple chips such as T2/T3/T5AI/BK7231N/LN882H/ESP32, and is compatible with Windows/Linux/macOS operating systems. Please choose the corresponding GUI flashing tool based on your operating system.
- Windows: [tyutool_win](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.tar.gz)
- Linux: [tyutool_linux.tar](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz)
- macOS x86: [tyutool_mac_x86](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz)
- macOS arm64: [tyutool_mac_arm64.zip](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz)

### Command Line Flashing
You can flash the device with a single command using `tos flash`.

1. In environments like Linux, you need to set the serial port permissions first using the following command; otherwise, an error will occur during execution.
```sh
$ sudo usermod -aG dialout $USER
```

2. Run the `tos flash` command in the directory of the compiled project for one-click flashing. The `tos flash` command will automatically download the corresponding `tyutool` tool based on the current running environment and proceed with the flashing process.
```sh
$ cd examples/get-started/sample_project
$ tos flash
tyutool params:
[INFO]: tyut_logger init done.
[INFO]: Run Tuya Uart Tool.
[INFO]: Use default baudrate: [921600]
[INFO]: Use default start address: [0x00]
--------------------
1. /dev/ttyS0
2. /dev/ttyS1
3. /dev/ttyS2
4. /dev/ttyS3
5. /dev/ttyS4
6. /dev/ttyS5
7. /dev/ttyS6
8. /dev/ttyS7
9. /dev/ttyS8
10. /dev/ttyS9
11. /dev/ttyS10
12. /dev/ttyS11
13. /dev/ttyS12
14. /dev/ttyS13
15. /dev/ttyS14
16. /dev/ttyS15
17. /dev/ttyS16
18. /dev/ttyS17
19. /dev/ttyS18
20. /dev/ttyS19
21. /dev/ttyS20
22. /dev/ttyS21
23. /dev/ttyS22
24. /dev/ttyS23
25. /dev/ttyS24
26. /dev/ttyS25
27. /dev/ttyS26
28. /dev/ttyS27
29. /dev/ttyS28
30. /dev/ttyS29
31. /dev/ttyS30
32. /dev/ttyS31
33. /dev/ttyUSB0
^^^^^^^^^^^^^^^^^^^^
Select serial port: 33                              ## Select the correct serial port
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
```

> Note: During the flashing process, you need to enter the boot mode according to the actual situation of the chip before performing serial port flashing.
> If there is no response from the serial port during the flashing process, please check whether the serial port is correctly selected or if it is being used by another program.

3. The `tos flash` flashing tool is continuously adding support for new chip models. Versions prior to v1.8.0 do not support automatic tool upgrades; subsequent versions will detect upgrades and prompt for an upgrade upon startup.
You can query the version information using `tos flash --version`,
```sh
$ tyutool params: --version
tyuTool, version 1.8.3
```

For versions prior to v1.8.0, you need to manually run the following command to upgrade:

```sh
$ tos flash upgrade
```

## Sample Projects
The TuyaOpen provides a variety of sample projects to facilitate developers in quickly getting started and understanding the usage of the TuyaOpen.

```sh
$ TuyaOpen
├── ai
│   └── llm_demo
├── ble
│   ├── ble_central
│   └── ble_peripher
├── get-started
│   └── sample_project
├── graphics
│   └── lvgl_demo
├── multimedia
│   ├── audio
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
```

Each sample project includes a README.md file that provides detailed instructions on configuring, compiling, and running the project.

Each different chip has corresponding examples, and you can set the example project through the `tos set_example` command. Click [tos set_example](https://github.com/tuya/TuyaOpen/blob/master/docs/en/tos_guide.md#setting-example) to learn more details.

## AI Applications

Tuya.ai is a comprehensive AI service platform that provides the following core capabilities:

- Audio processing services
- Video processing services
- Multimodal AI services

For detailed usage instructions, please refer to the [Tuya.ai Documentation](apps/tuya.ai/README.md).

## Cloud Connectivity Applications

TuyaOpen provides a wealth of cloud connectivity application examples, which can be found in the apps directory. You can click [Cloud Connectivity Applications](apps/tuya_cloud/README.md) for more information.


## Adding and Adapting New Platforms

TuyaOpen supports adding and adapting new platforms. For specific operations, please refer to [Adding and Adapting New Platforms](./docs/en/new_platform.md).

## FAQ
1. The supported platform for TuyaOpen are dynamically downloaded through subrepositories. Updating the TuyaOpen repository itself will not automatically update the subrepositories. If you encounter any issues with compilation, please navigate to the corresponding directory in the "platform" folder and use the `git pull` command to update, or delete the corresponding directory in the "platform" folder and download it again.


2. TuyaOpen provides a wealth of cloud connectivity application examples. If you encounter issues such as being unable to connect or activate devices properly, please refer to [Cloud Connectivity Applications](apps/tuya_cloud/README.md).

## License
Distributed under the Apache License Version 2.0. For more information, see `LICENSE`.

## Contribute Code
If you are interested in the TuyaOpen and wish to contribute to its development and become a code contributor, please first read the [Contribution Guide](./docs/en/contribute_guide.md).

## Disclaimer and Liability Clause

Users should be clearly aware that this project may contain submodules developed by third parties. These submodules may be updated independently of this project. Considering that the frequency of updates for these submodules is uncontrollable, this project cannot guarantee that these submodules are always the latest version. Therefore, if users encounter problems related to submodules when using this project, it is recommended to update them as needed or submit an issue to this project.

If users decide to use this project for commercial purposes, they should fully recognize the potential functional and security risks involved. In this case, users should bear all responsibility for any functional and security issues, perform comprehensive functional and safety tests to ensure that it meets specific business needs. Our company does not accept any liability for direct, indirect, special, incidental, or punitive damages caused by the user's use of this project or its submodules.

## Related Links
- Arduino for TuyaOpen: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode for tuyaopen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)
