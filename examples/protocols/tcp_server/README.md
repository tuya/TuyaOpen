# NETWORK SERVER

## Introduction

This example creates a TCP server. After a TCP client successfully connects, the server will print the received data through the log serial port and then send the received data back to the client. The server will close its socket and thread after receiving "stop". Note that this should be called after the module is connected to WiFi. The TCP client can use network debugging assistants like `NetAssist`. It is important that the module's network and the network debugging assistant are on the same LAN.

This example can be used in conjunction with tcp_client by changing the tcp_client's port to 1234, to facilitate interaction between the client and server.

## Execution Result
```c
[11-22 17:03:48 ty D][tal_thread.c:203] Thread:eg_tcp_server Exec Start. Set to Running Stat
[11-22 17:03:48 ty N][example_tcp_server.c:55] --- start the tcp server at port 1234
[11-22 17:03:48 ty I][tal_thread.c:184] thread_create name:eg_tcp_server,stackDepth:3072,totalstackDepth:51712,priority:3
[11-22 17:03:52 ty D][example_tcp_server.c:72] accept client ip:127.0.0.1, port:58028
[11-22 17:03:57 ty D][example_tcp_server.c:81] 127.0.0.1:58028==> 1234
[11-22 17:04:07 ty D][example_tcp_server.c:81] 127.0.0.1:58028==> abc
[11-22 17:04:13 ty D][example_tcp_server.c:81] 127.0.0.1:58028==> stop
[11-22 17:04:13 ty D][tal_thread.c:244] Del Thrd:eg_tcp_server
[11-22 17:04:13 ty D][tal_thread.c:219] Thread:eg_tcp_server Exec Finish. Set to Del Stat
[11-22 17:04:13 ty D][tal_thread.c:95] del list not empty...deleting 0x42fc00, next 0x42fd88
[11-22 17:04:13 ty D][tal_thread.c:103] Final Free Thread:eg_tcp_server, is_self:1
[11-22 17:04:13 ty D][tal_thread.c:107] delay to delete thread self
[11-22 17:04:13 ty D][tal_thread.c:118] finally delete thread self
[11-22 17:04:13 ty D][tal_thread.c:73] real delete thread:0x0x4192d4
```
![TCP_SERVER](https://images.tuyacn.com/fe-static/docs/img/f38a7898-f120-4ae4-ac47-e8168018d536.png)

## Technical Support

You can obtain support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com