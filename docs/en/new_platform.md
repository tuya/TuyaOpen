# Platform Addition and Adaptation

## Introduction
[TuyaOpen](https://github.com/tuya/TuyaOpen) uses the `platform_config.yaml` file to manage platforms. The `platform/platform_config.yaml` file contains information about platform repositories that have implemented generic interfaces. You can configure `platform` and `chip` (if the current platform supports multiple chips, you need to configure chip information) in the `project_build.ini` file in the project directory. After running the tos compilation, the target platform will automatically download the corresponding platform repository and associate it with the `TuyaOpen` project.

If you want to port `TuyaOpen` to your own chip, you need to perform porting adaptation.

## Porting and Adapting the Platform
### Generate a New Platform Directory
1. Download and enter the `TuyaOpen` directory and set the environment variables:
```sh
$ cd TuyaOpen
$ export PATH=$PATH:$PWD
```
Or add the TuyaOpen path to the system environment variables.

Tuyaopen uses the tos command for compilation, debugging, etc. The tos command will search for the TuyaOpen repository based on the path set in the environment variables and execute the corresponding operations.

For detailed usage of the tos command, please refer to [tos command](tos_guide.md).

2. Use the following command to generate a new `platform` subdirectory:

```bash
$ tos new_platform <new-platform-name>
```

When creating a new `platform`, the command will automatically pop up a `menuconfig` dialog box:
```shell
$ tos new_platform <new-platform-name>
(1.0.0) PROJECT_VERSION (NEW)
    configure TuyaOpen  --->
        configure tuya cloud service  --->
        configure enable/disable liblwip  --->
        configure enable/disable libtflm  --->
        configure mbedtls  --->
        configure system parameter  --->
    configure board <your-board-name>  --->
        OPERATING_SYSTEM (RTOS)  --->
        ENDIAN (LITTLE_ENDIAN)  --->
        [ ] ENABLE_FILE_SYSTEM --- support filesystem (NEW)
        [ ] ENABLE_WIFI --- support wifi (NEW)
        [*] ENABLE_WIRED --- support wired (NEW)
        [ ] ENABLE_BLUETOOTH --- support BLE (NEW)  ----
        [ ] ENABLE_RTC --- support rtc (NEW)
        [ ] ENABLE_WATCHDOG --- support watchdog (NEW)
        [*] ENABLE_UART --- support uart (NEW)
        [*] ENABLE_FLASH --- support flash (NEW)  --->
        [ ] ENABLE_ADC --- support adc (NEW)
        [ ] ENABLE_PWM --- support pwm (NEW)
        [*] ENABLE_GPIO --- support gpio (NEW)
        [ ] ENABLE_I2C --- support i2c (NEW)
        [ ] ENABLE_SPI --- support spi (NEW)
        [ ] ENABLE_TIMER --- support hw timer (NEW)
        [ ] ENABLE_DISPLAY --- support GUI display (NEW)
        [ ] ENABLE_MEDIA --- support media (NEW)
        [ ] ENABLE_PM --- support power manager (NEW)
        [ ] ENABLE_STORAGE --- support storage such as SDCard (NEW)
        [ ] ENABLE_DAC --- support dac (NEW)
        [ ] ENABLE_I2S --- support i2s (NEW)
        [ ] ENABLE_WAKEUP --- support lowpower wakeup (NEW)
        [ ] ENABLE_REGISTER --- support register (NEW)
        [ ] ENABLE_PINMUX --- support pinmux (NEW)
        [ ] ENABLE_PLATFORM_AES --- support hw AES (NEW)
        [ ] ENABLE_PLATFORM_SHA256 --- support hw sha256 (NEW)
        [ ] ENABLE_PLATFORM_MD5 --- support hw md5 (NEW)
        [ ] ENABLE_PLATFORM_SHA1 --- support hw sha1 (NEW)
        [ ] ENABLE_PLATFORM_RSA --- support hw rsa (NEW)
        [ ] ENABLE_PLATFORM_ECC --- support hw ecc (NEW)
```
- In `TuyaOpen`, configure the default software features for the newly added platform based on the software feature configuration.
- In `configure board <your-board-name>`, configure the default hardware features for the newly added platform based on the hardware feature support.

After completing the configuration, save the configuration [Shortcut `S`], exit [Shortcut `Q`], and the default feature configuration `default.config` will be automatically generated.

3. Generate the New Platform Directory

The `tos new_platform` command generates a new platform directory based on the configuration and automatically generates the `tkl` interface layer related code according to the configuration in `menuconfig`.

The following log output is displayed during the process:

```
..............
    make ability: system
        new file: tkl_sleep.c
        new file: tkl_memory.c
        new file: tkl_output.c
        new file: tkl_semaphore.c
        new file: tkl_queue.c
        new file: tkl_system.c
        new file: tkl_fs.c
        new file: tkl_ota.c
        new file: tkl_thread.c
        new file: tkl_mutex.c
    make ability: uart
        new file: tkl_uart.c
    make ability: security
generate code finished!
```
Seeing `generate code finished!` indicates that the template has been successfully generated.

> Note: The log may vary slightly depending on the configured features for reference.

After completion, a new directory as follows will be obtained under the platform:
```bash
- platform
    + t2
    + ubuntu
    - <new-platform-name>
        + <new-platform-sdk>       # Need to create by yourself, store chip-related SDK
        + toolchain                # Need to create by yourself, store toolchain
        + tuyaos                   # Store tuyaos adaptation layer
            - tuyaos_adapter       # Store tuyaos adaptation layer interface source code
                - include
                - src        
        - Kconfig                   # Expected configurable items, can be modified
        - default.config            # Platform default supported features, can be modified
        - platform_config.cmake     # Adaptation layer source code path
        - toolchain_file.cmake      # TuyaOpen compilation tool path and options
        - build_example.sh          # Compilation script  
```



## Complete Compilation Adaptation

After the template is generated, you can make some modifications to the template as needed and complete the implementation of some scripts to ensure that the compilation process can proceed smoothly.

### Kconfig
The Kconfig file defines the features supported by the new platform. Please modify the `<your-board-name>` in the `menu` of the initially generated Kconfig file according to the current new platform name. You can also add/delete related configurations based on the actual hardware situation.
```bash
menu "configure board <your-board-name>"
    ...

    endmenu
```

### platform_config.cmake:
This file is the TuyaOpen compilation dependency platform-related adaptation file and header file path, generally does not need to be modified.
```bash
list_subdirectories(PLATFORM_PUBINC ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)
```

Tuyaopen uses cmake for compilation, the `list_subdirectories` function will add all subdirectories in the specified directory to the PLATFORM_PUBINC variable for use during TuyaOpen compilation.

If you need to add other paths, you can refer to the following method:
```bash
list_subdirectories(PLATFORM_PUBINC_1 ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)
set(PLATFORM_PUBINC_2 
    ${PLATFORM_PATH}/TuyaOpen/build/config
)

set(PLATFORM_PUBINC 
    ${PLATFORM_PUBINC_1}
    ${PLATFORM_PUBINC_2})
```

### toolchain_file.cmake:
1. Set the compilation tool path and toolchain prefix, you can refer to the files in the existing `t2` and `ubuntu` directories.

```bash
set(TOOLCHAIN_DIR "${PLATFORM_PATH}/toolchain/<your-toolchain-name>")
set(TOOLCHAIN_PRE "<your-toolchain-prefix>")
```

2. According to the compilation parameters of the newly added platform, correctly set the TuyaOpen compilation options.
```bash
set(CMAKE_C_FLAGS "<your-compiler-c-flags>")
```

3. build_example.sh
You need to complete the compilation and linking of the original factory SDK here to generate firmware for various purposes. You can refer to the files in the existing `t2` and `ubuntu` directories. (Note, the commented line can be opened directly in the `linux` system, combined with the default `Makefile` can directly compile the executable file.)



### Modify platform_config.yaml
Add a `<new-platform-name>` in the `platform/platform_config.yaml` file. If the current project is a local project, you can leave `repo` and `commit` blank. If the project is managed through `git`, you can fill in according to the actual situation.
The format is as follows:
```bash
- name: t3
  repo: https://github.com/tuya/TuyaOpen-platform-t3
  commit: master
```

If the new platform supports multiple chips, you can fill in as follows:
```bash
- name: new_platform
  repo: https://github.com/xxxx/new_platform
  commit: master
  chip: 
    - chip1
    - chip2
    - chip3
```



### Compilation
Follow the instructions in the [README_zh.md](../README_zh.md) document, modify/add the current platform name in the `project_build.ini` of the project to be compiled and tested, and then compile once.
```bash
[project:sample_project_<new-platform-name>]
platform = <new-platform-name>
```

It is recommended that the project name be `current project name_platform name` or `current project name_chip name`.

```bash
$ cd examples/get-started/sample_project
$ tos build
```

Normally, after completing the compilation adaptation, it can be compiled directly. If the compilation is not successful, please make adaptation modifications according to the error prompts.


### Modify default.config Default Values
default.config is the default configuration generated based on the selection when generating the porting template, used for the default configuration of the first project compilation.

default.config and Kconfig together form the TuyaOpen menu configuration function, and automatically generate `using.config`, `using.cmake`, `tuya_kconfig.h` files, which are used in TuyaOpen compilation and source code.

If you need to modify the default configuration, please execute `make menuconfig` in the corresponding project directory to modify the feature configuration, and manually merge the relevant modifications into default.config.

## Complete Interface Adaptation
When generating the porting template, some empty functions are generated in the `tuyaos/tuyaos_adapter/src/` directory, which need to be implemented to ensure the normal operation of the functions.

Because `TuyaOpen` uses the same underlying interface as `TuyaOS`, you can port it in the same way as `TuyaOS`, or you can refer to the files in the existing `t2` and `ubuntu` directories.

[Port to Linux System Chip](https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_linux?id=Kcrwrf72ciez5#title-1-%E9%80%82%E9%85%8D%20RTC)

[Port to RTOS System Chip](https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-translation_rtos?id=Kcrwraf21847l#title-1-%E9%80%82%E9%85%8D%E7%A8%8B%E5%BA%8F%E5%85%A5%E5%8F%A3)

For specific interface introductions, please refer to the documents in the `./tools/porting/docs/` directory of the `TuyaOpen` directory.
> Note 1: Peripheral interfaces are not mandatory to adapt, developers can completely use the original factory interfaces; in order to facilitate developers to use more TuyaOpen related functions, it is recommended to adapt.

> Note 2: For network-related interfaces, if it is an external network card structure, you need to implement the network card driver yourself, implement socket related operations, and related `wifi`, `wired` operations:


## Sample Programs
After completing the adaptation, you can compile and debug with sample programs. `TuyaOpen` provides rich sample programs that can be compiled through the `tos` command, and then burned and run through your own burning tool for debugging.

```shell
$ TuyaOpen
├── ai
│   └── llm_demo
│   └── tflm
│       └── hello_world
│       └── micro_speech
│       └── person_detection
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

## Test Functions

Please test according to the following test cases, and also conduct full coverage functional and performance testing according to the functions of your own product.
[https://drive.weixin.qq.com/s?k=AGQAugfWAAkb5lIvFsAEgAwQZJALE](https://drive.weixin.qq.com/s?k=AGQAugfWAAkb5lIvFsAEgAwQZJALE)

## Submission
After completing the adaptation and passing the test, you are welcome to submit Push Requests to submit code, submit the `platform_config.yaml` corresponding to the newly adapted platform to the [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen) repository for other developers to use.

- Push Requests process can refer to [Contribution Guide](contribute_guide.md)
- Programming specifications can refer to [Coding Style Guide](code_style_guide.md)
