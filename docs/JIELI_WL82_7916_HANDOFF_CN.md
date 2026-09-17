# 杰理 wl82（AC7916A/7916x）接入 TuyaOpen 交接文档

## 1. 适配范围

本分支完成了杰理 AC79/wl82 平台接入 TuyaOpen 的第一版工程链路，目标板为 AC7916A。
TuyaOpen 中的命名约定如下：

| 项目 | 名称 |
| --- | --- |
| TuyaOpen 平台 | `JIELI` |
| 芯片/SoC | `wl82` |
| 开发板 | `AC7916A` |
| SDK | `AC79_AIoT_SDK` |
| 主要适配接口 | `tal_xxx`、`tkl_xxx` |

当前已覆盖：

- Linux 主机编译 `apps/tuya_cloud/switch_demo`；
- 杰理 wl82 SDK 的最终链接和原始应用镜像生成；
- `tal_system`、`tal_uart` 基础适配；
- `tkl_system`、`tkl_uart`、线程、互斥锁、信号量、队列、输出、Flash、OTA 等基础适配；
- `tkl_wifi` 和 `tkl_bluetooth` 到杰理原生 Wi-Fi/BLE API 的适配代码；
- AC7916A 板级配置和 `tos.py build` 入口；
- Windows 下使用杰理 `isd_download.exe` 的手工烧录链路。

目前没有连接实物板验证，因此 Wi-Fi、BLE、UART 日志和 `switch_demo` 的运行结果仍需硬件验证。

## 2. 代码位置

```text
platform/JIELI/
├── Kconfig
├── default.config
├── sdk_config.yaml
├── build_setup.py
├── build_example.py
├── jieli_build.py
├── platform_flash_bridge.py
└── tuyaos/
    └── tuyaos_adapter/
        ├── include/
        └── src/
            ├── tal_system.c
            ├── tal_uart.c
            ├── tkl_bluetooth.c
            ├── tkl_wifi.c
            └── 其他 tal/tkl 适配文件

boards/JIELI/AC7916A/
├── CMakeLists.txt
└── Kconfig
```

杰理 SDK 不要提交到 TuyaOpen 仓库，建议放在工作区旁边：

```text
/home/share/samba/tyopen/jieli/AC79_AIoT_SDK
```

## 3. Linux 编译环境

需要准备：

```text
Jieli SDK:
/home/share/samba/tyopen/jieli/AC79_AIoT_SDK

Jieli pi32v2 工具链:
/home/share/samba/tyopen/ipc_ac7916a/toolchain/jieli-linux-toolchains/pi32v2/bin
```

工具链目录至少应包含：

```text
clang
objcopy
objdump
objsizedump
lto-wrapper
lto-ar
```

## 4. Linux 编译 switch_demo

```bash
cd /home/share/samba/tyopen/jieli/TuyaOpen-7916-uart/apps/tuya_cloud/switch_demo

JIELI_SDK_ROOT=/home/share/samba/tyopen/jieli/AC79_AIoT_SDK \
JIELI_TOOL_DIR=/home/share/samba/tyopen/ipc_ac7916a/toolchain/jieli-linux-toolchains/pi32v2/bin \
JIELI_BUILD_JOBS=4 \
python /home/share/samba/tyopen/jieli/TuyaOpen-7916-uart/tos.py build -v
```

成功标志：

```text
BUILD SUCCESS
[JIELI] raw artifact: .../.build/jieli-staging/build/cpu/wl82/tools/app.bin
[JIELI] artifact: .../dist/switch_demo_1.0.0/switch_demo_QIO_1.0.0.bin
```

最终文件：

```text
apps/tuya_cloud/switch_demo/dist/switch_demo_1.0.0/switch_demo_QIO_1.0.0.bin
```

注意：当前没有 Linux 版杰理 `host-client`/下载器，因此构建后处理使用 `objcopy` 从 `sdk.elf`
拼出杰理原始应用 `app.bin`。输出文件虽然沿用 TuyaOpen 的 `*_QIO_*.bin` 命名，但它不是
ESP/Tuya 芯片格式的完整 QIO/UFW 文件。

## 5. Windows 烧录流程

### 5.1 准备文件

在 Windows 创建目录，例如：

```text
C:\jieli\wl82_flash\
```

放入：

```text
app.bin
isd_download.exe
isd_config.ini
uboot.boot
cfg_tool.bin
```

文件来源：

