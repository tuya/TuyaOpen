# tos Guide

The tos command is a build tool for TuyaOpen, supporting various functions such as creating, compiling, and configuring.

The tos command is located in the root directory of [TuyaOpen](https://github.com/tuya/TuyaOpen.git) and is implemented using shell scripts. Before using it, you need to add the path of TuyaOpen to the system environment variable.

## Command List
You can view the command list by running `tos help`.

```shell
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
```

## Usage Examples

### View Version
```shell
$ tos version
2.0.0
```

### Check Current Environment
```shell
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
```

The tos check command will check whether the current environment meets the build requirements. If it does, it will output OK. If not, it will prompt for the minimum version required, such as `Please install [lcov], and version > [1.14]`. Please install the relevant dependencies and corresponding versions according to the check results.

### Create Project
```shell
$ tos new
```
1. Enter the project name according to the prompt, such as: `hello_world`.

2. Select the board to the project:
```shell
$ cd hello_world
$ tos menuconfig
```

Select the corresponding board on the display page.

```shell
├── CMakeLists.txt
├── app_default.config
└── src
    └── hello_world.c
```
Among them:
- `CMakeLists.txt`: Project configuration file, used to configure project compilation options.
- `app_default.config`: The project configuration file, which records the difference results of menuconfig, is not actively saved, and needs to be saved by the command `tos savedef`.
- `src`: Source code directory, used to store project source code files.
- `src/hello_world.c`: Project source code file, used to store project source code.

### Compile Project

Enter the project directory and run the following command:
```shell
$ cd hello_world
$ tos build
```
When you build for the first time, the tos tool downloads the related toolchain.

### Configure Project
Enter the project directory and run the following command:
```shell
$ cd hello_world
$ tos menuconfig
```

### Clean Project
Enter the project directory and run the following command:
```shell
$ cd hello_world
$ tos clean
```

Deep cleaning command:

```shell
$ cd hello_world
$ tos fullclean
```

### Save the configuration file

```shell
$ tos savedef
```

This command saves the difference between the menuconfig result and the default value in the `app_default.config` file.

### Select the configuration
```shell
$ tos conifg_choice
```

This command selects the configuration file in the config directory instead of the app_defalut.config file.

### Setting example

```shell
$ tos set_example
```

This command is used to display examples for different chip platforms, changing the contents of the directory `examples`.

### Update TuyaOpen

Run the following command:

```shell
$ tos update
```
