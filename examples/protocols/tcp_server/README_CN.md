# NETWORK SERVER

## 简介

该示例会创建一个 TCP 服务器，在 TCP 客户端连接成功后，服务器会将接收到的数据通过日志串口进行打印，然后再将接收到的数据发送到客户端。当服务器接收到 "stop" 后会关闭掉服务器的套接字和线程。注意需要在模组连上 WiFi 之后调用，TCP 客户端可以使用 `NetAssist` 等网络调试助手。需要注意的是模组连接的网络要和网络调试助手在同一个局域网下。

该示例可以和tcp_client配合，修改tcp_client的port为1234，集合进行clint和server的交互。

## 运行结果

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

## 技术支持

您可以通过以下方法获得涂鸦的支持:

* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos) 
