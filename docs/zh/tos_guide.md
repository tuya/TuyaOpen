# tos 使用指南
tos 命令是 TuyaOpen 构建工具，支持创建、编译、配置等多种功能。

tos 命令位于 [TuyaOpen](https://github.com/tuya/TuyaOpen.git) 根目录下，使用 shell 脚本实现，使用前先将 TuyaOpen 路径添加到系统环境变量中。

## 命令列表
可通过 `tos help` 查看命令列表。
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

## 使用示例

### 查看版本
```shell
$ tos version
2.0.0
```

### 检测当前环境
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

tos check 命令会检测当前环境是否满足构建要求，如果满足则输出 OK，不符合会提示所需最低版本如 `Please install [lcov], and version > [1.14]`，请根据检测结果，安装相关依赖及对应版本。

### 创建项目
```shell
$ tos new
```
1. 根据提示输入项目名称，如： `hello_world`。


2. 选择项目对应的 board：
```shell
$ tos menuconfig
```

在展示页面选择对应的board。

项目目录及模板文件如下。
```shell
├── CMakeLists.txt
├── app_default.config
└── src
    └── hello_world.c
```
其中：
- `CMakeLists.txt`：项目配置文件，用于配置项目编译选项。
- `app_default.config`：项目配置文件，记录menuconfig的差异结果,不会主动保存，需要通过命令 `tos savedef` 保存。
- `src`：源码目录，用于存放项目源码文件。
- `src/hello_world.c`：项目源码文件，用于存放项目源码。

### 编译项目

进入项目目录，执行以下命令：
```shell
$ cd hello_world
$ tos build
```

第一次build时，tos工具会下载相关的工具链。

### 配置项目
进入项目目录，执行以下命令：
```shell
$ cd hello_world
$ tos menuconfig
```

### 清理项目
进入项目目录，执行以下命令：
```shell
$ cd hello_world
$ tos clean
```

深度清理命令：
```shell
$ cd hello_world
$ tos fullclean
```

### 保存配置文件
```shell
$ tos savedef
```

该命令会将menuconfig配置结果与默认值的差异内容，保存在`app_default.config`文件中。

### 选择配置文件
```shell
$ tos config_choice
```

该命令会选择config目录中的配置文件代替app_defalut.config文件。

### 设置示例

```shell
$ tos set_example
```

该命令用来设置不同芯片平台的示例展示，会改变目录`examples`的内容。


### 更新tuyaopen
执行以下命令：
```shell
$ tos update
```
