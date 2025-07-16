# camera_demo

## 简介
一个简单的，跨平台、跨系统、支持多种连接的开关示例。通过涂鸦 APP、涂鸦云服务，可以对这个Camera进行远程拉视频流观看（外出）、局域网拉视频流观看（同一局域网）。
![](https://images.tuyacn.com/fe-static/docs/img/74af5009-43f6-439e-8089-fca2b0edbe96.jpg)



## 目录结构
```sh
+- camera_demo
    +- libqrencode
    +- src
        -- cli_cmd.c
        -- qrencode_print.c
        -- tuya_main.c
        -- tuya_config.h
    -- CMakeLists.txt
    -- README_CN.md
    -- README.md
```
* libqrencode: 一个二维码工具库，用于有线网络连接涂鸦云服务时生成二维码，并使用涂鸦 APP 扫描二维码进行绑定
* qrencode_print.c: 用于在屏幕、串口工具上展示二维码
* cli_cmd.c: camera_demo 的一些命令行操作，用于查看、操作 camera_demo 的信息和状态
* tuya_main.c: camera_demo 的主要功能
* tuya_config.h: 涂鸦PID和授权信息，在涂鸦IoT平台上创建并获取，可以参考文档 [TuyaOS quickstart](https://developer.tuya.com/en/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc)

## 支持硬件
当前工程可在所有当前已支持的芯片和开发板上运行


