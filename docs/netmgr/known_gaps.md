# netmgr 之下的已知缺口

> 状态：随 netmgr M0–M4 重构落地，对应 `yj/feat-4G` 分支；全文校对基线 commit `27072238`
> **§1–§3 已修复并合入**（`a7259232..efb118a4`，9 个 commit）。诊断与影响面分析保留，供理解修法用；各节标题已改标【已修复】。§4 的前置因此解除，见 §0。
> 目标读者：要**修**这些东西的人 —— 维护 TAL / TKL 适配层、LAN 服务、netcfg、`tuya_iot` 的人
> 与另两篇的分工：
> - [`extension_guide.md`](extension_guide.md) 讲怎么往 netmgr **上面加东西**（加链路技术、加 socket 后端、换排序、换探测）；
> - [`release_notes.md`](release_notes.md) 讲升级之后**调用方能观察到什么变化**；
> - 本文讲重构过程中**撞到、确认、但故意没修**的东西。它们几乎全部在 netmgr 之下（TAL/TKL、LAN、netcfg、`tuya_iot`、`tal_cli`），修它们要动 netmgr 以外的模块，不属于这次重构的范围。今天这些结论只存在于源码注释和 commit message 里，那正是它们会被丢掉的地方。

**引用约定**：本文所有 `文件:行` 都在 commit `27072238` 上逐条核过，核的是**参与编译的那个文件**，不是同名的模板或另一个平台的副本。如果行号将来漂了，以符号名为准。

**一条必须先分清的区分**：每节标题后都标了类别，只有两种 ——

| 标记 | 含义 |
| --- | --- |
| **【已绕开】** | netmgr 里有对策，对策本身在代码里有注释记录，现场不会因为它出问题。修它是为了拿回能力或释放约束，不是为了灭火。 |
| **【活的 bug】** | 没有绕开的余地。今天就在出问题，或者一定会在某个可描述的场景里出问题。 |

有一节是两者的混合（§6），标题里写清了哪一半是哪一类。

---

## 0. 建议的修复顺序，以及为什么是这个顺序

| # | 缺口 | 类别 | 改动量 | 不修的代价 |
| --- | --- | --- | --- | --- |
| 1 | porting 模板的 `tkl_wired_set_status_cb()` 无保护 | **已修复** `a7259232` | ~6 行 | 随新 port 数量线性增长 |
| 2 | `tuya_lan_init()` 三条失败路径返回 `OPRT_OK` | **已修复** `d8e930bf`+`c7ae26a9` | 3 处赋码 + 1 条日志 + 1 个判空 | LAN 静默且永久地死掉 |
| 3 | LAN 与 AI monitor 共用 socket loop，无引用计数 | **已修复** `f2323e11..efb118a4` | 中（实际修法是每 owner 一个 loop，不是引用计数） | 崩溃；且是 §4 和 netmgr LAN 门控的根因 |
| 4 | AP 配网关掉 LAN，没人开回来 | 活的 bug | 小；**前置 §3 已修，现已解锁** | 每次重新配网后 LAN 功能消失到重启 |
| 5 | `ENABLE_BLUETOOTH=y` + `ENABLE_WIFI=n` 编译不过 | 活的 bug | CMake + 1 个 include 位置 | 纯 BLE 产品做不出来 |
| 6 | `tkl_net_getsockname()` 各平台语义不一致 | 一半已绕开 / 一半是活的 | 每平台 ~15 行 | ESP32 上源地址绑定静默失效；T3 上"已绑定则不动"这条保护失效 |
| 7 | 没有任何 TAL/TKL 入口能撤回已安装的 wired 回调 | 已绕开 | 大（TKL 接口变更 × 全平台） | netmgr 必须永久保留互斥锁；`close()` 永远是空的 |
| 8 | `tal_cellular` 没有 deinit，也没有 connect/disconnect | 已绕开 | 中（TAL+TKL 接口，只有一个在树内的移植） | 蜂窝链路无法降下、无法退避、无法省电 |
| 9 | `tuya_mqtt_stop()` 的赋值顺序 | 已绕开 | 2 行 | 订阅者永远分不清主动停止和 keepalive 死亡 |
| 10 | `tuya_iot_destroy()` 硬编码三种链路类型 | 潜在 | 小 | 板级新增第四种链路时才发作 |
| 11 | `tal_cli` 的 `argv` 在 `argc` 之后不清零 | 潜在 | 2 行 | "读了一个不存在的参数"这一类错误读到似是而非的字符串而不是崩掉 |
| 12 | `NETCONN_CAP_METERED` 声明了但内置排序不看它 | 模块内缺口 | 中（分两半，可独立做） | "蜂窝是兜底"这个意图靠优先级数字表达，板级改优先级就静默丢失 |

排序依据，按权重从高到低：

1. **修起来最便宜、但不修的代价随时间增长的先修。** §1 是唯一一条"缺陷会被复制"的：它在 `tools/porting/template/` 里，是新 port 的起点。今天没有任何构建链接它，所以改它零风险；每多一个从它派生出来的移植，修它的成本就多一份。这一条排第一不是因为它今天最严重，而是因为它今天最便宜而明天不是。
2. **静默 + 永久 + 改动量小的排前面。** §2 是本文里性价比最高的一条之一：三处失败路径各自赋一个错误码、补一条日志、给清理路径加一个判空就能解决（3 处赋码 + 1 条日志 + 1 个判空），而它今天的效果是 LAN 在整个进程生命周期里彻底消失而上层收到"成功"。
3. **别人绕不开的根因排在它的后果前面。** §3 是 §4 的前置，也曾是 netmgr 的 LAN 门控只敢"启动"不敢"停止"的原因。§3 修好之后，`netmgr.c` 的 LAN 门控注释里由 §3 撑着的两条理由（共用 loop 会崩、因此"停止"今天是坏的）已经撤下，剩下三条与 §3 无关的理由仍然成立 —— 门控依旧只管启动。**§4 的干净修法现在可以做了。**
4. **挡住一整类产品的排在只损失单个能力的前面。** §5 让纯 BLE 设备根本做不出来，这比 §6/§7/§8 各自损失的能力更贵。
5. **需要跨平台新增 TKL 接口的排最后。** §7、§8 的改动面是本文里最大的（TKL 头文件 + 每个适配 + 移植模板 + 树外移植），而 netmgr 已经有对策活下来了。它们值得做，但不该排在灭火前面。
6. **同类里，"已绕开"永远排在"活的 bug"后面。** §9、§10、§11 都很便宜，但都已经被绕开或还没有受害者，所以放在末尾。

---

## 1. porting 模板的 `tkl_wired_set_status_cb()` 每次调用都起一个线程，且会直接调用 NULL 回调 —— 【已修复 `a7259232`】

### 涉及文件

- `tools/porting/template/linux/tkl_wired.c:278-287` —— 函数本体
- `tools/porting/template/linux/tkl_wired.c:149` —— `TKL_WIRED_STATUS_CHANGE_CB status_cb;`（文件作用域，没有 `static`，没有初始值）
- `tools/porting/template/linux/tkl_wired.c:157-176` —— `link_status_thread()`，`while (1)` 永不退出
- `tools/porting/template/linux/tkl_wired.c:173` —— `status_cb(status);`，无判空
- 对照物（**正确**的那个）：`platform/LINUX/tuyaos_adapter/src/tkl_wired.c:129`、`:157`、`:164`

### 问题是什么

模板里的实现是这样：

```c
OPERATE_RET tkl_wired_set_status_cb(TKL_WIRED_STATUS_CHANGE_CB cb)
{
    // --- BEGIN: user implements ---
    pthread_t thread;

    status_cb = cb;

    return pthread_create(&thread, NULL, link_status_thread, NULL);
    // --- END: user implements ---
}
```

赋值和 `pthread_create()` 都没有任何保护，于是有三个独立的缺陷：

