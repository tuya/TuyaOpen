## 涂鸦云应用
[English](README.md) | 简体中文

lck:
NFO]: Saving log to: monitor.log
[09-07 20:28:49 ty D][netmgr.c:390] netmgr conn auto get 1
[09-07 20:28:52 ty D][netmgr.c:390] netmgr conn auto get 1
[09-07 20:28:52 ty D][tuya_lan.c:683] Process Data. FD:4, Num:12106, Type:13, Len:70
[09-07 20:28:52 ty D][tuya_lan.c:702] JSON string:{"data":{"dps":{"1":true}},"protocol":5,"t":1757248131}
[09-07 20:28:52 ty D][tuya_lan.c:720] Rev TP CMD. Send to User,Lan Ver 3.5
[09-07 20:28:52 ty W][tuya_iot_dp.c:182] devid is null
[09-07 20:28:52 ty D][tuya_main.c:95] Tuya Event ID:8(TUYA_EVENT_DP_RECEIVE_OBJ)
[09-07 20:28:52 ty I][tuya_main.c:96] Device Free heap 146248
[09-07 20:28:52 ty D][tuya_main.c:133] SOC Rev DP Cmd t1:0 t2:0 CNT:1
[09-07 20:28:52 ty D][tuya_main.c:135] devid.6c67d9414dc7b0e718avy5
[09-07 20:28:52 ty D][tuya_main.c:141] idx:0 dpid:1 type:0 ts:1757248132
[09-07 20:28:52 ty D][tuya_main.c:144] bool value:1
[09-07 20:28:52 ty D][tuya_iot_dp.c:239] dp report: devid 6c67d9414dc7b0e718avy5, dps 0x608124cc, dpscnt 1, flags 0
[09-07 20:28:52 ty D][dp_schema.c:526] dp<1> check. need_update:1 pv_stat:1 trig_t:0 type:0 force_send:0 prop_tp:0
[09-07 20:28:52 ty D][dp_schema.c:533] dp<1> bool: 0, new: 1
[09-07 20:28:52 ty D][dp_schema.c:940] dp rept out: {"1":true}
[09-07 20:28:52 ty D][tuya_iot_dp.c:300] lan channel report
[09-07 20:28:52 ty D][tuya_lan.c:658] Prepare To Send Lan:3.5, msg_len:51, out_len:103
[09-07 20:28:53 ty D][tuya_health.c:75] feed watchdog
[09-07 20:28:53 ty D][tuya_lan.c:683] Process Data. FD:4, Num:12107, Type:13, Len:71
[09-07 20:28:53 ty D][tuya_lan.c:702] JSON string:{"data":{"dps":{"1":false}},"protocol":5,"t":1757248133}
[09-07 20:28:53 ty D][tuya_lan.c:720] Rev TP CMD. Send to User,Lan Ver 3.5
[09-07 20:28:53 ty W][tuya_iot_dp.c:182] devid is null
[09-07 20:28:53 ty D][tuya_main.c:95] Tuya Event ID:8(TUYA_EVENT_DP_RECEIVE_OBJ)
[09-07 20:28:53 ty I][tuya_main.c:96] Device Free heap 146248
[09-07 20:28:53 ty D][tuya_main.c:133] SOC Rev DP Cmd t1:0 t2:0 CNT:1
[09-07 20:28:53 ty D][tuya_main.c:135] devid.6c67d9414dc7b0e718avy5
[09-07 20:28:53 ty D][tuya_main.c:141] idx:0 dpid:1 type:0 ts:1757248133
[09-07 20:28:53 ty D][tuya_main.c:144] bool value:0
[09-07 20:28:53 ty D][tuya_iot_dp.c:239] dp report: devid 6c67d9414dc7b0e718avy5, dps 0x608124cc, dpscnt 1, flags 0
[09-07 20:28:53 ty D][dp_schema.c:526] dp<1> check. need_update:1 pv_stat:1 trig_t:0 type:0 force_send:0 prop_tp:0
[09-07 20:28:53 ty D][dp_schema.c:533] dp<1> bool: 1, new: 0
[09-07 20:28:53 ty D][dp_schema.c:940] dp rept out: {"1":false}
[09-07 20:28:53 ty D][tuya_iot_dp.c:300] lan channel report
[09-07 20:28:53 ty D][tuya_lan.c:658] Prepare To Send Lan:3.5, msg_len:52, out_len:104
[09-07 20:28:55 ty D][netmgr.c:390] netmgr conn auto get 1
[09-07 20:29:02 ty D][mqtt_client_wrapper.c:72] MQTT_PACKET_TYPE_PUBACK id:6
[09-07 20:29:02 ty D][mqtt_service.c:338] PUBACK ID:6