| Windows 文件 | 来源 |
| --- | --- |
| `app.bin` | 将 Linux 生成的 `switch_demo_QIO_1.0.0.bin` 复制并重命名为 `app.bin` |
| `isd_download.exe` | `AC79_AIoT_SDK/cpu/wl82/tools/isd_download.exe` |
| `isd_config.ini` | 本次构建的 `.build/jieli-staging/build/cpu/wl82/tools/isd_config.ini` |
| `uboot.boot` | `AC79_AIoT_SDK/cpu/wl82/tools/uboot.boot` |
| `cfg_tool.bin` | `AC79_AIoT_SDK/cpu/wl82/tools/cfg_tool.bin` |

`isd_config.ini` 应尽量使用与本次构建对应的版本，以避免 Flash 分区、VM 区域和预留区配置
不一致。

### 5.2 执行下载

在 `C:\jieli\wl82_flash\` 中打开 CMD，执行：

```bat
cd /d C:\jieli\wl82_flash

isd_download.exe isd_config.ini ^
-gen2 -tonorflash -dev wl82 ^
-boot 0x1c02000 ^
-div1 -wait 300 ^
-uboot uboot.boot ^
-app app.bin cfg_tool.bin ^
-reboot 500 ^
-update_files normal ^
-extend-bin
```

开发板通常需要连接下载 USB/UART，并在上电或复位时按住 Boot/下载键进入下载模式。具体按键
和时序以 AC7916A 开发板说明为准。烧录前要关闭串口助手，避免端口被占用。

### 5.3 查看串口

烧录完成后，使用串口工具打开开发板日志串口，默认配置为：

```text
115200 baud, 8 data bits, no parity, 1 stop bit, no flow control
```

当前工程默认日志串口为 `LOG_UART_PORT=0`，映射到 Jieli 的 `uart2`，日志 TX 为
`IO_PORTB_06`（PB6）。USB 串口号需要根据设备管理器实际枚举结果确认。

## 6. 当前已验证内容

已在 Linux 环境验证：

```bash
python -m unittest discover -s tests/platform -p 'test_jieli_*.py'
```

结果：17 个平台测试通过。

另外已验证：

- `git diff --check` 通过；
- `apps/tuya_cloud/switch_demo` 的 `tos.py build -v` 返回 0；
- TuyaOpen 库和杰理 wl82 原生 Wi-Fi/BLE 相关库完成最终链接；
- 生成 `switch_demo_QIO_1.0.0.bin`，当前大小约 585 KB。

尚未验证：

- AC7916A 实物烧录；
- UART 实物启动日志；
- Wi-Fi 扫描、连接、DHCP 和云端通信；
- BLE 广播、连接和 GATT 数据交互；
- `switch_demo` 的按键、DP、云端和网络完整运行链路。

## 7. 继续开发时的重点

1. 连接 AC7916A，先确认 `app.bin` 能被 `isd_download.exe` 烧录并启动。
2. 先验证 UART 日志和 `tal_system` 基础线程/同步原语。
3. 验证 `tkl_wifi` 的初始化、扫描、连接、断开、DHCP 事件映射。
4. 验证 `tkl_bluetooth` 的 BLE 广播、连接和 GATT profile；当前杰理 ATT 数据库为静态链接，运行时动态添加 Service 仍需补充 profile 生成方案。
5. 将 Windows 下载命令封装成平台专用 `tos.py flash` bridge，避免手工复制文件。
6. 如果要支持 Linux 烧录，需要取得或实现杰理 wl82 的 Linux 下载器，不能直接使用通用 `tyutool`。
7. AC792x 不能直接复用本适配；应基于 AC792 SDK、芯片库和内存/启动配置重新建立平台层。

## 8. 常见问题

### 编译提示找不到工具链

检查两个环境变量：

```bash
echo "$JIELI_SDK_ROOT"
echo "$JIELI_TOOL_DIR"
```

并确认 `JIELI_TOOL_DIR` 下存在 `clang`、`objcopy`、`objdump` 和 `objsizedump`。

### 直接把 `*_QIO_*.bin` 交给通用 TuyaOpen 烧录工具失败

这是预期现象。当前文件是杰理原始 `app.bin`，必须使用杰理 `isd_download.exe`，并配合
`isd_config.ini`、`uboot.boot` 和 `cfg_tool.bin`。

### 烧录后仍运行旧程序

先确认使用了本次构建对应的 `app.bin` 和 `isd_config.ini`，再检查开发板是否真正进入下载模式。
不要直接执行 `-format all`，该参数会擦除更多 Flash 区域，可能清除配置和授权数据。