1. **每次调用多一个轮询线程。** 调 N 次就有 N 个线程，每个都在 `while (1)` 里每秒读一次链路状态，并且每个都会调用 `status_cb`。一次 `netmgr_deinit()` + `netmgr_init()` 就多一个。
2. **`NULL` 参数会导致调用空函数指针。** `status_cb = NULL` 之后照样起线程，`:173` 的 `status_cb(status)` 没有判空，链路状态一变就是一次 NULL 函数指针调用。也就是说，试图用 `tkl_wired_set_status_cb(NULL)` 来"注销回调"这个非常自然的动作，在这里不是无效，而是主动崩溃。
3. **`pthread_t` 是局部变量（`:281`）**，句柄出了函数就没了，线程既没有 detach 也永远无法 join，资源不会回收。

而**真正发货的那个适配是对的**，两者的区别必须说清楚，否则很容易把结论搬错地方：`platform/LINUX/tuyaos_adapter/src/tkl_wired.c:129` 把整个函数体放在 `if (cb)` 里面，`:157` 用 `if (!wired_event_thread)` 保护创建，`:164` 的 `event_cb(status)` 也在 `if (cb)` 块内。所以在 LINUX 平台上，**不管 netmgr 被初始化多少次，轮询线程只有一个，活到进程结束**；而 `NULL` 参数会被整个忽略，连已存的指针都不会清掉。

### 怎么观察到

今天观察不到：树里没有任何构建会编译 `tools/porting/template/`。它会在两个时刻第一次被观察到 ——

- 有人用 `tools/porting` 起了一个新移植，然后在同一个进程里第二次调 `netmgr_init()`（例如串口 CLI 的 `netmgr deinit` / `netmgr init`，见 §7）；
- 或者有人照着"注销回调"的直觉调了 `tal_wired_set_status_cb(NULL)`。

### netmgr 今天怎么处理

不处理，只记录并明确禁止。`netmgr_priv.h:104-116` 在 `netmgr_deinit()` 的设计记录里写明：**不要**用 `NULL` 去清 wired 状态回调，并且点名了这个模板文件作为理由之一。所以 `netconn_wired_close()`（`src/tuya_cloud_service/netmgr/src/conn/netconn_wired.c:120-123`）是一个有注释的空实现。

### 建议的修法与影响面

照发货适配改：函数体整体放进 `if (cb)`，`pthread_t` 提到文件作用域并用它保护创建，`:173` 加判空，`:149` 补 `static`。约 6 行。

影响面：**对现有构建为零** —— 没有任何 CMake 目标链接这个文件。它改变的是每个未来移植继承到的起点。这也是它排第一的全部理由。

---

## 2. `tuya_lan_init()` 在三条失败路径上返回 `OPRT_OK` —— 【已修复 `d8e930bf`+`c7ae26a9`】

### 涉及文件

- `src/tuya_cloud_service/lan/tuya_lan.c:1352` —— `int op_ret;`，未初始化
- `src/tuya_cloud_service/lan/tuya_lan.c:1369` / `:1376` / `:1381` —— 三条不给 `op_ret` 赋值就 `goto __exit` 的路径
- `src/tuya_cloud_service/lan/tuya_lan.c:1392` —— `return op_ret;`
- `src/tuya_cloud_service/lan/tuya_lan.c:208` —— 同一条清理路径上的空指针解引用
- 唯一的活调用方：`src/tuya_cloud_service/netmgr/src/netmgr.c:562`

### 问题是什么

`op_ret` 只在三处被赋值：`:1353`（`tuya_sock_loop_init()`）、`:1357`、`:1361`（两个互斥锁）。这三处都成功之后，`op_ret == OPRT_OK`。接下来三条失败路径**都不给它赋新值**：

| 行 | 失败的动作 | `goto __exit` 时 `op_ret` 的值 | 有日志吗 |
| --- | --- | --- | --- |
| `:1367-1369` | `session` 数组 `tal_malloc()` 失败 | `OPRT_OK` | **没有** |
| `:1374-1376` | `lan_tcp_create_serv_socket()` 失败 | `OPRT_OK` | 有 `PR_ERR`（`:1375`） |
| `:1379-1381` | `lan_udp_create_serv_socket()` 失败 | `OPRT_OK` | 有 `PR_ERR`（`:1380`） |

`__exit`（`:1387-1392`）先调 `tuya_lan_exit()` 把 `s_lan_mgr` 释放并置 `NULL`，然后 `return op_ret;` —— 也就是**在 LAN 已经彻底不存在的状态下向上返回成功**。

同一条清理路径上还有一个真的空指针解引用，而且正好在最没有日志的那条路径上：`:1369` 走 `__exit` → `tuya_lan_exit()` → `lan_session_close_all()`（`:187`）→ `:208` 的 `if (lan->session[i].active)`，此时 `lan->session` 是 `NULL`。这里没有侥幸可言：`lan->cfg` 在 `:1349` 已赋值所以循环上界是有效的，`lan->mutex` 在 `:1357` 已创建所以 `:198` 的加锁能过。malloc 失败这条路径是先崩，崩不掉才轮到返回错误的码。

顺带说明一个**看着像但其实不是**的猜测，因为它是下一个人最可能猜的：两条互斥锁创建失败的路径同样会走到 `lan_session_close_all()`，此时 `lan->mutex == NULL`。但那不是空指针解引用 —— `tkl_mutex_lock()` 在两个平台上都判空：`platform/T5AI/tuyaos/tuyaos_adapter/src/system/tkl_mutex.c:78` 和 `platform/LINUX/tuyaos_adapter/src/tkl_mutex.c:83`。这两条路径会正常返回错误码。

### 怎么观察到

`netmgr.c:562` 是 `tuya_lan_init()` 在整棵树里唯一的活调用方（`tuya_lan_enable()` 也调它，但那个函数零调用方，见 §4）：

```c
s_netmgr.lan_started = TRUE;                       // netmgr.c:558
if (OPRT_OK != tuya_lan_init(client)) {            // netmgr.c:562
    s_netmgr.lan_started = FALSE;                  // netmgr.c:564 —— 永远不会执行
    PR_ERR("netmgr LAN init failed on [%s], retrying at the next link change", ...);
}
```

netmgr 的回滚逻辑写得是对的，但因为被调方谎报成功，`:564` 永远不会执行。`lan_started` 停在 `TRUE`，LAN 门控在 `netmgr.c:520` 每次都提前返回，**LAN 在整个进程生命周期里不会被再试一次**。现场表现是：设备正常上云，但 LAN 直连、局域网发现、局域网 OTA 全部没有，日志里只有一行 `PR_DEBUG("init error")`（`:1388`），最坏的那条路径（malloc 失败）连这行之外什么都没有。

### netmgr 今天怎么处理

netmgr 做了它能做的全部：`netmgr.c:558-566` 先置位再调用、失败回滚、并在注释里说明"下次链路变化时重试"。它无法处理的是被调方返回值本身是假的。

### 建议的修法与影响面

**`:1352` 改成 `int op_ret = OPRT_COM_ERROR;` 不能修掉这三条路径 —— 不要这么做。** 这是一个死存储：下一行 `:1353` 就是 `op_ret = tuya_sock_loop_init();`，在任何一条 `goto __exit` 有机会被执行到之前，`op_ret` 已经被这一行和随后两个互斥锁的赋值覆盖了三次。等控制流走到 `:1367`（session 判空）、`:1374`（tcp socket）、`:1379`（udp socket）任何一条失败路径时，`op_ret` 已经是 `OPRT_OK`。初始值唯一能生效的场景 —— 第一次赋值之前就发生 `goto __exit` —— 在这个函数里不存在。

正确的修法是三条路径各自在 `goto __exit` 之前赋一个有意义的错误码，而不是依赖一个走不到的初始值：

- `:1367` session 数组 `tal_malloc()` 失败 → `op_ret = OPRT_MALLOC_FAILED;`，并补一条 `PR_ERR`（这条路径是三条里唯一连日志都没有的，见上表）；
- `:1374` `lan_tcp_create_serv_socket()` 失败 → `op_ret = OPRT_SOCK_ERR;`，已有的 `PR_ERR("init tcp serv fd err")` 保留；
- `:1379` `lan_udp_create_serv_socket()` 失败 → `op_ret = OPRT_SOCK_ERR;`，已有的 `PR_ERR("init udp serv fd err")` 保留。

