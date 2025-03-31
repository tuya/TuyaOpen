## board 的新增和适配

### 定义

在 Tuyaopen 中 board 是指为了方便特定领域的开发工作而专门设计的一套硬件集合。它除了核心板以外，还包括各类传感器、执行器、扩展板等硬件模块，旨在为开发者提供一个完整且便捷的开发平台，以加速产品开发进程或方便学习研究。每种 board 都会有一个与之绑定的 platform。

### board 命名

#### 必选字段

- 厂家名称：代表开发板的生产厂家。如涂鸦推出的开发板则会用 “TUYA“ 开头。
- 芯片名称：platform 名称，如 ”T2“，“T5AI”。

#### 可选字段

- 体现开发板的主要特点或者功能：若 board 具有特定的功能模块或特点，如 ”LCD“， ”CAM“。
- 系列名称和版本信息：一些厂商会有系列化的产品，可在命名中体现系列名称和版本号。
- 其他：一些硬件标识等。

#### 命名规则

1. 字母必须大写。
2. 字段和字段之间用下划线隔开。
3. 字段结合顺序顺序，”厂家名称_核心板名称\_可选字段“

示例：TUYA_T5AI_BOARD

### 新增 board 

请先阅读 tuyaopen 根目录下的 [README](../../README_zh.md) ，先了解基础操作并保证所需的环境已被正确安装。

#### 新建文件夹

先进入 board 目录下，找到 对应的 platform 的 board 集合目录。在这个集合目录下新增一个 board 的子目录。

该目录主要存放和板子特性相关的配置文件和适配文件。

> 例：我想基于 T5AI 的芯片增加一块开发板，这块板子根据 board 命名规范确定板子的名称为 TUYA_T5AI_EVB。

```shell
cd boards/T5AI
mkdir -p TUYA_T5AI_EVB 
```

#### 添加 Kconfig 文件

板子的外设类型可能不一致，每个board下需要一个 Kconfig 文件。

> 例：在 TUAY_T5AI_EVB 中新增 Kconfig 文件

```shell
cd TUAY_T5AI_EVB 
vim Kconfig
```

在添加board相关的配置项并保存 Kconfig 文件，如果没有特殊配置，则Kconfig是个空文件。

```shell
:wq
```

#### 将 Board 添加到选择列表中

修改 board 集合目录下的 Kconfig 文件，在 “Choice a board”的选择列表中增添板子的配置选项（BOARD_CHOICE_<new_board>）并引用刚才新增的 Kconfig 文件路径。操作完成后，在应用中执行 `tos menuconfig` 的命令后就可以选择到该目标板子。

> 例：修改 boards/T5AI/Kconfig 文件，新增 BOARD_CHOICE_TUYA_T5AI_EVB 的配置选项，并添加 “。/TUYA_T5AI_EVB/Kconfig” 的文件路径。

回到 board/T5AI 目录下

```shell
cd ..
```

修改 Kconfig 文件

```shell
Vim Kconfig
```

新增板子选项

```shell
config BOARD_CHOICE_TUYA_T5AI_EVB
    bool "TUYA_T5AI_EVB"
if (BOARD_CHOICE_T5AI_EVB)
rsource "./TUYA_T5AI_EVB/Kconfig"
endif
```

保存 Kconfig 文件

```shell
:wq
```

### 适配应用

进入你想要适配的应用目录，先执行 `tos menuconfig` 根据板子的能力以及特性选择配置选项。配置选定后，编译应用并生成目标bin文件。将bin文件烧录到你的板子上，进行功能验证。如果功能验证通过，可将此时的配置保存成默认配置，并将该默认配置保存到应用的 config 文件夹下，供其他开发者选择。

> 例：基于 TUYA_T5AI_EVB 适配 your_chat_bot 这个应用工程，并生成适配该板子的默认配置。

回到 TuyaOpen 根路径

```shell
cd ../..
```

进入 your_chat_bot 应用

```shell
cd apps/tuya.ai/your_chat_bot
```

进行配置，板子选择 TUYA_BOARD_EVB

```
tos menuconfig
```

编译工程生成目标 bin 文件

```shell
tos build
```

验证功能通过后，将此时的配置保存成默认配置

```shell
tos savedef
```

将默认配置保存到应用的 config 文件夹下

```
cp app_default.config ./config/TUYA_T5AI_EVB.config
```

下次开发者如果想要在 TUYA_T5AI_EVB  上运行该应用，可以直接选择这个板子的配置。

```
tos config_choice
```

### 贡献代码

可参考 [贡献代码指导](contribute_guide.md) 将新增的板子合到 tuyaopen 的主分支上，让更多的开发者可以用到你的开发板。