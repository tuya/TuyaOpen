[English](./README.md) | 简体中文

# your_robot_dog
[your_robot_dog](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_robot_dog) 是基于 TuyaOS 的 `tuyaos_demo_ai_toy` 项目在 TuyaOpen 的 `your_char_bot` 基础上移植而来，添加机器狗生动表情变化与控制舵机动作，移植开源的大模型智能聊天机器狗。通过麦克风采集语音，语音识别，实现对话、互动、调侃，还能通过屏幕看到情感变化，以及互动行为。

![](../../../docs/images/apps/robot_dog.png)

## 支持功能
1. AI 智能对话
2. 按键唤醒 / 语音唤醒，回合制对话，支持语音打断（需硬件支持）
3. 表情显示
4. 支持 LCD 显示实时聊天内容，支持 App 端实时查看聊天内容
5. App 端实时切换 AI 智能体角色
6. 语音控制机器狗行为

## 依赖硬件能力
1. 音频采集
2. 音频播放
3. 舵机驱动

## 已支持硬件
| 型号 | config |
| --- | --- |
| TUYA_T5AI_ROBOT_DOG | TUYA_T5AI_ROBOT_DOG.config |

## 硬件烧录方式
自备 CH340，连接方式：
TX  -------------- RX0
RX  -------------- TX0
RST -------------- RST

查看串口日志方式：
TX  -------------- RX_L
RX  -------------- TX_L
GND -------------- GND
必须共地，不然日志可能乱码。

## 编译
1. 运行 `tos.py config choice` 命令，选择 `TUYA_T5AI_ROBOT_DOG.config`。
2. 如需修改配置，请先运行 `tos.py config menu` 命令修改配置。
3. 运行 `tos.py build` 命令，编译工程。

## 使用说明

### 首次启动与配网

设备第一次启动时，上方状态栏会显示“配网中”，此时即可开始配网。

当满足以下条件时，表示配网成功：

- WiFi 图标显示有 WiFi 信号
- 上方状态栏从“配网中”变为“待命”

当机器小狗说“我已联网，让我们开始对话吧”时，即可进行后续对话。

### 对话模式

一共四种对话模式：

1. 长按对话模式
2. 按键对话模式
3. 唤醒对话模式
4. 随意对话模式

### 切换对话模式

连续快速按两下按钮以切换对话模式。切换成功之后，小狗会说：“长按对话/按键对话/唤醒对话/随意对话”。

## 配置说明

### 默认配置
- 随意对话模式，未开启 AEC，不支持打断
- 唤醒词：
	- T5AI 版本：你好涂鸦

### 通用配置

- **选择语言**

	运行 `tos.py config menu` 命令，在 choose ai language 选项中选择 `Chinese` 或 `English` 。

- **选择对话模式**

	- 长按对话模式

		| 选项 | 说明 |
		| --- | --- |
		| enable ai mode hold | 按住按键后说话，一句话说完后松开按键。 |

	- 按键对话模式

		| 选项 | 说明 |
		| --- | --- |
		| enable ai mode oneshot | 按一下按键，设备会进入/退出聆听状态。如果在聆听状态，会开启 VAD 检测，此时可以进行对话。 |

	- 唤醒对话模式

		| 选项 | 说明 |
		| --- | --- |
		| enable ai mode wakeup | 需要说出唤醒词才能唤醒设备，设备唤醒后会进入聆听状态，此时可以进行对话。每次唤醒只能进行一轮对话。如果想继续对话，需要再次用唤醒词唤醒。 |

	- 随意对话模式

		| 选项 | 说明 |
		| --- | --- |
		| enable ai mode free | 需要说出唤醒词才能唤醒设备，设备唤醒后会进入聆听状态，此时可以进行随意对话。如果 30s 没有检测到声音，则需要再次唤醒。 |

- **使能显示**

	| 选项 | 说明 |
	| --- | --- |
	| enable ai chat display ui | 使能显示功能，如果板子有带屏幕，可将该功能打开。 |
	| enable the dog action | 使能小狗动作。 |

### 文件系统配置

必要配置：机器狗部分表情 GIF 已打包为 LittleFS 镜像，位于 `./src/display/emotion/fs/fs.bin`，必须将其烧入 FLASH 中指定地址处。

若不配置可能导致系统异常重启或小狗表情显示不全。

步骤如下：
1. 下载 TuyaOpen 官方烧录工具 TyuTool，TyuTool 是一款为物联网（IoT）开发者设计的、跨平台的串口工具，用于烧录和读取多种主流芯片的固件。
2. 进入后在“Operate”中选中“Write”，在“File In”中选中 `fs.bin` 的路径。
3. 在“Start”中配置 `fs.bin` 的起始地址，配置起始地址为 `0x6cb000`，点击下方“Start”按键开始烧录。

![](../../../docs/images/apps/TyaTool_1.png)

4. 如图所示即烧录成功。

![](../../../docs/images/apps/TyaTool_2.png)

#### 如何自己生成 fs.bin（可选）

若需要增删表情，可本地重新打包：

1. 准备 GIF  
   把想要的表情文件重命名为：  
   `angry.gif confused.gif disappointed.gif embarrassed.gif happy.gif laughing.gif relaxed.gif sad.gif happy.gif surprise.gif thinking.gif `  
   并统一放到任意空目录，例如 `/tmp/gif_pack`。
   注意：所有 GIF 总和必须 < 1 MB。设备端代码通过 /angry.gif 这类绝对路径加载，务必保持代码里的命名一致。

2. 一键打包  
   使用mklittlefs工具，工具使用说明位于TuyaOpen/platform/T5AI/t5_os/ap/components/littlefs/mkimg/README.md
   在 TuyaOpen 根目录执行：

   platform/T5AI/t5_os/ap/components/littlefs/mkimg/mklittlefs \
     -c /tmp/gif_pack \
     -b 4096 -p 256 -s 1048576 \
     apps/tuya.ai/your_robot_dog/fs.bin

	执行完该命令可以看到生成的 `fs.bin` 在 apps/tuya.ai/your_robot_dog 目录下。

	参数含义：
	-c 源目录 -b 块大小 -p 页大小 -s 镜像总大小（1 MB）

## 补充说明
`your_robot_dog` 为移植项目，TUYA_T5AI_ROBOT_DOG 的底板与常规 T5AI 开发板对比有较大差异。

暂未支持摄像头功能。