`:1352` 的初始值可以保留，但它的角色是给未来编辑的兜底（万一以后有人在第一次赋值之前加一条新的失败路径又忘了赋值），不是这三条路径的修法。

另外 `lan_session_close_all()` 在 `:207` 的循环前加 `lan->session` 判空，这一条判空修法是对的。

影响面：`tuya_lan_init()` 的返回值变成可信的，于是 `netmgr.c:564` 的回滚开始生效，LAN 会在下一次链路变化时重试 —— 这本来就是已经写好的预期行为。没有其他调用方，所以影响面就到这里。

---

## 3. LAN 与 AI monitor 共用一个 socket loop，没有引用计数，两处空指针解引用 —— 【已修复 `f2323e11..efb118a4`；实际修法是每个 owner 自建 loop，不是引用计数】

### 涉及文件

- `src/tuya_cloud_service/lan/lan_sock.c:46` —— `static P_LAN_SLOOP_S g_sloop = NULL;`，全局单例
- 两个所有者：`src/tuya_cloud_service/lan/tuya_lan.c:1353` 与 `src/tuya_ai_service/svc_ai_monitor/src/tuya_ai_monitor.c:901`
- `src/tuya_cloud_service/lan/lan_sock.c:299-301` —— `tuya_sock_loop_init()` 在已存在时直接返回 `OPRT_OK`，不计数
- `src/tuya_cloud_service/lan/lan_sock.c:91-93` —— 拆除时关掉**所有** reader fd
- `src/tuya_cloud_service/lan/lan_sock.c:358` / `:383` —— 两处无判空的 `g_sloop->queue`
- `src/tuya_cloud_service/lan/tuya_lan.c:1454-1459` —— 最长 3000 ms 的忙等
- 触发点：`src/tuya_cloud_service/netcfg/ap_netcfg.c:863`

### 问题是什么

`g_sloop` 是一个进程级单例，两个模块都会去初始化它：LAN（`tuya_lan.c:1353`）和 AI monitor（`tuya_ai_monitor.c:901`）。`tuya_sock_loop_init()` 在 `lan_sock.c:299-301` 发现 `g_sloop` 已存在就直接返回 `OPRT_OK` —— **第二个所有者拿不到任何句柄，也没有任何计数记录它是所有者之一**。

于是任何一个所有者调 `tuya_lan_disable()` 就会把整个 loop 拆掉：

```
tuya_lan_disable()                          tuya_lan.c:1431
  └─ tuya_sock_loop_disable()               tuya_lan.c:1454  → terminate = FALSE
       └─ tuya_sock_loop_run() 退出 while    lan_sock.c:196
            ├─ 逐个调 reader 的 quit()        lan_sock.c:263-267
            ├─ tuya_lan_exit()               lan_sock.c:277
            └─ __ty_sock_loop_deinit()       lan_sock.c:278
                 ├─ tal_net_close() 每一个 reader fd   lan_sock.c:91-93
                 └─ g_sloop = NULL                     lan_sock.c:112
```

`lan_sock.c:91-93` 关的是 `g_sloop->readers` 里的**每一个** fd，包括 AI monitor 在 `tuya_ai_monitor.c:671` 和 `:740` 注册进来的那两个。之后 `g_sloop == NULL`，而 AI monitor 重新注册的路径上：

- `tuya_reg_lan_sock()`，`lan_sock.c:358`：`tal_queue_post(g_sloop->queue, ...)`，**无判空**
- `tuya_unreg_lan_sock()`，`lan_sock.c:383`：同样，**无判空**

这就是空指针解引用的两处。此外 `tuya_lan.c:1456-1459` 会以 50 ms 一步忙等最长 3000 ms 等 loop 真的下去。

同一个函数里还有一处值得复核的写法：`lan_sock.c:108-109` 在 loop 线程自己的上下文里调 `tal_thread_delete(g_sloop->thread)`，即线程删除自己。这条不在本节的结论里，只是标记出来 —— 修引用计数的人会正好读到这段。

### 怎么观察到

设备同时启用 LAN 和 AI monitor，并且走到任何一个调 `tuya_lan_disable()` 的地方：

- `src/tuya_cloud_service/netcfg/ap_netcfg.c:863` —— **AP 配网开始时**，这是现场最容易走到的一条；
- `src/tuya_cloud_service/cloud/tuya_iot.c:779`、`:1154`。

之后 AI monitor 下一次注册 socket 就是一次空指针解引用。

### netmgr 今天怎么处理

netmgr **自己**完全绕开了：它的 LAN 门控只负责"启动"，永远不调 `tuya_lan_disable()`。`netmgr.c:454-501` 的注释给了四条理由，任何一条单独成立，其中前两条正是本节：

> - `tuya_lan_disable()` closes every reader fd on the shared socket loop, which the AI monitor also uses, and there is no reference count. After it runs the AI monitor dereferences `g_sloop == NULL`;
> - `tuya_lan_disable()` blocks for up to 3000 ms. Called from here it would park the system work queue for that long and blow `netmgr_deinit()`'s 2000 ms drain budget;

第三条理由说明为什么不需要停：两个 LAN 服务端 socket 都绑在通配地址上，LAN 每个包都通过 `netmgr_conn_get()` 现读活跃地址，所以链路切换会自愈。第四条直接写了"干净的替代方案今天是坏的，采用它等于用一个能工作的行为换一次崩溃"。

**但 netmgr 的自律不构成全局保护** —— `ap_netcfg.c:863` 就在调它。所以这一条是活的 bug，只是 netmgr 不在触发链上。

### 建议的修法与影响面

三件事，可以分开做：

1. `tuya_sock_loop_init()` / `tuya_sock_loop_disable()` 加引用计数，最后一个所有者退出时才真的拆 loop；
2. `tuya_reg_lan_sock()`（`:358`）和 `tuya_unreg_lan_sock()`（`:383`）加判空，这两行是止血，可以先做；
3. `lan_sock.c:277` 的 `tuya_lan_exit()` 不应该由 loop 线程代替一个它并不认识的所有者去调 —— loop 的退出路径不该越过所有权边界。

影响面：LAN + AI monitor + `ap_netcfg`。同时这是**解锁 netmgr 那条被主动放弃的能力**的前置：只有引用计数和判空都在了，netmgr 才可能实现"路由离开带 `NETCONN_CAP_LAN` 的链路时停 LAN、回来时再起"。`netmgr.c:483-500` 已经把那件事该满足的条件写下来了。

---

## 4. AP 配网关掉 LAN，没有任何东西把它开回来 —— 【活的 bug】

### 涉及文件

- `src/tuya_cloud_service/netcfg/ap_netcfg.c:863` —— `ap_netcfg_start()` 的第一件事就是 `tuya_lan_disable()`
- `src/tuya_cloud_service/netcfg/ap_netcfg.c:931-947` —— `ap_netcfg_stop()`，**没有**任何 LAN 相关动作
- `src/tuya_cloud_service/lan/tuya_lan.c:1470-1482` —— `tuya_lan_enable()`，**零调用方**
- `src/tuya_cloud_service/netmgr/src/netmgr.c:558` —— `lan_started = TRUE`，只有 `netmgr_init()` / `netmgr_deinit()` 会清

### 问题是什么

`ap_netcfg_start()` 在 `:863` 把 LAN 停掉（理由是可以理解的：AP 模式下设备换了网段，LAN 的那套 socket 没有意义）。但配网结束时 `ap_netcfg_stop()`（`:931-947`）只做三件事 —— 停 AP、切回 station 模式、停广播定时器 —— **没有任何东西把 LAN 开回来**。

树里确实有一个 `tuya_lan_enable()`（`tuya_lan.c:1470`），签名和语义都对得上，但对 `src/`、`apps/`、`examples/` 全量搜索的结果是：**零调用方**。它只被自己的声明和定义提到。

