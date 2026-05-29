# http_service_portal

该示例在设备上启动 SoftAP，并通过 `http_host` 与 `http_site` 提供基础 Web 门户页面（HTML/JS/CSS）：

- `GET /`：配网页面（输入路由器 SSID/密码）
- `GET /status.html`：状态页面
- `GET /assets/style.css`：样式文件
- `GET /assets/app.js`：脚本文件
- `GET /api/status`：返回设备 Wi-Fi 模式与 STA 状态
- `POST /api/provision`：提交路由器 SSID/密码，设备切换到 STA 并尝试连接

## 默认 SoftAP 参数

- SSID: `TuyaOpen-Setup`
- Password: `12345678`
- AP IP: `192.168.50.1`
- HTTP Port: `80`

## 使用方式

1. 进入示例目录：

```bash
cd examples/protocols/http_service_portal
```

2. 选择你的板卡配置（交互）或直接编辑 `app_default.config`。

3. 编译：

```bash
tos.py build
```

4. 烧录并运行后，手机连接设备热点；多数系统会弹出配网页，或自动打开 `http://192.168.50.1/`。

## 说明

- 页面与静态资源请直接编辑 `web/` 下的 HTML/CSS/JS；`tos.py build` 会在 `src/` 下生成 `portal_web_assets.c/h`。
- 应用侧只需调用 `portal_web_attach()` 注册静态页，配网成功页用 `portal_web_provision_ok_html()` 获取。
- 这是最小可运行版本，重点是打通 SoftAP + 静态网页 + 配网 API 的主链路。
- 示例通过 SoftAP DNS 劫持与 HTTP 探测跳转，尽量触发系统 Captive Portal 弹窗；不同机型表现可能略有差异。
