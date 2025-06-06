# NETWORK CLIENT 简介

该示例为 TCP 客户端的一个示例演示，TCP 客户端在连接上服务器后会发送 5 次 `Hello Tuya`，然后关闭 TCP 客户端 socket 套接字和线程。注意需要在模组连上 WiFi 之后调用，TCP 客户端可以使用 `NetAssist` 等网络调试助手。需要注意的是模组连接的网络要和网络调试助手在同一个局域网下。

该示例演示连接的是当前主机的echo服务，需要手动开启，你也可以根据自己的网络环境手动修改IP和port。
```
sudo nc -lk 7
```

## 运行结果

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

TCP 服务器显示如下：

![TCP_client](https://images.tuyacn.com/fe-static/docs/img/d0f49c56-bced-4efa-b61d-8a828eb36273.png)

## 技术支持

您可以通过以下方法获得涂鸦的支持:

* [开发者中心](https://developer.tuya.com)
* [帮助中心](https://support.tuya.com/help)
* [技术支持帮助中心](https://service.console.tuya.com)
* [Tuya os](https://developer.tuya.com/cn/tuyaos)