netmgr 也不会替它补上：`netmgr.c:558` 在第一次成功启动 LAN 时置了 `lan_started = TRUE`，这个字段在 `netmgr.c` 里一共只有 5 处出现（`:183` 定义、`:520` 读、`:554` 读、`:558` 置位、`:564` 回滚），没有任何路径会因为"LAN 被别人停了"而把它清掉。于是 LAN 门控在 `:520` 每次都提前返回。

### 怎么观察到

一台已经上云、LAN 正常工作的设备，重新配一次网（按键复位进 AP 配网，或 App 触发重新配网），配网成功回到在线状态之后：App 的局域网直连、局域网发现、局域网 OTA 全部没有，**直到重启**。云端链路完全正常，所以问题看起来不像网络问题。

### 这不是重构引入的回归

这一条我专门去核了历史，因为"重构前那个 500 ms 轮询定时器会把它救回来"是一个非常像对的错误答案。实际不是：

```
$ git show 92cec3f2:src/tuya_cloud_service/netmgr/src/netmgr.c
...
167:    if ((type & NETCONN_WIRED || type & NETCONN_WIFI) && client->is_activated) {
168:        PR_DEBUG("Start LAN initialization");
169:        tuya_lan_init(client);
170:        tal_sw_timer_stop(sg_lan_init_timer);
171:    }
```

重构前的轮询回调在第一次 `tuya_lan_init()` 之后立刻 `tal_sw_timer_stop()`（`:170`），**同样是一次性的**，同样不会重试。而且它连返回值都不看（`:169`）。所以行为在重构前后一致；新的门控至少检查了返回值（尽管 §2 使这个检查形同虚设）。

### netmgr 今天怎么处理

不处理。这是 netcfg 单方面动了 LAN 的生命周期，而 netmgr 的 `lan_started` 记账不知道这件事。

### 建议的修法与影响面

正确的修法是**让 netcfg 不再拥有 LAN 的生命周期**：配网开始/结束发事件，由 LAN 的所有者（今天是 netmgr 的门控）去响应。这样 `lan_started` 天然跟着走。

最小修法是 `ap_netcfg_stop()` 调 `tuya_lan_enable()`，同时 netmgr 需要一个清 `lan_started` 的入口。**两种修法都必须先做 §3**：

- 重新 `init` 需要 socket loop 真的已经下去。`tuya_lan.c:1456` 的 3000 ms 只是等，不是保证；
- `tuya_lan_enable()` → `tuya_lan_init()` → `tuya_sock_loop_init()` 会在 `lan_sock.c:299` 静默复用一个可能仍被 AI monitor 持有的 loop。

影响面：`ap_netcfg` + LAN + netmgr 的 `lan_started` 一个字段。

---

## 5. `ENABLE_BLUETOOTH=y` 配 `ENABLE_WIFI=n` 编译不过 —— 【活的 bug】

### 涉及文件

- `src/tuya_cloud_service/CMakeLists.txt:98-104` —— `netcfg/*.c` 源文件**和** netcfg 头文件目录，都只在 `CONFIG_ENABLE_WIFI` 下加入
- `src/tuya_cloud_service/CMakeLists.txt:118-123` —— `ble/*.c` 在 `CONFIG_ENABLE_BLUETOOTH` 下加入
- `src/tuya_cloud_service/cloud/tuya_iot.c:40-41` —— `#include "netcfg.h"` 自己就在 `#if ENABLE_WIFI` 里面
- `src/tuya_cloud_service/cloud/tuya_iot.c:783-784` —— `netcfg_stop(NETCFG_TUYA_BLE)` 在 `#if ENABLE_BLUETOOTH` 里面
- `src/tuya_cloud_service/ble/ble_netcfg.c:18` / `:181`

### 问题是什么

netcfg 的构建门是 **wifi**，而不是"支持配网的技术的并集"：

```cmake
if(CONFIG_ENABLE_WIFI STREQUAL "y")                          # :98
    file(GLOB_RECURSE WIFI_SRCS
        "${MODULE_PATH}/netmgr/src/conn/netconn_wifi.c"
        "${MODULE_PATH}/netcfg/*.c")                         # :101
    list(APPEND LIB_SRCS ${WIFI_SRCS})
    list(APPEND  LIB_PUBLIC_INC ${MODULE_PATH}/netcfg)       # :103
endif()
```

于是 `WIFI=n` + `BLUETOOTH=y` 会在两个互相独立的地方断掉：

1. `tuya_iot.c:784` 的 `netcfg_stop(NETCFG_TUYA_BLE)` 既没有声明（`netcfg.h` 的 include 在 `:41`，被 `:40` 的 `#if ENABLE_WIFI` 包着）也没有定义（`netcfg/netcfg.c` 没进编译）。编译错 + 链接错。
2. **更根本的一处，和 `tuya_iot.c` 无关**：`ble/ble_netcfg.c` 是被 `:118-123` 编进来的，而它 `:18` 的 `#include "netcfg.h"` 找不到头文件 —— 把那个目录放到 include path 上的正是 `:103`。它 `:181` 还调了 `netcfg_register()`，定义在 `netcfg/netcfg.c:74`。也就是说 **BLE 配网模块自己就无法在没有 wifi 的情况下构建**。

### 怎么观察到

一个纯 BLE 产品（没有 wifi 的设备，靠 BLE 配网）根本编不出来。

树里有一处已经把这件事写下来了：`apps/tuya_cloud/switch_demo/config/Ubuntu.config:18-24` 明确因为这个原因把 bluetooth 关着 ——

> Bluetooth stays off, and not by preference: `tuya_iot.c` calls `netcfg_stop()` from under `ENABLE_BLUETOOTH`, but netcfg's sources and include directory are added by `src/tuya_cloud_service/CMakeLists.txt` only under `ENABLE_WIFI`. So bluetooth-without-wifi does not compile.

需要更正一处传闻：本次重构的两个验证 config 里**只有这一个**引用了它。另一个（`apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config:42-45`）说明的是另一件事 —— T5AI 上 `ENABLE_WIFI` 和 `ENABLE_BLUETOOTH` 在 `boards/T5AI/TKL_Kconfig` 里是无 prompt 的 bool 且默认 `y`，config 文件动不了它们，所以那个目标根本无法构造出这个组合。

### netmgr 今天怎么处理

不处理，netmgr 不在这条链上。它只是在验证矩阵里撞到了这堵墙，并把墙的位置写在了 config 注释里。

### 建议的修法与影响面

按"是否与技术无关"把 netcfg 拆成两半，`netcfg/` 目录只有 5 个文件，拆点很干净：

| 文件 | 归属 | 新的门 |
| --- | --- | --- |
| `netcfg.c` / `netcfg.h` | 与技术无关的注册/分发核心 | `ENABLE_WIFI` **或** `ENABLE_BLUETOOTH` |
| `ap_netcfg.c` / `ap_netcfg.h` / `ap_pbkdf2.c` | 只属于 wifi AP 配网 | `ENABLE_WIFI` |

同时把 `tuya_iot.c:41` 的 `#include "netcfg.h"` 挪出只由 wifi 控制的块。

影响面：CMakeLists 加一个 include 位置，不动任何业务逻辑。**但它需要一个新的验证 config** —— 树里今天没有任何目标构建 `WIFI=n` + `BLE=y`，所以这个修复不做一个新 config 就无法证明。

---

## 6. `tkl_net_getsockname()` 各平台语义不一致 —— 【T3 上的一半已绕开；T3 的另一半和 ESP32 是活的】

### 涉及文件

- `src/tal_network/src/tal_network.c:345` —— `__net_connect_bind_active_src()`，从 `tal_net_connect()` 的 `:422` 调用
- `src/tal_network/src/tal_network.c:364-374` —— 记录"输出参数预先清零"的那段注释
- `src/tal_network/src/tal_network.c:375` / `:378` / `:382` —— 三个分支
- `src/tal_network/include/tal_net_provider.h:87-91` —— 决定哪个后端应答
- `src/tal_network/src/tal_net_tkl.c:80` —— TKL 后端把 `.getsockname` 接到 `tkl_net_getsockname`
- 各平台实现，见下表
- 第二个消费者：`src/tuya_p2p/pjproject/pjlib/src/pj/sock_tal.c:483`

