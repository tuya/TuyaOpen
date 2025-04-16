# Quick Start

## Prerequisites
- Ubuntu and Debian

```sh
$ sudo apt-get install lcov cmake-curses-gui build-essential ninja-build wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev
```

> Note: After version v1.1.0, we adopted ninja as the build tool to speed up compilation. If you encounter compilation errors, please install ninja.

- macOS

Please run `tos check` to check the system dependencies and install them according to the prompts.

## Clone the repository

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

### Step2. Select the project to be compiled

- Method 1: Compile Example

To select the example to be compiled, use the `tos set_example` command to choose based on the platform. The `examples` directory will be modified to match the selected platform's example.

For more information about examples, click [Example Project](#example).

- Method 2: Compile App

Select the app to be compiled, such as [apps/tuya_cloud/switch_demo](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya_cloud/switch_demo), and switch to the corresponding directory.

Use the `tos config_choice` command to select the target platform or board for compilation.

```sh
$ cd apps/tuya_cloud/switch_demo
$ tos config_choice
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
```

The `tos config_choice` command reads the configuration files in the `config` directory of the project and generates the configuration file `app_default.config` for the current project.

> After running `tos config_choice` to switch configurations, the `tos` command will automatically clear the previously compiled intermediate files in the current project.

### step3. Compilation
Select the corresponding project for the current compilation in examples or apps, and then run the following command to compile:
```sh
$ cd apps/tuya_cloud/switch_demo
$ tos build
```
After compilation, the target files will be located in the `apps/tuya_cloud/switch_demo/.build/bin` directory.

The compiled target files include:
- `switch_demo_QIO_1.0.0.bin`: Complete firmware including boot, used for flashing.
- `switch_demo_UA_1.0.0.bin`: Application firmware without boot. This file needs to be flashed to the corresponding address based on different platforms/chips; otherwise, it may not run properly.
- `switch_demo_UG_1.0.0.bin`: BIN file for OTA upgrade, cannot be directly flashed and run.

The project name defaults to the directory name, and the project version defaults to `1.0.0`. These can be modified in the `tos menuconfig` configuration.

### step4. Configuration 
If you need to modify the project configuration, select the corresponding example or app project that needs to be configured, and run the following command for menu-based configuration in the corresponding project directory:

```sh
$ cd apps/tuya_cloud/switch_demo
$ tos menuconfig
```

Configure the current project, save and exit after configuration, and then compile the project.

> When you run `tos menuconfig` to switch the chip or development board, the `tos` command will automatically clear the compiled intermediate files in the current project.

## Flashing

### GUI Tool Flashing
The `tyutool gui` flashing tool supports serial port flashing for multiple chips such as T2/T3/T5AI/BK7231N/LN882H/ESP32, and is compatible with Windows/Linux/macOS operating systems. Please choose the corresponding GUI flashing tool based on your operating system.
- Windows: [tyutool_win](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/win_tyutool_gui.zip)
- Linux: [tyutool_linux.tar](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/tyutool_gui.tar.gz)
- macOS x86: [tyutool_mac_x86](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_x86_tyutool_gui.tar.gz)
- macOS arm64: [tyutool_mac_arm64.zip](https://images.tuyacn.com/smart/embed/package/vscode/data/ide_serial/darwin_arm64_tyutool_gui.tar.gz)

### Command Line Flashing
You can flash the device with a single command using `tos flash`.

1. In environments like Linux, you need to set the serial port permissions first using the following command; otherwise, an error will occur during execution.
```sh
$ sudo usermod -aG dialout $USER
```

After completing the settings, you need to restart the system for them to take effect.

2. Run the `tos flash` command in the directory of the compiled project for one-click flashing. The `tos flash` command will automatically download the corresponding `tyutool` tool based on the current running environment and proceed with the flashing process.
```sh
$ cd apps/tuya_cloud/switch_demo
$ tos flash
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
Select serial port: 3                              ## Select the correct serial port
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