# GPIO

GPIO（General Purpose Input/Output）即通用输入输出接口，是嵌入式系统中用于**控制外部设备或读取外部信号**的基本数字接口。每个 GPIO 引脚可以被软件配置为输入或输出模式，在输入模式下可以读取引脚的电平状态（高电平或低电平），在输出模式下可以控制引脚输出高电平或低电平。GPIO 还支持中断功能，当引脚电平发生变化时可以触发中断处理函数，实现事件驱动的程序设计。

本示例的代码主要向开发者演示 GPIO 的输入，输出以及中断的功能。关于 GPIO 接口的详细说明请查看: [TKL_GPIO](https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_gpio)。

## 使用指导

### 前置条件

由于每个开发平台上的资源不一样，并不会支持所有外设。
在编译运行该示例代码前，您需要检查 `board/<目标开发平台，如 T5AI>/TKL_Kconfig` 中确认使能配置是否默认打开：

```
config ENABLE_GPIO
    bool
    default y
```

在运行本示例工程前要确认基础的 [环境搭建](https://www.tuyaopen.ai/zh/docs/quick-start/enviroment-setup) 已经完成。

### 选择配置文件

在编译示例工程之前需要根据自己的目标开发平台选择对应的配置文件。

- 进入本示例工程目录（假设当前路径是在 TuyaOpen 仓库的根目录下）, 请执行以下命令：

  ```shell
  cd examples/peripherals/gpio 
  ```

- 进入选择配置文件的菜单，请执行以下命令：

  ```shell
  tos.py config choice
  ```

  命令执行完成后，终端会显示类似以下界面：

  ```
  --------------------
  1. BK7231X.config
  2. ESP32-C3.config
  3. ESP32-S3.config
  4. ESP32.config
  5. EWT103-W15.config
  6. LN882H.config
  7. T2.config
  8. T3.config
  9. T5AI.config
  10. Ubuntu.config
  --------------------
  Input "q" to exit.
  Choice config file: 
  ```

- 根据目标开发平台输入对应配置文件的编号然后按回车键。如选择 T5AI 平台，请输入数字 "9" 后按回车键，终端会显示以下界面：

  ```shell
  Choice config file: 9
  [INFO]: Initialing using.config ...
  [NOTE]: Choice config: /home/share/samba/TuyaOpen/boards/T5AI/config/T5AI.config
  ```

### 运行准备

- **参数配置**

  GPIO 的输入引脚，输出引脚，中断引脚可通过 Kconfig (配置文件路径：./Kconfig)配置。

  - 进入 Kconfig 配置菜单界面， 请执行以下命令：

    ```
    tos.py config menu
    ```

    命令执行完成后，终端会显示类似以下界面：

    ```
    configure project  --->
    Application config  --->
    Choice a board (T5AI)  --->
    configure tuyaopen  --->
    ```

  - 按上下方向键选择子菜单，选择应用配置子菜单 ( Application config ) 按回车键进入。

    进入应用配置的菜单后，终端会显示类似以下界面：

    ```shell
    (26) output pin
    (7) input pin
    (6) irq pin
    ```

    工程会给定一个默认参数，如果您想修改配置可以按上下键选修改项，选定后按回车键可进行修改，修改完成后按Q键和Y键保存退出。

- **硬件连接**

  将上述配置的输出脚接与输入脚相接，中断检测引脚接在一个高电平有效的按键（按下按键后连接高电平）。

### 编译烧录

- 编译工程，请执行以下指令：

  ```
  tos.py build
  ```

  工程编译成功后，终端会出现类似以下界面:

  ```
  [NOTE]: 
  ====================[ BUILD SUCCESS ]===================
   Target    : gpio_QIO_1.0.0.bin
   Output    : /home/share/samba/TuyaOpen/examples/peripherals/gpio/dist/gpio_1.0.0
   Platform  : T5AI
   Chip      : T5AI
   Board     : TUYA_T5AI_BOARD
   Framework : base
  ========================================================
  ```

- 烧录固件，请执行以下指令：

  ```
  tos.py flash
  ```

### 运行结果

- 查看日志，请执行以下指令：

  ```shell
  tos.py monitor
  ```

​	如果烧录和查看日志的步骤出现问题，请阅读 [烧录和日志](https://www.tuyaopen.ai/zh/docs/quick-start/firmware-burning) 。

- 如果 GPIO 输入输出正常，会打印类似如下日志：

  ```
  [01-01 00:00:00 TUYA I][example_gpio.c:xx] pin output high
  [01-01 00:00:00 TUYA I][example_gpio.c:xx] GPIO read high level
  [01-01 00:00:02 TUYA I][example_gpio.c:xx] pin output low
  [01-01 00:00:02 TUYA I][example_gpio.c:xx] GPIO read low level
  [01-01 00:00:04 TUYA I][example_gpio.c:xx] GPIO interrupt triggered
  ```

- 按一下按键，中断引脚检测上升沿后，会打印类似如下日志：

  ```
  ------------ GPIO IRQ Callbcak ------------
  ```

## 示例说明

### 流程图

```mermaid
flowchart TB
    id1[系统环境]
    id2{是否是 Linux 环境}
    id3["user_main() 函数"]
    id4["tuya_app_main() 函数"]
    id5[日志系统初始化]
    id6[GPIO 参数配置]
    id7[GPIO 初始化]
    id8[中断配置和使能]
    id9[GPIO 任务循环<br/>输出控制和输入读取]
    
    id1 --> id2
    id2 --是：直接进入--> id3
    id2 --否：进入--> id4
    id4 --创建 user_main 线程--> id3
    id3 --> id5
    id5 --> id6
    id6 --> id7
    id7 --> id8
    id8 --> id9
```

### 流程说明

1. 系统初始化：如果是 Linux 环境，直接调用 user_main()。其他环境则进入 tuya_app_main() 创建 user_main() 线程。
2. 调用 tal_log_init() 初始化日志系统。
3. 初始化 GPIO 配置结构体，设置输出引脚、输入引脚和中断引脚参数。
4. 调用 tkl_gpio_init() 函数对 GPIO 进行初始化。
5. 调用 tkl_gpio_irq_init() 函数对中断进行初始化，调用 tkl_gpio_irq_enable() 函数使能中断。
6. 进入任务循环：周期性改变输出引脚状态，读取输入引脚状态，处理中断事件。

## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOpen：https://www.tuyaopen.ai/zh

- GitHub：https://github.com/tuya/TuyaOpen