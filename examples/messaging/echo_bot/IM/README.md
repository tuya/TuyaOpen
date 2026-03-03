# IM 组件

独立的消息通道组件，提供消息总线、HTTP/SOCKS5 代理、TLS 证书管理与多通道（Telegram / Discord / 飞书）收发能力。
可整目录拷贝至其他项目复用，只需提供 `tal_api.h`（TuyaOpen 平台 SDK）即可编译。

## 目录结构

```
IM/
├── im_platform.h       # 平台适配层（日志宏 IM_LOG* / 内存 im_malloc 等 / KV 存储 im_kv_*）
├── im_config.h         # 编译期默认配置（密钥 / 通道参数 / NVS 键名）
├── im_utils.h / .c     # 公共工具函数（字符串 / HTTP / JSON / 哈希）
├── bus/
│   └── message_bus.h / .c   # 入站 / 出站消息队列 (im_msg_t)
├── channels/
│   ├── telegram_bot.h / .c  # Telegram Bot (long-poll)
│   ├── discord_bot.h / .c   # Discord Bot (WebSocket Gateway)
│   └── feishu_bot.h / .c    # 飞书 Bot (WebSocket + Protobuf)
├── proxy/
│   └── http_proxy.h / .c    # HTTP CONNECT / SOCKS5 隧道
├── certs/
│   ├── tls_cert_bundle.h / .c  # 域名证书查询（iot-dns + 内置 CA 回退）
│   └── ca_bundle_mini.h / .c   # 内置 CA 证书包
└── CMakeLists.txt
```

## 配置

- `im_config.h` 定义所有编译期默认值（API 地址、超时、线程栈 等）。
- 创建 `im_secrets.h`（不纳入版本控制）可覆盖 token / 密钥等敏感默认值。
- 运行时通过 NVS (KV) 存储可动态覆盖密钥和代理设置。

## 集成

主工程 CMakeLists.txt 中添加：

```cmake
add_subdirectory(${APP_PATH}/IM)
```

确保 `add_subdirectory` 在 `add_library(${EXAMPLE_LIB})` 之后。
IM 组件的源文件和 include 目录会自动注册到 `${EXAMPLE_LIB}`。

代码中 include：

```c
#include "bus/message_bus.h"
#include "channels/telegram_bot.h"
#include "proxy/http_proxy.h"
#include "im_config.h"
```