### 问题是什么

M0 加的出向源地址绑定（`tal_network.c:345`）用 `getsockname` 做一次探测，三条分支：

```c
if (OPRT_OK != tal_net_getsockname(fd, &local, &local_port)) {   // :375
    return;                        // 探测不可靠 → 不碰
}
if ((0 != TUYA_IP_ADDR_GET_IP4(local)) || (0 != local_port)) {   // :378
    return;                        // 调用方自己绑过了 → 不碰
}
if (0 != tal_net_bind(fd, src, 0)) { ... }                       // :382
```

哪个后端来应答由 `tal_net_provider.h:87-91` 决定：`ENABLE_LIBLWIP == 1` 或 `OPERATING_SYSTEM == SYSTEM_LINUX` 用 POSIX 后端，否则用 TKL 后端。逐平台核下来：

| 平台 | 后端 | `getsockname` 实现 | 源地址绑定的实际效果 |
| --- | --- | --- | --- |
| T5AI | TKL（`default.config` 未开 liblwip） | 真实实现，`platform/T5AI/tuyaos/tuyaos_adapter/src/system/tkl_network.c:857-880` | 正常 |
| **T3** | TKL（`platform/T3/default.config:26` 未开 liblwip） | **`return 0;`，不写任何输出**，`platform/T3/tuyaos/tuyaos_adapter/src/system/tkl_network.c:846-849` | 见下，一半靠运气一半是坏的 |
| **ESP32** | TKL（`platform/ESP32/default.config:27` 未开 liblwip） | **`OPRT_NOT_SUPPORTED`**，`platform/ESP32/tuya_open_sdk/tuyaos_adapter/src/drivers/tkl_network.c:961-964` | **静默完全不生效** |
| T2 / BK7231X / LN882H | POSIX（`boards/*/config/*.config` 里 `CONFIG_ENABLE_LIBLWIP=y`） | 真实实现，`src/tal_network/src/tal_net_posix.c:1071-1093` | 正常 |
| LINUX | POSIX | 同上 | 整个 helper 被 `#if OPERATING_SYSTEM != SYSTEM_LINUX` 编译掉（`tal_network.c:387`），这是故意的 |

LN882H 也有一个 `OPRT_NOT_SUPPORTED` 的桩（`platform/LN882H/tuyaos/tuyaos_adapter/src/tkl_network.c:963-968`），但默认配置绕开了它（`platform/LN882H/default.config:27` 是 `CONFIG_ENABLE_LIBLWIP=y`）。它今天是死代码，谁把 liblwip 关掉它就活。

**T3 那一半已绕开的**：`tal_network.c:364-374` 特意把 `local` / `local_port` 预先清零，并在注释里写明理由就是"某些后端的 getsockname 是个报告成功但不写输出的桩"。所以 T3 上读到的是 `0/0` 而不是栈上垃圾，一个新 socket 会走到 `:382` 去绑定 —— 结果是对的。这一半是有注释记录的绕开。

**T3 那一半是坏的，而且预清零救不了**：`:378` 那条"调用方自己绑过了就不要动"的保护，在 T3 上是**失效的**。桩永远报告 `0/0`，所以一个调用方已经 `bind()` 过的 socket 会被判定为未绑定，然后在 `:382` 被**再绑一次**到活跃链路地址。这正是 `tal_network.c:329-332` 的注释说不能干的事：

> ... and on lwIP a second bind of a still-CLOSED pcb succeeds and silently moves the local address (`tcp_bind()` only rejects a pcb that left CLOSED), so an unconditional bind here would quietly corrupt candidate gathering instead of failing loudly.

也就是说：在 T3 上 `__net_connect_bind_active_src()` 事实上退化成了"无条件绑定"，正是那段注释要避免的形态。

**ESP32 那一条是纯粹的活缺口**：`:375` 直接返回，不绑定，**没有任何日志**。一块多链路 ESP32 板子的出向 socket 会跟随 lwIP 路由表而不是 netmgr 选中的链路。失效方向是安全的（单链路设备完全不受影响），但是静默的。

### 怎么观察到

- ESP32 多链路板子：netmgr 报告的活跃链路和实际发包走的接口可能不一致，而没有任何提示。
- T3 且启用了 `tuya_p2p`：`pj_sock_getsockname()`（`sock_tal.c:483`）拿到全零的地址，pjlib 自己有兜底（`sock_tal.c:493` 用 station IP 补地址），但**端口补不回来，仍然是 0** —— ICE host candidate 的端口是 0。这是从源码推出的结论，我没有在设备上跑过。
- ESP32 / 关了 liblwip 的 LN882H 且启用了 `tuya_p2p`：`NOT_SUPPORTED` 直接变成 pjlib 的错误返回。

### netmgr 今天怎么处理

`tal_network.c:364-374` 的预清零是唯一的对策，它只覆盖"报告成功但不写输出"这一种坏法，不覆盖"根本不支持"，也不覆盖被它掩盖掉的 `:378` 保护失效。

### 建议的修法与影响面

1. T3（`platform/T3/.../tkl_network.c:846-849`）和 ESP32（`platform/ESP32/.../tkl_network.c:961-964`）实现真实的 `getsockname`，照抄 T5AI 的 `:857-880` 即可，约 15 行。
2. 更一般的规矩：**一个 TKL 函数不能在不兑现出参的情况下返回 `OPRT_OK`**。T3 那个文件里同样形状的还有两处：`tkl_net_getpeername`（`:876-879`）和 `tkl_net_set_broadcast`（`:860-863`）。

影响面：只在各自平台的适配文件里。行为从"没有源地址绑定"变成"有源地址绑定"（ESP32）、从"无条件绑定"变成"按约定绑定"（T3），这两个都是 M0（commit `b80055e6`）本来就想要的行为，不是新语义。

---

## 7. 没有任何 TAL/TKL 入口能撤回一个已安装的 wired 回调 —— 【已绕开】

### 涉及文件

- `src/tal_wired/include/tal_wired.h:37`、`:47`、`:57`、`:67`、`:77`、`:87` —— 全部 6 个函数，都是状态/配置，没有 uninit
- `src/tal_wired/src/tal_wired.c:54-57` —— 纯转发到 `tkl_wired_set_status_cb()`
- `platform/LINUX/tuyaos_adapter/src/tkl_wired.c:129`、`:156`、`:158` —— `NULL` 不被当作注销
- `src/tuya_cloud_service/netmgr/src/conn/netconn_wired.c:78` —— 安装回调
- `src/tuya_cloud_service/netmgr/src/conn/netconn_wired.c:120-123` —— `close()` 是有注释的空实现
- `src/tuya_cloud_service/netmgr/src/netmgr_priv.h:38-168` —— `netmgr_deinit()` 的设计记录，其中 `:121-126` 就是这一条

### 问题是什么

`tal_wired.h` 一共 6 个函数，全部是读状态和配地址/MAC，**没有 uninit，也没有任何方式表达"别再叫我了"**。`tal_wired.c` 只是转发，所以能力上限就是 TKL 的上限。

`NULL` 也不是注销。在真正发货的 LINUX 适配里，`platform/LINUX/tuyaos_adapter/src/tkl_wired.c:129` 把整个函数体放在 `if (cb)` 里 —— 一个 `NULL` 参数被**整个忽略**，连 `:156` 存下来的指针都不会被清掉；而 `:158` 创建的轮询线程（`if (!wired_event_thread)` 保护，所以只有一个）会一直活到进程结束。在 porting 模板里情况更糟，`NULL` 会导致空函数指针调用，见 §1。

于是：`netconn_wired_open()` 在 `netconn_wired.c:78` 装上 `__netconn_wired_event`，`netconn_wired_close()`（`:120-123`）什么也做不了。`netmgr_deinit()` 之后，平台轮询线程**仍在运行**，仍在调 `__netconn_wired_event`（`netconn_wired.c:49`），而后者在 `:57` 不加锁地读 `netmgr_wired->base.event_cb` 然后调进 netmgr。这就是 use-after-free。

