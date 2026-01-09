# HTTP 客户端 POST 示例

`example_http_client_post` 示例演示了如何使用 HTTP 客户端接口向服务器发送 POST 请求，并在日志和屏幕上显示响应内容。

## 功能特性

- **HTTP POST 请求**: 向服务器发送带 JSON 体的 POST 请求
- **响应显示**: 在日志和屏幕上显示服务器响应
- **JSON 解析**: 从 JSON 格式中提取响应文本
- **网络集成**: 网络连接后自动发送请求

## 前置要求

1. **Python 服务器**: 需要在电脑上运行 Python HTTP 服务器
2. **网络连接**: 设备必须与服务器连接到同一网络
3. **配置**: 在源代码中更新服务器 IP 和端口

## 快速开始

### 步骤 1: 启动 Python 服务器

在您的电脑上运行提供的 Python 服务器：

```bash
cd examples/protocols/http_client_post
python3 server.py
```

服务器将显示：
```
============================================================
HTTP POST Server for TuyaOpen HTTP Client Example
============================================================
Server starting on 0.0.0.0:8080
Local IP address: 192.168.1.100
Access URL: http://192.168.1.100:8080
API Endpoint: http://192.168.1.100:8080/api/random
============================================================
```

**请记下 IP 地址** - 下一步需要用到。

### 步骤 2: 配置客户端

编辑 `src/example_http_client_post.c` 并更新以下常量：

```c
#define SERVER_HOST "192.168.1.100"  // 改为您电脑的 IP
#define SERVER_PORT 8080             // 改为服务器端口（默认: 8080）
#define SERVER_PATH "/api/random"    // 服务器端点路径
```

### 步骤 3: 编译和烧录

```bash
tos config_choice  # 选择您的开发板
tos build          # 编译项目
# 烧录到设备
```

### 步骤 4: 连接设备到网络

确保设备连接到与电脑相同的 Wi-Fi 网络。

### 步骤 5: 查看结果

当设备连接到网络后，它将自动：
1. 向服务器发送 POST 请求
2. 接收随机字符串响应
3. 在日志和屏幕上显示响应

## 服务器使用方法

### 基本用法

```bash
python3 server.py
```

### 自定义主机和端口

```bash
python3 server.py --host 0.0.0.0 --port 9000
```

### 获取帮助

```bash
python3 server.py --help
```

## 服务器 API

### POST /api/random

**请求:**
```http
POST /api/random HTTP/1.1
Host: 192.168.1.100:8080
Content-Type: application/json
Content-Length: 32

{"action":"get_random_string"}
```

**响应:**
```json
{
  "status": "success",
  "message": "aB3xK9mP2qR7nT4v",
  "text": "aB3xK9mP2qR7nT4v",
  "data": "aB3xK9mP2qR7nT4v",
  "length": 16
}
```

### GET /

**响应:**
```json
{
  "status": "running",
  "message": "HTTP POST Server is running",
  "endpoint": "/api/random",
  "method": "POST",
  "description": "Send POST request to /api/random to get a random string"
}
```

## 执行结果

当设备连接并发送 POST 请求时，您将看到：

**设备日志中:**
```
========================================
Server Response:
aB3xK9mP2qR7nT4v
========================================

╔════════════════════════════════════╗
║     Server Response Display       ║
╠════════════════════════════════════╣
║ aB3xK9mP2qR7nT4v                  ║
╚════════════════════════════════════╝
```

**服务器控制台:**
```
[SERVER] 192.168.1.50 - - [01/Jan/2025 12:00:00] "POST /api/random HTTP/1.1" 200 -
Request body: {"action":"get_random_string"}
Response: {
  "status": "success",
  "message": "aB3xK9mP2qR7nT4v",
  ...
}
```

## 配置说明

### 客户端配置

编辑 `src/example_http_client_post.c`:

```c
// 服务器配置
#define SERVER_HOST "192.168.1.100"  // 您电脑的 IP
#define SERVER_PORT 8080             // 服务器端口
#define SERVER_PATH "/api/random"    // API 端点

// Wi-Fi 配置（如果使用 Wi-Fi）
#define DEFAULT_WIFI_SSID "your-ssid"
#define DEFAULT_WIFI_PSWD "your-password"
```

### 服务器配置

编辑 `server.py` 或使用命令行参数：

```bash
python3 server.py --host 0.0.0.0 --port 8080
```

## 故障排除

### 找不到服务器

1. **检查 IP 地址**: 确保 `SERVER_HOST` 与您电脑的 IP 匹配
2. **检查网络**: 确保设备和电脑在同一网络
3. **检查防火墙**: 禁用防火墙或允许端口 8080

### 连接超时

1. **检查服务器**: 确保 Python 服务器正在运行
2. **检查端口**: 验证端口号是否匹配配置
3. **检查网络**: 测试网络连接

### 响应未显示

1. **检查日志**: 查看设备日志中的错误消息
2. **检查响应格式**: 服务器应返回带 "message" 字段的 JSON
3. **检查缓冲区大小**: 如果响应被截断，增加 `server_response` 缓冲区

## 高级用法

### 自定义服务器实现

您可以创建自己的服务器。服务器应该：
1. 在配置的路径上接受 POST 请求
2. 返回包含以下字段之一的 JSON 响应：
   - `message`
   - `text`
   - `data`
   - `response`
   - `result`

示例响应格式：
```json
{
  "message": "您的随机字符串"
}
```

### 显示集成

要启用屏幕显示，请在 `__display_on_screen()` 中取消注释并配置显示代码：

```c
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#include "app_display.h"
app_display_send_msg(TY_DISPLAY_TP_SYSTEM_MSG, (uint8_t *)text, len);
#endif
```

## 技术支持

您可以通过以下方式获得涂鸦技术支持：

- TuyaOS 论坛: https://www.tuyaos.com
- 开发者中心: https://developer.tuya.com
- 帮助中心: https://support.tuya.com/help
- 技术支持工单中心: https://service.console.tuya.com

