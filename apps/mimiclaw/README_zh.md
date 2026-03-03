## 在涂鸦 T5AI 上运行 MimiClaw -  TuyaOpen 移植版

本项目是在 **涂鸦 T5AI 开发板** 以及可选的 **Linux 运行时** 上运行 MimiClaw 开源项目的 TuyaOpen 移植版，用于演示和开发 MimiClaw 风格的本地优先 AI 助手。  
插上电、连上 Wi‑Fi，这块几十块钱的 T5AI 板子就会变成一个可以 7×24 小时运行的超级 AI 管家。

你可以通过 **飞书 / Telegram / Discord** 直接和它对话，把任务丢给它处理；  
它支持 **DeepSeek、Claude、GPT 等主流大模型**，可以在不同模型和兼容接口之间灵活切换；  
它有本地记忆，所有对话和偏好都以纯文本文件保存在芯片里，重启不丢失，用得越久越懂你。

它还内置了 **定时任务调度器**，可以让 AI 自己安排每日提醒、定时汇总；  
内置 **心跳检测机制**，定时读取任务清单，自动唤醒执行任务——不需要你催，它自己会动。  

更重要的是，依托 TuyaOpen 丰富的 **驱动、显示和传感器外设能力**，它可以读取板载传感器、控制 GPIO，把 AI 真正“接到”各种硬件设备上——  
无论是 AI 台灯、逗宠机器人还是 AI 玩具，都可以快速拥有一个功能完整、可自我进化的 AI 管家。

**免责声明 / 商标说明**：本项目是社区维护的 TuyaOpen 移植版，用于在涂鸦 T5AI 硬件上运行 MimiClaw 开源项目，与涂鸦官方、MimiClaw 原始项目维护者以及文中提到的各大模型 / 平台均无隶属、合作或背书关系。“MimiClaw”、“OpenClaw”、“Telegram”、“Discord”、“飞书”等名称及商标归各自权利人所有。

---

## 与上游 MimiClaw 项目的差异

