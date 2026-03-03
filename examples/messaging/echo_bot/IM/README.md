# IM Component

[English](#overview) | [中文](#概述)

## Overview

A self-contained, reusable instant-messaging component for [TuyaOpen](https://github.com/tuya/TuyaOpen). It provides a unified message bus, multi-channel drivers (Telegram / Discord / Feishu), HTTP/SOCKS5 proxy tunneling, and TLS certificate management.

The entire `IM/` directory can be copied into any TuyaOpen project. The only external dependency is `tal_api.h` from the TuyaOpen platform SDK.

### Supported Channels

| Channel | Protocol | Inbound | Outbound |
|---|---|---|---|
| **Telegram** | HTTPS long-poll (`getUpdates`) | Text, document metadata | Text (Markdown + plain fallback) |
| **Discord** | WebSocket Gateway (v10) | Text, attachment metadata | Text (REST API) |
| **Feishu (Lark)** | WebSocket + Protobuf frames | Text, post, interactive card, share | Text (REST API) |

### Key Design

- **`im_platform.h`** — Platform adapter layer. All IM code goes through this single header for logging (`IM_LOG*`), memory (`im_malloc` / `im_free` / `im_calloc` / `im_realloc` / `im_strdup`), and KV storage (`im_kv_*`). Porting to a non-TuyaOpen platform only requires reimplementing this file.
- **`im_api.h`** — Single-include convenience header that pulls in the entire public API.
- **Message bus** — Lock-free inbound/outbound queues (`im_msg_t`) decouple channel drivers from application logic.
- **Proxy** — Transparent HTTP CONNECT / SOCKS5 tunneling; channel drivers automatically use it when configured.
- **TLS certs** — Queries Tuya iot-dns for domain certificates, falls back to a built-in CA bundle for public hosts (Telegram, Discord, Feishu).

## Directory Structure

```
IM/
├── im_platform.h           # Platform adapter (logging, memory, KV storage)
├── im_config.h             # Compile-time defaults (secrets, timeouts, NVS keys)
├── im_api.h                # Single-include public API header
├── im_utils.h / .c         # Shared utilities (string, HTTP parse, JSON, FNV hash)
├── bus/
│   └── message_bus.h / .c  # Inbound / outbound message queues (im_msg_t)
├── channels/
│   ├── telegram_bot.h / .c # Telegram Bot — HTTPS long-poll + send
│   ├── discord_bot.h / .c  # Discord Bot — WebSocket Gateway + REST send
│   └── feishu_bot.h / .c   # Feishu Bot — WebSocket + Protobuf + REST send
├── proxy/
│   └── http_proxy.h / .c   # HTTP CONNECT / SOCKS5 proxy tunnel
├── certs/
│   ├── tls_cert_bundle.h/.c# Domain cert query (iot-dns + builtin CA fallback)
│   └── ca_bundle_mini.h/.c # Builtin CA certificate bundle (ISRG Root X1 etc.)
└── CMakeLists.txt           # Auto-registers sources & includes into ${EXAMPLE_LIB}
```

## Configuration

Configuration is layered, from lowest to highest priority:

| Layer | File / Mechanism | Description |
|---|---|---|
| **Defaults** | `im_config.h` | API hosts, timeouts, thread stacks, NVS key names |
| **Compile-time secrets** | `im_secrets.h` (git-ignored) | Bot tokens, app IDs, proxy settings |
| **Runtime overrides** | NVS (KV storage) | CLI or programmatic `im_kv_set_string()` calls |

### `im_secrets.h` Example

```c
// Telegram
#define IM_SECRET_TG_TOKEN       "123456:ABC-DEF..."
#define IM_SECRET_CHANNEL_MODE   "telegram"

// Discord
#define IM_SECRET_DC_TOKEN       "MTIz..."
#define IM_SECRET_DC_CHANNEL_ID  "1234567890"

// Feishu
#define IM_SECRET_FS_APP_ID      "cli_xxxx"
#define IM_SECRET_FS_APP_SECRET  "xxxx"
#define IM_SECRET_FS_ALLOW_FROM  "ou_xxxx,ou_yyyy"

// Proxy (optional)
#define IM_SECRET_PROXY_HOST     "192.168.1.100"
#define IM_SECRET_PROXY_PORT     "7890"
#define IM_SECRET_PROXY_TYPE     "http"    // "http" or "socks5"
```

## Integration

### CMake

In your project's `CMakeLists.txt`, add the IM subdirectory **after** `add_library`:

```cmake
add_library(${EXAMPLE_LIB})
# ... your sources ...
add_subdirectory(${APP_PATH}/IM)
```

The IM `CMakeLists.txt` automatically registers all sources and include paths into `${EXAMPLE_LIB}`.

### Code

Include the single convenience header, or individual modules as needed:

```c
#include "im_api.h"
```

### Typical Init Sequence

```c
// 1. Initialize subsystems
message_bus_init();
http_proxy_init();
telegram_bot_init();    // or discord_bot_init() / feishu_bot_init()

// 2. Start channel driver (spawns background thread)
telegram_bot_start();

// 3. Consume inbound messages
while (1) {
    im_msg_t msg = {0};
    if (message_bus_pop_inbound(&msg, timeout_ms) == OPRT_OK) {
        // msg.channel  = "telegram" / "discord" / "feishu"
        // msg.chat_id  = sender / chat identifier
        // msg.content  = message text (caller must free)
        process(msg);
        free(msg.content);
    }
}
```

### Public API Summary

| Module | Function | Description |
|---|---|---|
| **message_bus** | `message_bus_init()` | Initialize inbound & outbound queues |
| | `message_bus_push_inbound(msg)` | Push a message to the inbound queue |
| | `message_bus_pop_inbound(msg, ms)` | Pop from inbound (blocks up to `ms`) |
| | `message_bus_push_outbound(msg)` | Push a message to the outbound queue |
| | `message_bus_pop_outbound(msg, ms)` | Pop from outbound (blocks up to `ms`) |
| **telegram** | `telegram_bot_init()` | Load token from secrets / NVS |
| | `telegram_bot_start()` | Start long-poll thread |
| | `telegram_send_message(chat_id, text)` | Send text to a chat |
| | `telegram_set_token(token)` | Update token at runtime (persists to NVS) |
| **discord** | `discord_bot_init()` | Load token & channel from secrets / NVS |
| | `discord_bot_start()` | Start WebSocket Gateway thread |
| | `discord_send_message(channel_id, text)` | Send text to a channel |
| | `discord_set_token(token)` | Update token at runtime |
| | `discord_set_channel_id(id)` | Update default channel at runtime |
| **feishu** | `feishu_bot_init()` | Load app credentials from secrets / NVS |
| | `feishu_bot_start()` | Obtain tenant token & start WebSocket thread |
| | `feishu_send_message(chat_id, text)` | Send text to a user or group |
| | `feishu_set_app_id(id)` | Update App ID at runtime |
| | `feishu_set_app_secret(secret)` | Update App Secret at runtime |
| | `feishu_set_allow_from(csv)` | Update allowed sender list |
| **proxy** | `http_proxy_init()` | Load proxy settings from secrets / NVS |
| | `http_proxy_is_enabled()` | Check if proxy is configured |
| | `http_proxy_set(host, port, type)` | Configure proxy at runtime |
| | `http_proxy_clear()` | Remove proxy configuration |

---

## 概述

IM 是一个独立的、可复用的即时通讯组件，基于 [TuyaOpen](https://github.com/tuya/TuyaOpen) 构建。它提供统一的消息总线、多通道驱动（Telegram / Discord / 飞书）、HTTP/SOCKS5 代理隧道和 TLS 证书管理。

整个 `IM/` 目录可直接复制到任意 TuyaOpen 项目中使用，唯一的外部依赖是 TuyaOpen 平台 SDK 提供的 `tal_api.h`。

### 支持的通道

| 通道 | 协议 | 入站消息 | 出站消息 |
|---|---|---|---|
| **Telegram** | HTTPS 长轮询（`getUpdates`） | 文本、文档元数据 | 文本（Markdown + 纯文本回退） |
| **Discord** | WebSocket Gateway (v10) | 文本、附件元数据 | 文本（REST API） |
| **飞书** | WebSocket + Protobuf 帧 | 文本、富文本、卡片消息、分享 | 文本（REST API） |

### 核心设计

- **`im_platform.h`** — 平台适配层。所有 IM 代码通过此头文件统一调用日志（`IM_LOG*`）、内存（`im_malloc` / `im_free` / `im_calloc` / `im_realloc` / `im_strdup`）和 KV 存储（`im_kv_*`）。移植到非 TuyaOpen 平台时只需重新实现此文件。
- **`im_api.h`** — 聚合头文件，一次引入即可访问全部公开 API。
- **消息总线** — 入站/出站队列（`im_msg_t`）将通道驱动与应用逻辑解耦。
- **代理** — 透明的 HTTP CONNECT / SOCKS5 隧道；配置后通道驱动自动使用。
- **TLS 证书** — 通过 Tuya iot-dns 查询域名证书，对公共域名（Telegram、Discord、飞书）回退到内置 CA 证书包。

## 目录结构

```
IM/
├── im_platform.h           # 平台适配层（日志、内存、KV 存储）
├── im_config.h             # 编译期默认配置（密钥、超时、NVS 键名）
├── im_api.h                # 聚合头文件，一次引入全部 API
├── im_utils.h / .c         # 公共工具（字符串、HTTP 解析、JSON、FNV 哈希）
├── bus/
│   └── message_bus.h / .c  # 入站 / 出站消息队列（im_msg_t）
├── channels/
│   ├── telegram_bot.h / .c # Telegram Bot — HTTPS 长轮询 + 发送
│   ├── discord_bot.h / .c  # Discord Bot — WebSocket Gateway + REST 发送
│   └── feishu_bot.h / .c   # 飞书 Bot — WebSocket + Protobuf + REST 发送
├── proxy/
│   └── http_proxy.h / .c   # HTTP CONNECT / SOCKS5 代理隧道
├── certs/
│   ├── tls_cert_bundle.h/.c# 域名证书查询（iot-dns + 内置 CA 回退）
│   └── ca_bundle_mini.h/.c # 内置 CA 证书包（ISRG Root X1 等）
└── CMakeLists.txt           # 自动注册源文件和头文件路径到 ${EXAMPLE_LIB}
```

## 配置

配置分为三层，优先级从低到高：

| 层级 | 文件 / 机制 | 说明 |
|---|---|---|
| **默认值** | `im_config.h` | API 地址、超时时间、线程栈大小、NVS 键名 |
| **编译期密钥** | `im_secrets.h`（不纳入版本控制） | Bot Token、App ID、代理设置 |
| **运行时覆盖** | NVS（KV 存储） | 通过 CLI 或代码调用 `im_kv_set_string()` |

### `im_secrets.h` 示例

```c
// Telegram
#define IM_SECRET_TG_TOKEN       "123456:ABC-DEF..."
#define IM_SECRET_CHANNEL_MODE   "telegram"

// Discord
#define IM_SECRET_DC_TOKEN       "MTIz..."
#define IM_SECRET_DC_CHANNEL_ID  "1234567890"

// 飞书
#define IM_SECRET_FS_APP_ID      "cli_xxxx"
#define IM_SECRET_FS_APP_SECRET  "xxxx"
#define IM_SECRET_FS_ALLOW_FROM  "ou_xxxx,ou_yyyy"

// 代理（可选）
#define IM_SECRET_PROXY_HOST     "192.168.1.100"
#define IM_SECRET_PROXY_PORT     "7890"
#define IM_SECRET_PROXY_TYPE     "http"    // "http" 或 "socks5"
```

## 集成

### CMake

在项目的 `CMakeLists.txt` 中，**在 `add_library` 之后**添加 IM 子目录：

```cmake
add_library(${EXAMPLE_LIB})
# ... 你的源文件 ...
add_subdirectory(${APP_PATH}/IM)
```

IM 的 `CMakeLists.txt` 会自动将所有源文件和头文件路径注册到 `${EXAMPLE_LIB}`。

### 代码引用

引入聚合头文件或按需引入单独模块：

```c
#include "im_api.h"
```

### 典型初始化流程

```c
// 1. 初始化子系统
message_bus_init();
http_proxy_init();
telegram_bot_init();    // 或 discord_bot_init() / feishu_bot_init()

// 2. 启动通道驱动（会创建后台线程）
telegram_bot_start();

// 3. 消费入站消息
while (1) {
    im_msg_t msg = {0};
    if (message_bus_pop_inbound(&msg, timeout_ms) == OPRT_OK) {
        // msg.channel  = "telegram" / "discord" / "feishu"
        // msg.chat_id  = 发送者 / 聊天标识
        // msg.content  = 消息文本（调用方负责释放）
        process(msg);
        free(msg.content);
    }
}
```

### 公开 API 一览

| 模块 | 函数 | 说明 |
|---|---|---|
| **message_bus** | `message_bus_init()` | 初始化入站和出站队列 |
| | `message_bus_push_inbound(msg)` | 推送消息到入站队列 |
| | `message_bus_pop_inbound(msg, ms)` | 从入站队列取出（阻塞等待 `ms` 毫秒） |
| | `message_bus_push_outbound(msg)` | 推送消息到出站队列 |
| | `message_bus_pop_outbound(msg, ms)` | 从出站队列取出（阻塞等待 `ms` 毫秒） |
| **telegram** | `telegram_bot_init()` | 从密钥文件 / NVS 加载 Token |
| | `telegram_bot_start()` | 启动长轮询线程 |
| | `telegram_send_message(chat_id, text)` | 向指定聊天发送文本 |
| | `telegram_set_token(token)` | 运行时更新 Token（持久化到 NVS） |
| **discord** | `discord_bot_init()` | 从密钥文件 / NVS 加载 Token 和频道 |
| | `discord_bot_start()` | 启动 WebSocket Gateway 线程 |
| | `discord_send_message(channel_id, text)` | 向指定频道发送文本 |
| | `discord_set_token(token)` | 运行时更新 Token |
| | `discord_set_channel_id(id)` | 运行时更新默认频道 |
| **feishu** | `feishu_bot_init()` | 从密钥文件 / NVS 加载应用凭证 |
| | `feishu_bot_start()` | 获取 Tenant Token 并启动 WebSocket 线程 |
| | `feishu_send_message(chat_id, text)` | 向用户或群组发送文本 |
| | `feishu_set_app_id(id)` | 运行时更新 App ID |
| | `feishu_set_app_secret(secret)` | 运行时更新 App Secret |
| | `feishu_set_allow_from(csv)` | 运行时更新允许的发送者列表 |
| **proxy** | `http_proxy_init()` | 从密钥文件 / NVS 加载代理设置 |
| | `http_proxy_is_enabled()` | 检查代理是否已配置 |
| | `http_proxy_set(host, port, type)` | 运行时配置代理 |
| | `http_proxy_clear()` | 清除代理配置 |
