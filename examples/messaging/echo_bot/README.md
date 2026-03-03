# Echo Bot

[English](#overview) | [中文](#概述)

## Overview

Echo Bot is a standalone messaging demo built on [TuyaOpen](https://github.com/tuya/TuyaOpen). It connects to **Telegram**, **Discord**, or **Feishu (Lark)** and echoes back every message it receives — no LLM, no cloud AI, just a clean round-trip through the IM channel.

Use it to verify your bot credentials, test network/proxy connectivity, or as a starting point for building your own messaging integrations on TuyaOpen-supported hardware (T5AI, ESP32, Raspberry Pi, Linux, etc.).

### Features

- **Multi-channel support** — Telegram (long-poll), Discord (WebSocket Gateway), Feishu (WebSocket + Protobuf).
- **Runtime configuration** — Switch channels, set tokens, and configure proxy via CLI commands. All settings persist in NVS (KV storage).
- **HTTP / SOCKS5 proxy** — Route all API traffic through a proxy when direct access is unavailable.
- **WiFi management** — Connect, scan, and switch WiFi networks from the serial console (on WiFi-capable targets).
- **Cross-platform** — Runs natively on Linux as a regular ELF binary, or on embedded targets like Tuya T5AI.

### Architecture

```
+-----------+     inbound queue      +-----------+     outbound queue     +------------+
|  Channel  | ---( im_msg_t )-----→ | Echo Loop | ---( im_msg_t )-----→ | Outbound   |
|  Drivers  |                        |  (copy)   |                        | Dispatcher |
| TG/DC/FS  | ←---( send_message )-- +-----------+                        +------------+
+-----------+                                                                   |
                                                                    send_message(channel)
```

Inbound messages arrive from the active channel driver, pass through a message bus queue, get copied by the echo loop, and are dispatched back to the same channel.

### Project Structure

```
echo_bot/
├── CMakeLists.txt              # Top-level build script
├── app_default.config          # Default board config (T5AI)
├── config/
│   ├── Linux.config            # Linux-specific overrides
│   └── T5AI.config             # T5AI-specific overrides
├── include/
│   ├── echo_base.h             # App-level platform adapter (logging, KV helpers)
│   ├── echo_config.h           # App-level compile-time config (WiFi defaults)
│   └── wifi_manager.h          # WiFi management API
├── src/
│   ├── tuya_app_main.c         # Entry point, channel init, echo loop
│   ├── cli_echo.c              # CLI command handlers
│   ├── echo_cli.h              # CLI init declaration
│   └── wifi/
│       └── wifi_manager.c      # WiFi connect / scan / status
└── IM/                         # Reusable IM component (see IM/README.md)
    ├── im_platform.h           # Platform adapter (logging, memory, KV)
    ├── im_config.h             # IM compile-time defaults & secrets
    ├── im_api.h                # Single-header include for all IM APIs
    ├── im_utils.h / .c         # Shared utilities (string, HTTP, JSON, hash)
    ├── bus/
    │   └── message_bus.h / .c  # Inbound / outbound message queues
    ├── channels/
    │   ├── telegram_bot.h / .c # Telegram Bot driver
    │   ├── discord_bot.h / .c  # Discord Bot driver
    │   └── feishu_bot.h / .c   # Feishu Bot driver
    ├── proxy/
    │   └── http_proxy.h / .c   # HTTP CONNECT / SOCKS5 tunnel
    ├── certs/
    │   ├── tls_cert_bundle.h/.c# Domain cert query (iot-dns + builtin CA)
    │   └── ca_bundle_mini.h/.c # Builtin CA certificate bundle
    └── CMakeLists.txt
```

### Prerequisites

- TuyaOpen SDK cloned and environment initialized (`. ./export.sh` from repo root).
- A bot token / app credential for at least one channel:
  - **Telegram**: Bot token from [@BotFather](https://t.me/BotFather).
  - **Discord**: Bot token from [Discord Developer Portal](https://discord.com/developers/applications), plus a channel ID.
  - **Feishu**: App ID & App Secret from [Feishu Open Platform](https://open.feishu.cn/), with Event Subscription enabled.

### Build

```bash
cd /path/to/TuyaOpen
. ./export.sh
cd examples/messaging/echo_bot

# Build for default target
tos.py build
```

Output binaries are placed in `dist/`.

### Configure Secrets (Compile-Time)

Create `IM/im_secrets.h` (git-ignored) to embed credentials at compile time:

```c
#define IM_SECRET_TG_TOKEN       "123456:ABC-DEF..."
#define IM_SECRET_CHANNEL_MODE   "telegram"
```

Or for Feishu:

```c
#define IM_SECRET_FS_APP_ID      "cli_xxxx"
#define IM_SECRET_FS_APP_SECRET  "xxxx"
#define IM_SECRET_FS_ALLOW_FROM  "ou_xxxx,ou_yyyy"
#define IM_SECRET_CHANNEL_MODE   "feishu"
```

See `IM/im_config.h` for the full list of overridable macros.

### Run

**Linux:**

```bash
./dist/echo_bot_1.0.0/echo_bot_1.0.0
```

**Embedded (T5AI etc.):**

Flash the firmware via `tos.py flash`, then open a serial console (460800 baud).

### CLI Commands

Once running, the following commands are available via serial console (embedded) or stdin (Linux):

| Command | Description |
|---|---|
| `help` | List all available commands |
| `set_wifi <ssid> <password>` | Set WiFi credentials (embedded only) |
| `wifi_status` | Show WiFi connection status and IP |
| `wifi_scan` | Scan and list nearby WiFi APs |
| `set_channel_mode <mode>` | Switch channel: `telegram`, `discord`, or `feishu` |
| `set_tg_token <token>` | Set Telegram bot token |
| `set_dc_token <token>` | Set Discord bot token |
| `set_dc_channel <id>` | Set Discord default channel ID |
| `set_fs_appid <id>` | Set Feishu App ID |
| `set_fs_appsecret <secret>` | Set Feishu App Secret |
| `set_fs_allow <csv>` | Set Feishu allowed sender open_ids (comma-separated) |
| `config_show` | Display current configuration (tokens are masked) |
| `push_outbound <ch> <chat_id> <text>` | Manually send a message to a channel |
| `restart` | Reboot the device |

All `set_*` commands persist to NVS and take effect after `restart`.

### Quick Start Example (Telegram on Linux)

```bash
# 1. Build
cd examples/messaging/echo_bot && tos.py build -t Linux

# 2. Create secrets file
cat > IM/im_secrets.h << 'EOF'
#define IM_SECRET_TG_TOKEN     "123456:ABC-your-token"
#define IM_SECRET_CHANNEL_MODE "telegram"
EOF

# 3. Rebuild and run
tos.py build -t Linux
./dist/echo_bot_1.0.0/echo_bot_1.0.0

# 4. Send a message to your bot in Telegram — it replies with the same text.
```

### Proxy Support

If your device cannot directly reach Telegram / Discord / Feishu APIs, configure an HTTP or SOCKS5 proxy:

**Compile-time** (in `IM/im_secrets.h`):

```c
#define IM_SECRET_PROXY_HOST  "192.168.1.100"
#define IM_SECRET_PROXY_PORT  "7890"
#define IM_SECRET_PROXY_TYPE  "http"      // "http" or "socks5"
```

**Runtime** (via CLI — not yet exposed, but settable via KV):

Proxy settings are stored in NVS under `proxy_config` namespace with keys `host`, `port`, `proxy_type`.

---

## 概述

Echo Bot 是基于 [TuyaOpen](https://github.com/tuya/TuyaOpen) 构建的独立即时通讯演示项目。它连接到 **Telegram**、**Discord** 或**飞书**，并原样回复收到的每一条消息——无 LLM、无云端 AI，仅验证消息通道的完整收发链路。

可用于验证 Bot 凭证、测试网络/代理连通性，或作为在 TuyaOpen 支持的硬件（T5AI、ESP32、树莓派、Linux 等）上构建自定义消息集成的起点。

### 功能特性

- **多通道支持** — Telegram（长轮询）、Discord（WebSocket Gateway）、飞书（WebSocket + Protobuf）。
- **运行时配置** — 通过 CLI 命令切换通道、设置 Token、配置代理，所有设置持久化到 NVS（KV 存储）。
- **HTTP / SOCKS5 代理** — 当无法直接访问 API 时，可通过代理路由所有流量。
- **WiFi 管理** — 在支持 WiFi 的目标上，通过串口控制台连接、扫描和切换 WiFi 网络。
- **跨平台** — 在 Linux 上以普通 ELF 可执行文件原生运行，或在 Tuya T5AI 等嵌入式目标上运行。

### 架构

```
+-----------+     入站队列           +-----------+     出站队列           +------------+
|  通道驱动  | ---( im_msg_t )-----→ | Echo 循环  | ---( im_msg_t )-----→ | 出站分发器   |
| TG/DC/FS  |                        |  (复制消息) |                        |            |
|           | ←---( send_message )-- +-----------+                        +------------+
+-----------+                                                                   |
                                                                    send_message(channel)
```

入站消息从活跃的通道驱动到达，经过消息总线队列，由 Echo 循环复制后，通过出站分发器发回同一通道。

### 项目结构

```
echo_bot/
├── CMakeLists.txt              # 顶层构建脚本
├── app_default.config          # 默认板级配置（T5AI）
├── config/
│   ├── Linux.config            # Linux 平台配置覆盖
│   └── T5AI.config             # T5AI 平台配置覆盖
├── include/
│   ├── echo_base.h             # 应用层平台适配（日志、KV 辅助）
│   ├── echo_config.h           # 应用层编译期配置（WiFi 默认值）
│   └── wifi_manager.h          # WiFi 管理接口
├── src/
│   ├── tuya_app_main.c         # 入口、通道初始化、Echo 循环
│   ├── cli_echo.c              # CLI 命令处理
│   ├── echo_cli.h              # CLI 初始化声明
│   └── wifi/
│       └── wifi_manager.c      # WiFi 连接 / 扫描 / 状态
└── IM/                         # 可复用 IM 组件（详见 IM/README.md）
    ├── im_platform.h           # 平台适配层（日志、内存、KV）
    ├── im_config.h             # IM 编译期默认值与密钥
    ├── im_api.h                # 聚合头文件，一次引入所有 IM API
    ├── im_utils.h / .c         # 公共工具（字符串、HTTP、JSON、哈希）
    ├── bus/
    │   └── message_bus.h / .c  # 入站 / 出站消息队列
    ├── channels/
    │   ├── telegram_bot.h / .c # Telegram Bot 驱动
    │   ├── discord_bot.h / .c  # Discord Bot 驱动
    │   └── feishu_bot.h / .c   # 飞书 Bot 驱动
    ├── proxy/
    │   └── http_proxy.h / .c   # HTTP CONNECT / SOCKS5 隧道
    ├── certs/
    │   ├── tls_cert_bundle.h/.c# 域名证书查询（iot-dns + 内置 CA 回退）
    │   └── ca_bundle_mini.h/.c # 内置 CA 证书包
    └── CMakeLists.txt
```

### 前置条件

- 已克隆 TuyaOpen SDK 并初始化环境（在仓库根目录执行 `. ./export.sh`）。
- 至少拥有一个通道的 Bot 凭证：
  - **Telegram**：通过 [@BotFather](https://t.me/BotFather) 获取 Bot Token。
  - **Discord**：从 [Discord 开发者门户](https://discord.com/developers/applications) 获取 Bot Token，并准备一个频道 ID。
  - **飞书**：从[飞书开放平台](https://open.feishu.cn/) 获取 App ID 和 App Secret，并开启事件订阅。

### 编译

```bash
cd /path/to/TuyaOpen
. ./export.sh
cd examples/messaging/echo_bot

# 默认目标编译
tos.py build
```

产物输出在 `dist/` 目录。

### 配置密钥（编译期）

创建 `IM/im_secrets.h`（已被 git 忽略），在编译时嵌入凭证：

```c
#define IM_SECRET_TG_TOKEN       "123456:ABC-DEF..."
#define IM_SECRET_CHANNEL_MODE   "telegram"
```

或飞书配置：

```c
#define IM_SECRET_FS_APP_ID      "cli_xxxx"
#define IM_SECRET_FS_APP_SECRET  "xxxx"
#define IM_SECRET_FS_ALLOW_FROM  "ou_xxxx,ou_yyyy"
#define IM_SECRET_CHANNEL_MODE   "feishu"
```

完整可覆盖宏列表参见 `IM/im_config.h`。

### 运行

**Linux 平台：**

```bash
./dist/echo_bot_1.0.0/echo_bot_1.0.0
```

**嵌入式平台（T5AI 等）：**

通过 `tos.py flash` 烧录固件，然后打开串口控制台（波特率 460800）。

### CLI 命令

运行后，可通过串口控制台（嵌入式）或标准输入（Linux）使用以下命令：

| 命令 | 说明 |
|---|---|
| `help` | 列出所有可用命令 |
| `set_wifi <ssid> <password>` | 设置 WiFi 凭证（仅嵌入式平台） |
| `wifi_status` | 显示 WiFi 连接状态和 IP 地址 |
| `wifi_scan` | 扫描并列出附近 WiFi 热点 |
| `set_channel_mode <mode>` | 切换通道：`telegram`、`discord` 或 `feishu` |
| `set_tg_token <token>` | 设置 Telegram Bot Token |
| `set_dc_token <token>` | 设置 Discord Bot Token |
| `set_dc_channel <id>` | 设置 Discord 默认频道 ID |
| `set_fs_appid <id>` | 设置飞书 App ID |
| `set_fs_appsecret <secret>` | 设置飞书 App Secret |
| `set_fs_allow <csv>` | 设置飞书允许的发送者 open_id（逗号分隔） |
| `config_show` | 显示当前配置（Token 会脱敏显示） |
| `push_outbound <ch> <chat_id> <text>` | 手动向指定通道发送一条消息 |
| `restart` | 重启设备 |

所有 `set_*` 命令会持久化到 NVS，`restart` 后生效。

### 快速上手示例（Linux + Telegram）

```bash
# 1. 编译
cd examples/messaging/echo_bot && tos.py build -t Linux

# 2. 创建密钥文件
cat > IM/im_secrets.h << 'EOF'
#define IM_SECRET_TG_TOKEN     "123456:ABC-你的Token"
#define IM_SECRET_CHANNEL_MODE "telegram"
EOF

# 3. 重新编译并运行
tos.py build -t Linux
./dist/echo_bot_1.0.0/echo_bot_1.0.0

# 4. 在 Telegram 中给你的 Bot 发消息，它会回复相同内容。
```

### 代理支持

如果设备无法直接访问 Telegram / Discord / 飞书 API，可配置 HTTP 或 SOCKS5 代理：

**编译期配置**（在 `IM/im_secrets.h` 中）：

```c
#define IM_SECRET_PROXY_HOST  "192.168.1.100"
#define IM_SECRET_PROXY_PORT  "7890"
#define IM_SECRET_PROXY_TYPE  "http"      // "http" 或 "socks5"
```

**运行时配置**：

代理设置存储在 NVS 的 `proxy_config` 命名空间下，键名为 `host`、`port`、`proxy_type`。