涂鸦云应用是涂鸦 AI+IoT 平台提供的一种应用，通过涂鸦云应用，开发者可以快速实现设备远程控制、设备管理等功能。

`switch_demo` 演示一个简单的，跨平台、跨系统、支持多种连接的开关示例，通过涂鸦 APP、涂鸦云服务，可以对这个开关进行远程控制。

1. 创建产品并获取产品的 PID：

参考文档 [https://developer.tuya.com/cn/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc](https://developer.tuya.com/cn/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc) 在 [https://iot.tuya.com](https://iot.tuya.com) 下创建产品，并获取到创建产品的 PID 。

然后替换 [apps/tuya_cloud/switch_demo/src/tuya_config.h](./src/tuya_config.h) 文件中 `TUYA_PRODUCT_ID` 宏分别对应 pid。

2. 确认 TuyaOpen 授权码：

Tuyaopen Framework 包括：
- C 版 TuyaOpen：[https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- Arduino 版 TuyaOpen：[https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode 版 TuyaOpen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

均采用 TuyaOpen 专用授权码，使用其他授权码无法正常连接涂鸦云。

```shell
[tuya_main.c:220] Replace the TUYA_DEVICE_UUID and TUYA_DEVICE_AUTHKEY contents, otherwise the demo cannot work
[tuya_main.c:222] uuid uuidxxxxxxxxxxxxxxxx, authkey keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

可通过以下方式获取 TuyaOpen 专用授权码：

- 方式1：购买已烧录 TuyaOpen 授权码模块。该授权码已经在出厂时烧录在对应模组中，且不会丢失。TuyaOpen 在启动时通过 `tuya_iot_license_read()` 接口读取授权码。请确认当前设备是否为烧录了 TuyaOpen 授权码。

- 方式2：如当前模组未烧录 TuyaOpen 授权码，可通过 [https://platform.tuya.com/purchase/index?type=6](https://platform.tuya.com/purchase/index?type=6) 页面购买 ** TuyaOpen 授权码**，然后将 [apps/tuya_cloud/switch_demo/src/tuya_config.h](./src/tuya_config.h) 文件中 `TUYA_DEVICE_UUID` 和 `TUYA_DEVICE_AUTHKEY` 替换为购买成功后获取到的 `uuid` 和 `authkey`。

- 方式3： 如当前模组未烧录 TuyaOpen 授权码，可通过 [https://item.taobao.com/item.htm?ft=t&id=911596682625&spm=a21dvs.23580594.0.0.621e2c1bzX1OIP](https://item.taobao.com/item.htm?ft=t&id=911596682625&spm=a21dvs.23580594.0.0.621e2c1bzX1OIP) 页面购买 ** TuyaOpen 授权码**，然后将 [apps/tuya_cloud/switch_demo/src/tuya_config.h](./src/tuya_config.h) 文件中 `TUYA_DEVICE_UUID` 和 `TUYA_DEVICE_AUTHKEY` 替换为购买成功后获取到的 `uuid` 和 `authkey`。

![authorization_code](../../docs/images/zh/authorization_code.png)

```c
    tuya_iot_license_t license;

    if (OPRT_OK != tuya_iot_license_read(&license)) {
        license.uuid = TUYA_DEVICE_UUID;
        license.authkey = TUYA_DEVICE_AUTHKEY;
        PR_WARN("Replace the TUYA_DEVICE_UUID and TUYA_DEVICE_AUTHKEY contents, otherwise the demo cannot work");
    }
```

> 如 `tuya_iot_license_read()` 接口返回 OPRT_OK，则表示当前设备已经烧录了 TuyaOpen 授权码，否则表示当前模组并未烧录 TuyaOpen 授权码。

## 免费赠送 TuyaOpen 授权码活动

为了让开发者们可以自由体验 Tuyaopen Framework，现在只要在 GitHub 上给 Tuyaopen Framework 开发框架仓库，包括 [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen) 、[https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen) 和 [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen) 仓库加 star，凭 github 账号和截图，发送邮件至 `chenyisong@tuya.com` 或 加入 QQ 群 `796221529` 向群主免费领取一个 TuyaOpen Framework 专用授权码。

限量 500 个，先到先得，送完即止，赶紧扫码加群来领👇：

![qq_qrcode](../../docs/images/zh/qq_qrcode.png)