### 怎么观察到

`netmgr_deinit()`（`netmgr.c:2251`）在 `apps/` 和 `examples/` 里**没有任何调用方**。它今天只有两类调用者：netmgr_init() 自己的错误回滚，以及 M2 新加的串口 CLI 命令 `netmgr deinit` / `netmgr init`（`netmgr_cli.c:45`、`:756`、`:847`）。所以今天它只能从串口控制台触达 —— 而 `netmgr_cli.c:819-834` 正好在那里打印了警告：

> netmgr init: a re-init cannot withdraw callbacks a driver already installed, so a platform poller may outlive the netmgr it calls

同一段注释还特意说明了**不该说什么**：不要说线程数会增长。在发货平台上不会，`if (!wired_event_thread)` 保证了只有一个。

### netmgr 今天怎么处理

`netmgr_priv.h:38-168` 是 `netmgr_deinit()` 的完整设计记录，`:121-126` 把这一条称为"这份设计记录存在的理由，那个具体的 use-after-free"。对策有两条，都是"活下来"而不是"修好"：

1. **互斥锁永久保留、从不释放**（`netmgr_priv.h:57-66`）。理由写得很直接：驱动不加锁读 `base.event_cb`，而没有任何 TAL 层能撤回已装的回调，所以任何"先测标志再加锁"的保护都只是把窗口变窄。**一个进程里留一把锁，好过一把被释放之后又被加锁的锁。**
2. **`netmgr_notify_link()` 用 `stopping` 标志把门关上**（`netmgr.c:137`、`:311-314`）：`netmgr_deinit()` 在动任何东西之前置位，只有 `netmgr_init()` 会清。

另外 `netmgr_priv.h:104-116` 明确告诉未来的驱动作者：**不要**试图用 `NULL` 清回调。

同一个形状在 wifi 上也存在：`src/tal_wifi/include/tal_wifi.h` 里没有任何 `uninit` / `deinit`，所以 `WIFI_EVENT_CB` 同样撤不回来（`netmgr_priv.h:143-148`）。

### 建议的修法与影响面

需要一个新的 TKL 入口。两条路：

- 让 `tkl_wired_set_status_cb(NULL)` 在**契约上**表示注销，并让每个适配真的兑现（停轮询，或至少停止调用）；
- 或者新增 `tkl_wired_uninit()`。

影响面是本文里最大的：TKL 头文件变更 → 树内 3 个 wired 适配（`platform/LINUX`、`platform/T3`、`platform/T5AI` 各有 `include/wired/tkl_wired.h`）+ `tools/porting/template/linux/tkl_wired.c` + `tools/porting/adapter/wired/tkl_wired.h` + 所有树外移植。这正是 netmgr 选择"活下来"而不是"修好"的原因。

一旦它存在，netmgr 可以做两件今天做不了的事：释放它保留的互斥锁，以及让 `netconn_wired_close()` 变成真的实现。

---

## 8. `tal_cellular` 没有 deinit，也没有 connect/disconnect 对 —— 【已绕开，而且语义已经进了类型系统】

### 涉及文件

- `src/tal_cellular/include/tal_cellular.h:79` —— `tal_cellular_init()`，之后 `:88`–`:175` 全是 getter 和一个状态回调（`:97`）
- `platform/T5AI/tuyaos/tuyaos_adapter/include/cellular/tkl_cellular.h` —— 13 个函数，没有一个能把承载降下来
- `platform/T5AI/tuyaos/tuyaos_adapter/src/driver/tkl_cellular.c:47` / `:399` / `:424` —— 能力其实存在，但只在产测路径上
- `src/tuya_cloud_service/netmgr/include/netconn_registry.h:73-85` —— `NETCONN_CTRL_SUSTAINED` 的定义与文档
- `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c:176` —— 蜂窝行的控制级别
- `src/tuya_cloud_service/netmgr/src/conn/netconn_cellular.c:130-135` / `:154-162`

### 问题是什么

`tal_cellular.h` 只有一个 `tal_cellular_init()`（`:79`）负责把数据承载拉起来，之后全是读取类接口。**没有 deinit，没有 connect/disconnect 对。** netmgr 因此永远无法把一条蜂窝链路降下来。

值得单独指出的是：**能力在厂商 SDK 里是存在的**。`platform/T5AI/.../tkl_cellular.c:47` 声明了 `extern bk_err_t bk_modem_deinit(void);`，并在 `:399` 和 `:424` 真的调用了它 —— 但那两处都在 `tkl_cellular_mf_test_start()` / `tkl_cellular_mf_test_stop()` 里面，即产测路径。同一个文件里躺着一个能用的拆除动作，正常 API 没有把它暴露出来。

### 怎么观察到

- `netmgr_conn_set(NETCONN_CELLULAR, NETCONN_CMD_CLOSE, NULL)` 返回 `OPRT_NOT_SUPPORTED`（升级说明 §2.1 把这个记为可见的行为变化）；
- 一条蜂窝链路无法为省电而挂起，无法被 netmgr 的连接退避重试，并且在 wifi 被优选的时候仍然保持在线（继续计费）；
- `tuya_iot.c:800` 拿到这个错误码之后直接丢掉，见 §10。

### netmgr 今天怎么处理

**把这件事编码进了类型系统**，这是四条"已绕开"里做得最彻底的一条：

- `netconn_registry.h:73-85` 定义了 `NETCONN_CTRL_SUSTAINED`（"驱动自持"：netmgr 通过 `open()` 启动一次，之后驱动自己维持；没有逐次的 connect/disconnect，所以 netmgr 不能重试、不能退避、不能挂起省电），文档里直接点名 `tal_cellular.h`；
- `netconn_table.c:176` 把蜂窝行放在这个级别；
- `netconn_cellular.c:130-135` 的 `close()` 是一个有注释的空实现；
- `netconn_cellular.c:154-162` **故意没有** `NETCONN_CMD_CLOSE` 分支，让请求落到 `default:` 返回 `OPRT_NOT_SUPPORTED`。这段注释写明了为什么：原来那个分支调完空的 `close()` 就返回 `OPRT_OK`，等于告诉 `tuya_iot_destroy()` 链路已经关了，而它还开着。"这不是 netmgr 还没支持 —— 是根本没有 TKL 入口可以支持。"

所以调用方今天拿到的是真话，能力缺失是显式的、可读的、可查的。

### 建议的修法与影响面

加 `tal_cellular_deinit()`，理想情况再加 `tal_cellular_connect()` / `tal_cellular_disconnect()`。对 T5AI 来说 deinit 那一半已经躺在 `tkl_cellular.c:399` 了。

之后：`netconn_cellular_close()` 变成真实现，`netconn_table.c:176` 从 `NETCONN_CTRL_SUSTAINED` 升到 `NETCONN_CTRL_MANAGED`（或一个中间级），netmgr 的退避和策略层开始对蜂窝生效。

影响面：`tal_cellular.h` + `tkl_cellular.h` + T5AI 适配（树内唯一的蜂窝移植）+ 表里一行。相当收敛，而且**上面的每一层都已经写好了接它的准备** —— 注册表的控制级别就是为这件事留的接缝。扩展指南里讲了怎么加一种链路技术，这一条是同一个接缝的另一侧。

---

## 9. `tuya_mqtt_stop()` 的赋值顺序让订阅者分不清主动停止和 keepalive 死亡 —— 【已绕开】

### 涉及文件

- `src/tuya_cloud_service/cloud/mqtt_service.c:498-513` —— `tuya_mqtt_stop()`
- `src/tuya_cloud_service/cloud/mqtt_service.c:508` —— `mqtt_client_disconnect()`
- `src/tuya_cloud_service/cloud/mqtt_service.c:511` —— `context->manual_disconnect = true;`
- `src/tuya_cloud_service/cloud/mqtt_service.c:837` —— 这个标志唯一的消费点
- `src/tuya_cloud_service/cloud/tuya_iot.c:443` —— `tal_event_publish(EVENT_MQTT_DISCONNECTED, ...)`
- `src/tuya_cloud_service/netmgr/src/probe/netmgr_probe.c:104-115` —— netmgr 对这件事的记录

