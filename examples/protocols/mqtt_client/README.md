# MQTT Introduction
The `example_mqtt` routine demonstrates how to use the native MQTT interface.

## Execution Results
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
The log show MQTT connect, subscribe, publish and unsuscribe.

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com