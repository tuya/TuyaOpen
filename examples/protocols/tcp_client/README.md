# NETWORK CLIENT Introduction

This example demonstrates a TCP client. After connecting to the server, the TCP client will send `Hello Tuya` five times, then close the TCP client socket and thread. Note that it should be called after the module is connected to WiFi. The TCP client can use network debugging assistants like `NetAssist`. It is important that the module's network connection and the network debugging assistant are on the same LAN.

This example demonstrates connecting to the echo service of the current host, which needs to be manually activated. You can also manually modify the IP and port according to your network environment.
```
sudo nc -lk 7
```

## Execution Results
```c
[11-22 16:43:39 ty D][tal_thread.c:203] Thread:eg_tcp_client Exec Start. Set to Running Stat
[11-22 16:43:39 ty N][example_tcp_client.c:63] --- start connect the tcp server 127.0.0.1 port 7
[11-22 16:43:39 ty I][tal_thread.c:184] thread_create name:eg_tcp_client,stackDepth:3072,totalstackDepth:42496,priority:3
[11-22 16:43:40 ty D][example_tcp_client.c:73] connect tcp server ip: 127.0.0.1, port: 7
[11-22 16:43:48 ty D][tal_thread.c:244] Del Thrd:eg_tcp_client
[11-22 16:43:48 ty D][tal_thread.c:219] Thread:eg_tcp_client Exec Finish. Set to Del Stat
[11-22 16:43:48 ty D][tal_thread.c:95] del list not empty...deleting 0x42fc00, next 0x42fd88
[11-22 16:43:48 ty D][tal_thread.c:103] Final Free Thread:eg_tcp_client, is_self:1
[11-22 16:43:48 ty D][tal_thread.c:107] delay to delete thread self
[11-22 16:43:48 ty D][tal_thread.c:118] finally delete thread self
[11-22 16:43:48 ty D][tal_thread.c:73] real delete thread:0x0x4192d4
```

The TCP server displays as follows:

![TCP_client](https://images.tuyacn.com/fe-static/docs/img/d0f49c56-bced-4efa-b61d-8a828eb36273.png)

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com