### 问题是什么

```c
int tuya_mqtt_stop(tuya_mqtt_context_t *context)
{
    ...
    mqtt_status = mqtt_client_disconnect(context->mqtt_client);   // :508
    PR_DEBUG("MQTT disconnect result:%d", mqtt_status);

    context->manual_disconnect = true;                            // :511
    return OPRT_OK;
}
```

`mqtt_client_disconnect()` 会跑断连回调，其尾部在 `tuya_iot.c:443` 发布 `EVENT_MQTT_DISCONNECTED`。也就是说**事件在 `:508` 就发出去了，标志在 `:511` 才置上**。任何订阅者在处理这个事件时读到的 `manual_disconnect` 都还是 `false`，与 keepalive 超时导致的断连完全无法区分。这个标志今天只被 `mqtt_service.c` 自己在 `:837` 读。

### 怎么观察到

`netmgr_probe.c` 把 `EVENT_MQTT_DISCONNECTED` 当作一次 BAD 判决。`netmgr_probe.c:113-115` 把机制写得很准：

> `tuya_mqtt_context_t.manual_disconnect` is no help even to code that could read it, because `tuya_mqtt_stop()` sets it AFTER `mqtt_client_disconnect()` has already published the event (`mqtt_service.c:508` then `:511`).

### netmgr 今天怎么处理

三层吸收，都在探测层里：

1. **路由 epoch。** `netmgr_probe.c:136-175` 在 CONNACK 时记下当时的路由 epoch，一次 MQTT 会话完整地活在一个 epoch 里。netmgr **自己的**切换引发的 `tuya_mqtt_stop()` 带着过期的 epoch，会被丢掉。没有这一层的话，每次切换都会有一个虚假的 BAD 打在刚切过去的那条链路上 —— `netmgr_probe.c:143-155` 说这不是罕见竞态而是"每次必然发生"。
2. **`probe_bad_threshold > 1`**（`netmgr_policy.h:489`）：一次 BAD 不会导致降级。
3. **真正负责降级的是 `verify_timeout_ms`，不是这个计数器**（`netmgr_policy.h:477-489`）：被动后端每次建立会话都发一个 GOOD 把计数清零，所以判决流是 GOOD/BAD/GOOD/BAD，计数永远到不了阈值。

**残余暴露面**：一次不是 netmgr 引起的主动 `tuya_mqtt_stop()`，epoch 是**活的**，会作为一个看起来完全正常的 BAD 落在一条健康链路上。`netmgr_probe.c:104-109` 点了三个这样的调用点，我逐个核过：

| 调用点 | 上下文 |
| --- | --- |
| `tuya_iot.c:555` | `run_state_reset()` |
| `tuya_iot.c:789` | `tuya_iot_destroy()` |
| `tuya_iot.c:1108` | `STATE_STOP` |

### 建议的修法与影响面

把 `:511` 挪到 `:508` 之前。两行。

影响面：这个标志今天只在 `mqtt_service.c:837` 被读，提前置位在那里严格更安全（`tuya_mqtt_loop()` 会早那么一点拒绝重连）。这个修复的价值在于**让订阅者第一次有能力区分两种断连** —— 今天没有人能区分。它排在后面不是因为难，而是因为 netmgr 已经忍住了它。

---

## 10. `tuya_iot_destroy()` 硬编码三种链路类型 —— 【潜在】

### 涉及文件

- `src/tuya_cloud_service/cloud/tuya_iot.c:798-800`
- `src/tuya_cloud_service/netmgr/include/netconn_registry.h:304` —— `netconn_registry_get_table(&count)`
- `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c:87-91`、`:112-113`、`:127`

### 问题是什么

```c
netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);      // :798
netmgr_conn_set(NETCONN_WIRED, NETCONN_CMD_CLOSE, NULL);     // :799
netmgr_conn_set(NETCONN_CELLULAR, NETCONN_CMD_CLOSE, NULL);  // :800
```

三个字面量。一块通过 `netconn_registry_set_table()`（`netconn_registry.h:328`）加了第四种链路的板子，那条链路会被注册、被排序、被路由、被 CLI 打印，**但不会在这里被关掉**。

还有一个今天就成立的小问题：三行里只有第一行有实际效果。`NETCONN_WIFI_SET_MASK` 含 `NETCONN_CMD_CLOSE`（`netconn_table.c:90`），而 `NETCONN_WIRED_SET_MASK`（`:112-113`，只有 PRI|IP|MAC）和 `NETCONN_CELLULAR_SET_MASK`（`:127`，只有 PRI）都不含。所以 `:799` 和 `:800` 都会在 netmgr 的属性掩码筛查处返回 `OPRT_NOT_SUPPORTED`，而这两个返回值都被丢掉了。

### 怎么观察到

今天观察不到：发货的注册表正好就是这三行（`netconn_table.c:150-197`）。它会在第一块加了第四种链路的板子上发作。

### 建议的修法与影响面

两个选项：

1. **就地泛化**：用 `netconn_registry_get_table(&count)`（`netconn_registry.h:304`）遍历，对每一行调 `netmgr_conn_set(desc->type, NETCONN_CMD_CLOSE, NULL)`，并且检查返回值。
2. **更对的做法**：把这三行删掉，让 `netmgr_deinit()` 去做 —— 它已经会按注册的反序关闭每一条链路（`netmgr_priv.h:88-92`）。

选项 2 更干净，但它把"谁负责拆 netmgr"这个归属问题摆到了桌面上（今天 `tuya_iot` 不调 `netmgr_deinit()`），而且要注意 `netmgr_priv.h:162-163` 的约束：`netmgr_deinit()` 不能从 `WORKQ_SYSTEM` 线程调用。

影响面：`tuya_iot.c` 里一个函数。

---

## 11. `tal_cli` 的 `argv` 在 `argc` 之后不清零 —— 【潜在】

### 涉及文件

- `src/tal_cli/src/tal_cli.c:38-39` —— `CLI_ARGV_NUM` 为 8
- `src/tal_cli/src/tal_cli.c:95` —— `char *argv[CLI_ARGV_NUM];`，在长生命周期的 `cli_t` 里
- `src/tal_cli/src/tal_cli.c:826`、`:830` —— `cli_t` 只分配一次并清零，此后不释放
- `src/tal_cli/src/tal_cli.c:533-536` —— 只填 `argv[0..argc-1]`
- `src/tal_cli/src/tal_cli.c:573` —— 每条命令之后 memset `cli->buffer`
- `src/tuya_cloud_service/netmgr/src/cli/netmgr_cli.c:23-26` —— netmgr 侧的记录

### 问题是什么

`cli_parse_buffer()`（`:483`）末尾的填充循环是 `for (i = 0; i < *argc; i++)`（`:533-536`），只写 `argv[0..argc-1]`。`argv` 位于 `s_cli_handle` 指向的那个唯一的 `cli_t` 里（`:95`），只在 `:826` 分配一次、`:830` 清零一次，之后每条命令都复用。**`argv[argc..7]` 保留的是上一条命令留下的值。**

值得说清楚**那些残留指针指向什么**，因为直觉上的答案是错的：`cli_enter_key()` 在 `:573` 每条命令之后会 memset `cli->buffer`，但**下一条命令的文本又会写进同一个 buffer**，然后才解析。所以一个残留的 `argv[i]` 指向的是**当前**命令行中间的某个位置 —— 当前行的一个后缀，或者一个空字符串。不是栈上垃圾，第一条命令之后也不是 `NULL`。它读起来像一个像样的字符串，这才是麻烦的地方：会崩的错误会被发现，读到空串的错误不会。

### 怎么观察到

只能通过一条不检查 `argc` 就索引 `argv[i]` 的命令观察到。netmgr 自己的命令在 M2 之前就是这么写的，`netmgr_cli.c:23-26` 记录了这件事：

> tal_cli's argv is a persistent array that the tokenizer only fills up to argc, so reading `argv[argc]` reads whatever the previous command left there.

