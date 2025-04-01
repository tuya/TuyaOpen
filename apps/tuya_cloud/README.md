## Tuya Cloud Applications Project

English | [简体中文](README_zh.md)

The Tuya Cloud Application is an application provided by the Tuya AI+IoT platform, which allows developers to quickly implement features such as remote control and device management.

`switch_demo` demonstrates a simple, cross-platform, cross-system switch example that supports multiple connections. Through the Tuya APP and Tuya Cloud Service, this switch can be remotely controlled.

1. Create a product and obtain the product PID:

Refer to the documentation [https://developer.tuya.com/en/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc](https://developer.tuya.com/cn/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc) to create a product on [https://iot.tuya.com](https://iot.tuya.com) and obtain the PID of the created product. Replace the `TUYA_PRODUCT_KEY` macro in the [apps/tuya_cloud/switch_demo/src/tuya_config.h](./src/tuya_config.h) file with the PID.

2. Confirm the TuyaOpen authorization code:

The Tuyaopen Framework includes:
- C Version of TuyaOpen: [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- Arduino Version of TuyaOpen: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode Version of TuyaOpen: [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

All versions use TuyaOpen dedicated authorization codes. Using other authorization codes will not allow normal connection to the Tuya Cloud.

```shell
[tuya_main.c:220] Replace the TUYA_DEVICE_UUID and TUYA_DEVICE_AUTHKEY contents, otherwise the demo cannot work
[tuya_main.c:222] uuid uuidxxxxxxxxxxxxxxxx, authkey keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

The dedicated authorization code for TuyaOpen can be obtained through the following methods:

- Method 1: Purchase a module with a TuyaOpen authorization code pre-burned. The authorization code is already burned in the corresponding module at the factory and will not be lost. TuyaOpen reads the authorization code through the tuya_iot_license_read() interface at startup. Please confirm whether the current device has a TuyaOpen authorization code pre-burned.

- Method 2: If the current module is not pre-burned with a TuyaOpen authorization code, you can purchase an Open SDK Authorization Code through the https://platform.tuya.com/purchase/index?type=6 page and put 'TUYA_DEVICE_UUID' and 'TUYA_DEVICE_AUTHKEY' in the [apps/tuya_cloud/switch_demo/src/tuya_config.h](./src/tuya_config.h) file Replace the uuid and authkey obtained after successful purchase

![authorization_code](../../docs/images/en/authorization_code.png)

```c
    tuya_iot_license_t license;

    if (OPRT_OK != tuya_iot_license_read(&license)) {
        license.uuid = TUYA_DEVICE_UUID;
        license.authkey = TUYA_DEVICE_AUTHKEY;
        PR_WARN("Replace the TUYA_DEVICE_UUID and TUYA_DEVICE_AUTHKEY contents, otherwise the demo cannot work");
    }
```

> If the `tuya_iot_license_read()` interface returns OPRT_OK, it indicates that the current device has a TuyaOpen authorization code pre-burned. Otherwise, it indicates that the current module is not pre-burned with a TuyaOpen authorization code.

## Free TuyaOpen Authorization Code Giveaway

To allow developers to freely experience the Tuyaopen Framework, we are now offering a free TuyaOpen Framework authorization code to anyone who stars the Tuyaopen Framework repositories on GitHub, including [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen), [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen), and [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen). Simply star these repositories, provide a screenshot along with your GitHub account, send an email to `chenyisong@tuya.com` to obtain a free TuyaOpen Framework dedicated authorization code.

Limited to 500 units, first come, first served, while supplies last.