上游开源仓库：[`memovai/mimiclaw`](https://github.com/memovai/mimiclaw)

本项目基于上游 MimiClaw 开源项目实现，是一个面向 TuyaOpen 生态的第三方移植版，保留了其核心思想（嵌入式 Agent 循环、本地记忆、工具调用、定时任务与心跳机制），并针对 TuyaOpen 生态做了以下扩展和改造：

- **目标硬件与运行环境**
  - 新增对 **涂鸦 T5AI 开发板** 的一等公民支持，集成 TuyaOpen SDK、显示屏和硬件外设
  - 提供 **Linux Runtime**，方便在 PC / 云端调试、开发和快速验证 Agent 逻辑
- **对话通道**
  - 在原本 Telegram 的基础上，增加了 **飞书** 与 **Discord** 支持
  - 通过统一的 CLI 命令切换当前使用的社交通道
- **大模型配置方式**
  - 支持 **OpenAI** 与 **Anthropic**，同时支持 **兼容 OpenAI/Anthropic 协议的自建 / 第三方 URL**
  - 可通过命令行与 `mimi_secrets.h` 双层配置模型与 API
- **与 TuyaOpen 深度集成**
  - 利用 TuyaOpen 的 **驱动、传感器与显示能力**，让 AI 能够读取环境数据、控制 GPIO 以及更多硬件外设
  - 采用 TuyaOpen 的示例工程结构，位于 `apps/tuya_mimiclaw` 目录下，便于与其他示例一起管理和构建

关于 MimiClaw 自身的架构和实现细节，可参考上游仓库文档；本说明重点介绍 TuyaOpen 移植与使用方式。

---

## 迁移与配置说明

以下步骤默认你已经完成固件编译并烧录到板子（或已在 Linux Runtime 中启动进程）。  
通过串口工具以 **115200 波特率** 打开调试口，进入命令行界面。

### 1. 查看命令行工具

串口连接成功后，在命令行中输入：

```text
help
```

即可获取到当前固件支持的命令行工具列表和帮助信息。

### 2. 配置大模型（OpenAI / Anthropic / 兼容 URL）

你可以选择 `openai` 或 `anthropic` 作为模型提供方，也可以使用 **OpenAI / Anthropic 协议兼容的自建或第三方接口**。

典型配置流程：

1. 选择模型提供方：
   - `set_model_provider openai`
   - 或 `set_model_provider anthropic`
2. 设置 API Key：
   - `set_api_key sk-xxxx...`
3. （可选）设置兼容的 API Base URL：
   - `set_api_url https://your-compatible-endpoint.example.com`
4. 设置模型名称：
   - `set_model gpt-4o`（或你所使用提供方支持的任意模型）

如果你选择“兼容 URL”模式，**必须**同时设置 `API_KEY` 和 `API_URL`，缺一不可，否则接口调用会失败。

对应你原始提到的迁移说明，可以理解为：

1. **115200 打开烧录口，输入 `help`，可以获取到命令行工具**
2. **配置大模型，选择 `openai` 或者 `anthropic`，然后输入 `API_KEY` 和 模型；如果使用兼容 URL，需要额外输入 `API_URL`**

### 3. 连接路由器（Wi‑Fi 配网）

使用命令行配置路由器 SSID 与密码：

```text
set_wifi <ssid> <password>
restart
```

执行 `restart` 后，设备会按新配置重启并重新连接网络。

对应你的迁移说明：

3. **连接路由器：`set_wifi xxx xxx` 设置路由器，设置完成后重启 `restart` 命令**

### 4. 切换社交账号 / 对话通道（Telegram / Discord / 飞书）

当前固件支持通过多种通道与 MimiClaw 对话：

- `telegram`
- `discord`
- `feishu`（飞书）

切换通道示例：

```text
set_channel_mode telegram
```

或：

```text
set_channel_mode feishu
```

或：

```text
set_channel_mode discord
```

根据所选通道，在命令行中输入对应的 `token` / `appid` / `secret` 等认证信息。完成配置后，同样需要重启：

```text
restart
```

对应你的迁移说明：

4. **切换社交账号目前支持 telegram/discord/feishu，`set_channel_mode telegram` 或者 `set_channel_mode feishu`，并根据相关账号要求输入 token/appid 等信息，设置完成后重启 `restart` 命令**

### 5. 使用 `mimi_secrets.h` 一次性配置（可选）

如果你更习惯在编译前一次性写好配置，可以直接复制示例文件：

```bash
cp apps/tuya_mimiclaw/mimi_secrets.h.example apps/tuya_mimiclaw/mimi_secrets.h
```

然后编辑 `mimi_secrets.h`，填入：

- Wi‑Fi 名称与密码
- 大模型 API Key、模型名称
- 模型提供方（OpenAI / Anthropic / 兼容 URL）
- 对话通道及必要的 Token / AppID 等

保存后重新编译并烧录固件即可。

对应你的迁移说明：

5. **也可以直接复制 `apps/tuy_mimiclaw/mimi_secrets.h.example` 并重命名为 `mimi_secrets.h` 修改文件上的配置，并重新编译**  
（本仓库中目录名为 `apps/tuya_mimiclaw`，请以实际路径为准）

6. **存储切换**
  
在 `apps/tuya_mimiclaw/mimi_config.h` 中

```
#if MIMI_USE_SDCARD
#define MIMI_FS_BASE "/spiffs"
#else
#define MIMI_FS_BASE "/spiffs"
#endif
```c

7. **搜索引擎切换**

在 `apps/tuya_mimiclaw/mimi_config.h` 中

```
/* Search Engine Selection: 0 = Brave, 1 = Baidu */
#ifndef MIMI_USE_BAIDU_SEARCH
#define MIMI_USE_BAIDU_SEARCH 0
#endif
```c


### 6. 配置优先级规则

配置优先级（从高到低）如下：

1. **命令行输入（CLI）**
2. `mimi_secrets.h` 中的编译期默认值

如果某个参数在命令行中已经设置，则总是优先生效；  
命令行中没有设置的参数会退回到 `mimi_secrets.h` 的默认值；  
如果两处都没有提供必要参数，则会导致对应功能报错或无法启动。

对应你的迁移说明：

6. **所有配置以命令行输入的优先级最高，命令行中没有的参数会采用 `mimi_secrets.h` 中的参数，否则就会报错**

---

## 支持的芯片 / 运行环境

| 名称           | 支持状态 | 说明                                          |
| -------------- | -------- | --------------------------------------------- |
| 涂鸦 T5AI 开发板 | 支持     | 主要嵌入式目标，使用 TuyaOpen SDK 与硬件栈。 |
| Linux Runtime  | 支持     | 直接在 Linux 主机上运行 Agent，用于开发调试。 |

---

## 致谢与版权说明

本项目深受上游开源项目 [`memovai/mimiclaw`](https://github.com/memovai/mimiclaw) 启发，并在其基础上进行移植与扩展。

上游 MimiClaw 项目展示了如何在一块 ESP32‑S3 小板上，以纯 C 代码实现：

- 不依赖 Linux / Node.js 的轻量级运行时
- 基于文本文件的本地记忆（如 `SOUL.md`、`USER.md`、`MEMORY.md` 等）
- 工具调用、定时任务（Cron）与心跳驱动的自主执行机制

本仓库在此基础上，将 MimiClaw 带入 **TuyaOpen 生态**：

- 适配涂鸦 T5AI 硬件、显示与外设
- 与 TuyaOpen 的 AI / IoT 工作流打通
- 扩展更多模型 / 通道选项，面向真实产品落地场景

上游 MimiClaw 采用 **MIT License** 授权；  
本项目在遵循其许可证的前提下进行二次开发，并在此对原作者及社区表示由衷感谢。