现在 netmgr 侧的每一处都加了保护（`netmgr_cli.c:392`、`:408`、`:470`、`:526`、`:622`、`:870`）。我把树里其余的 CLI 命令也过了一遍 —— `src/tal_cli/src/cli_cmd.c` 和 `src/tuya_cloud_service/authorize/tuya_authorize.c` 都在索引之前检查了 `argc`。**所以今天树里没有活的受害者**，这是一个潜在危险，不是一个正在发作的 bug。

### 建议的修法与影响面

在 `cli_parse_buffer()` 的填充循环之后（`:536` 之后）把 `argv[argc..CLI_ARGV_NUM-1]` 清成 `NULL`。两行，在一处，把整个"读了一个不存在的参数"这一类错误从"读到似是而非的字符串"变成"解引用 NULL 并被立刻发现"。

影响面：任何**树外**今天恰好依赖 `argv[argc]` 非空的命令会开始崩溃 —— 这正是目的，但它确实是一个行为变化，所以应该配一条升级说明。

---

## 12. 模块内唯一还开着的缺口：`NETCONN_CAP_METERED` 声明了，但内置排序不看它

前面 11 节都在 netmgr 之下。这一节在 netmgr **里面**，是重构识别出的四个模块内缺口中唯一还没关的一个，放在这里是因为它和 §8 是同一件事的两面：§8 是"降不下来"，这一节是"不知道该不该降"。

### 涉及文件

- `src/tuya_cloud_service/netmgr/include/netconn_registry.h:192` —— `#define NETCONN_CAP_METERED (1u << 3)`
- `src/tuya_cloud_service/netmgr/include/netconn_registry.h:169-191` —— 它自己的文档，已经诚实地写了"DECLARED AND SURFACED, NOT ACTED ON"
- `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c:175` —— 唯一置这个位的地方（蜂窝行）
- `src/tuya_cloud_service/netmgr/src/cli/netmgr_cli.c:92` —— 渲染成字符串 `"metered"`
- `src/tuya_cloud_service/netmgr/include/netmgr_policy.h:772` —— `netmgr_link_view_t.caps`，交到产品排序钩子手里
- `src/tuya_cloud_service/netmgr/src/policy/netmgr_policy.c` —— **全文 `caps` 出现 0 次**
- `src/tuya_cloud_service/netmgr/src/policy/netmgr_policy.c:135-146` —— `__ranks_above()`
- `src/tuya_cloud_service/netmgr/src/policy/netmgr_policy.c:171` —— `netmgr_policy_select_default()`
- `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c:160`、`:177`、`:190` —— 三个 `default_pri`

### 问题是什么

这个能力位走完了除了"被使用"以外的每一步：

| 环节 | 位置 | 状态 |
| --- | --- | --- |
| 声明 | `netconn_registry.h:192` | 有 |
| 置位 | `netconn_table.c:175`（蜂窝行） | 有 |
| 渲染 | `netmgr_cli.c:92`，`netmgr` 命令会打印 `metered` | 有 |
| 交给产品钩子 | `netmgr_policy.h:772` 的 `netmgr_link_view_t.caps` | 有 |
| **内置排序消费** | `netmgr_policy.c` | **没有** —— 整个文件里 `caps` 出现 0 次 |

也就是说：板子**可以**不改 netmgr 就自己实现这个意图（通过 `netmgr_policy_select_cb_set()` 装一个钩子，钩子看得到 `caps`），CLI 上**看得到**哪条链路是计费的，但**内置排序看不见这个位**。

今天表达"蜂窝是兜底"的是优先级数字：蜂窝 0（`netconn_table.c:177`）、wifi 1（`:190`）、wired 2（`:160`），而 `__ranks_above()`（`netmgr_policy.c:135-146`）优选 `pri` **大**的，相同再按注册序。这是**能工作的**，但它是一个次序，不是一个属性 —— 它不携带任何计费含义。一块板子通过 `netmgr_conn_set(NETCONN_CMD_PRI)` 或自己的注册表重新调优先级，这个意图就**静默丢失**，没有告警也没有日志。

`netconn_registry.h:169-191` 已经把这件事诚实地写下来了，包括记录了它自己早先的一个错误说法（曾经写着"M3 的选路策略用这个位把计费链路保持为兜底"，实际没有）。

### 关掉它需要什么

两半，互相独立，可以分开做。

**第一半：排序。** 在 `netmgr_policy_select_default()`（`netmgr_policy.c:171`）里加一档，位置在优先级比较**之上**、DEGRADED 分层**之下**：一条不计费的可用链路胜过任何计费链路；同类之间原样落到 `__ranks_above()`。具体做法有两种：

- 把 `netmgr_policy.c:231-239` 的两个累加器（clean / suspect）拆成四个（clean/suspect × 不计费/计费）；
- 或者在 `__ranks_above()` 的 `pri` 比较（`:141`）之前加一个计费判断。

三条约束：

1. **必须是一档，不是一个过滤器。** 一台只有蜂窝的设备仍然必须选中它那条计费链路。这和规则 3（DEGRADED 分层）是同一个形状 —— `netmgr_policy.c:253-257` 的注释说明了为什么要"落到下一档"而不是"排除"。
2. **必须由策略字段开关控制**（在 `netmgr_policy_t` 里加一个 `avoid_metered`），出厂关闭，这样它不可能改变任何已发货板子的选路结果。这是探测层用过的同一套纪律，见升级说明 §1。
3. 它和 `default_pri` 不冲突：优先级仍然是同档内的排序依据。

**第二半：流量，这一半更大，而且它不在 netmgr 里。** 在计费链路是活跃链路时，压住那些"可选的、聊天式的"流量。今天的消费方问的问题是"有没有网"（`netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS)`），拿到的是一个两值答案。

- LAN 发现**已经**被另一个位覆盖了 —— `NETCONN_CAP_LAN` 加上 `netmgr.c:512` 的门控，所以一台从蜂窝启动的设备根本不会打开 LAN 端口；
- 但 OTA 轮询、健康上报、天气刷新都没有被覆盖。每一个都得改成问一个新问题。

netmgr 能提供答案（活跃链路的 caps 就在注册表里，`netconn_registry_find(active)->caps`），但**它没法让别人来问**。这是这一半的实质困难，也是为什么它值得单独立项而不是塞进策略层的一次改动里。

### 顺手要修的两处过期文字

这两处都会让后来的读者相信这个功能已经存在：

1. `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c:171-174` 的注释说 "M3 replaces that test with these bits"，`these bits` 指的是这一行的 `.caps`，即 METERED。但 M3 实际是用 `NETCONN_CAP_LAN` 做的门控（蜂窝**没有**那个位才是它被排除的原因，和它的 METERED 位无关）；而同一句里 "still wrapped around the LAN timer in `netmgr_init()`" 提到的那个 `#if !defined(ENABLE_CELLULAR)` 已经被删了（`netmgr.c:458-467` 记录了删除过程）。这句话在机制和现状两方面都过期了。
2. `apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config:13-15` 把 "NETCONN_CAP_METERED avoidance" 列进了这个 config 所验证的内容里。那个行为不存在，这一行是过度宣称。

---

## 附：本文没有收录的候选

为了让下一个人不必重新走一遍，记录一条**看起来对但核不出来**的说法，以及它为什么被排除：

- **"LINUX 的 `tkl_wired_set_status_cb()` 以一个无保护的 `pthread_create()` 结尾，所以每次 `netmgr_init()` 都漏一个轮询线程"** —— 对发货平台**不成立**。`platform/LINUX/tuyaos_adapter/src/tkl_wired.c:129` 把整个函数体放在 `if (cb)` 里，`:157` 用 `if (!wired_event_thread)` 保护创建，所以无论 netmgr 被重新初始化多少次，进程里只有一个轮询线程。这个说法的来源是读了 porting 模板（`tools/porting/template/linux/tkl_wired.c`）而不是适配文件 —— 模板那边确实无保护，那是 §1。**同名文件在不同目录下不是同一个结论**，这是本文所有引用都标注了完整路径的原因。
