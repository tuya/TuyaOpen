# HTTP  简介
 example_http 例程演示了如何使用原生http接口。

## 运行结果
```c
[01-02 04:09:16 ty D][examples_mqtt_client.c:122] start mqtt client to broker.emqx.io
[01-02 04:09:16 ty N][netconn_wired.c:43] wired status changed to 1, old stat: 1
[01-02 04:09:16 ty D][netmgr.c:113] netmgr status changed to 1, old 1, active 2
[01-02 04:09:17 ty D][tcp_transporter.c:102] bind ip:00000000 port:0 ok
[01-02 04:09:17 ty I][examples_mqtt_client.c:66] mqtt client connected! try to subscribe tuya/tos-test
[01-02 04:09:17 ty D][examples_mqtt_client.c:71] Subscribe topic tuya/tos-test ID:1
[01-02 04:09:18 ty D][mqtt_client_wrapper.c:58] MQTT_PACKET_TYPE_SUBACK id:1
[01-02 04:09:18 ty D][examples_mqtt_client.c:89] Subscribe successed ID:1
[01-02 04:09:18 ty D][examples_mqtt_client.c:95] Publish msg ID:2
[01-02 04:09:18 ty D][mqtt_client_wrapper.c:72] MQTT_PACKET_TYPE_PUBACK id:2
[01-02 04:09:18 ty D][examples_mqtt_client.c:100] PUBACK successed ID:2
[01-02 04:09:18 ty D][examples_mqtt_client.c:101] UnSubscribe topic tuya/tos-test
[01-02 04:09:18 ty D][examples_mqtt_client.c:84] recv message TopicName:tuya/tos-test, payload len:32
```


## 技术支持

您可以通过以下方法获得涂鸦的支持:

- TuyaOS 论坛： https://www.tuyaos.com

- 开发者中心： https://developer.tuya.com

- 帮助中心： https://support.tuya.com/help

- 技术支持工单中心： https://service.console.tuya.com
