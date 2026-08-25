# netmgr 重构升级说明

> 读者：把已有 app 升级到本次 netmgr 重构之后的产品工程师。
> 代码：`src/tuya_cloud_service/netmgr/`；开关：`src/tuya_cloud_service/Kconfig`。
> 一句话：**默认行为与重构前一致**——新增的策略层与探测层出厂即关闭，由产品显式打开。
> 本文只写调用方能观察到的东西；为什么这么做，见各 header 的注释与 commit message。

---

## 1. 默认姿态：策略层与探测层默认关闭

三个新 Kconfig 符号，与 `ENABLE_CELLULAR` 同级，都在 `src/tuya_cloud_service/Kconfig`：

| 符号 | 类型 | 默认 | 说明 |
| --- | --- | --- | --- |
| `ENABLE_NETMGR_PROBE` | bool | `n` | 总开关；同时决定 `netmgr_probe.c` 是否参与编译 |
| `NETMGR_PROBE_DEMOTE` | bool | `y` | 仅在总开关下出现；坏判决是否**真的允许移动路由** |
| `NETMGR_POLICY_MIN_DWELL_MS` | int，0..3600000 | `0` | 仅在总开关下出现；切换后的最短驻留时间（ms） |

两个子开关嵌在总开关**下面**而不是并列：总开关关掉时，「全都关」是平凡地中性的，而不是一个需要有人去推理的组合。

`ENABLE_NETMGR_PROBE=n`（出厂默认）时：

- netmgr 在「驱动报告已关联并拿到地址」的链路里按优先级选路，优先级相同按注册顺序；
- 没有任何东西观察包是否真的送达，因此不会有链路因为不可达被降级，`NETMGR_LINK_STATE_DEGRADED` 根本不可达；
- 所有策略时间量都是 0，netmgr 不会启动它的共享 deadline 定时器；
- `netmgr_probe.c` 不进 image，`app.elf` 里连 probe 的符号都没有。

也就是说：**不改任何配置，升级后的行为就是升级前的行为。** 单链路产品（只有 wifi，或只有 wired）没有打开它的理由——候选只有一个，一条判决改变不了排序结果，剩下的只有它占的 flash。

### 1.1 多链路产品的灰度上线路径

这是实际建议的顺序，也是唯一能在**不改变行为**的前提下拿到现场证据的顺序：

1. **只收数据，不改行为。** `CONFIG_ENABLE_NETMGR_PROBE=y`，同时 `# CONFIG_NETMGR_PROBE_DEMOTE is not set`。判决照常累积、照常在串口 CLI 上打印、`netmgr_probe_stat_get()` 照常能读，但它们不参与候选排序，路由永远不会因为一条判决而移动。
2. **看现场数据。** 串口 CLI 上敲一个不带参数的 `netmgr`（见 §5），或在 app 里调 `netmgr_probe_stat_get()`，确认探测对自己这批设备的判断是对的。
3. **放它动手。** `CONFIG_NETMGR_PROBE_DEMOTE=y`，并且同时设 `CONFIG_NETMGR_POLICY_MIN_DWELL_MS`——多链路产品必读 §6。

---

## 2. 会坏的地方

### 2.1 `NETCONN_CMD_CLOSE` 不再谎报成功

`netmgr_conn_get()` / `netmgr_conn_set()` 现在在进驱动之前先按链路描述符里的属性掩码筛一遍，不支持的命令统一在这一处返回 `OPRT_NOT_SUPPORTED`，而不再是每个驱动文件里的一个 `default:` 分支。受影响的组合：

| 调用 | 上一个版本 | 现在 | 升级可见 |
| --- | --- | --- | --- |
| `netmgr_conn_get(NETCONN_WIRED, NETCONN_CMD_CLOSE, ...)` | `OPRT_OK`（一个什么都不做的空 case） | `OPRT_NOT_SUPPORTED` | **是** |
| `netmgr_conn_set(NETCONN_CELLULAR, NETCONN_CMD_CLOSE, ...)` | `OPRT_NOT_SUPPORTED`（当时根本没有 CLOSE 分支） | `OPRT_NOT_SUPPORTED` | 否 |

只有 wired 那一行是升级可见的变化。cellular 一行列在这里是因为**能力确实不存在**、值得知道，但它跨版本没有变：上一个版本压根没有 CLOSE 分支，走 `default:` 就是 `OPRT_NOT_SUPPORTED`。中途曾有一版返回过 `OPRT_OK`，那是本次开发过程内部的事，已经在同一批改动里去掉了，对升级者不构成变化。

原因是这个能力本来就不存在：`tal_wired.h` 的六个函数里没有一个能拉起或断开链路；`tal_cellular.h` 只有 `tal_cellular_init()` 而没有 deinit，也没有 connect/disconnect 对，驱动无法让承载下来。这不是「netmgr 还没支持」，是 TKL 层没有入口。掩码也因此和描述符里的 `.ctrl` 对齐了：只有 `NETCONN_CTRL_MANAGED` 能兑现一个 close，而只有 wifi 是 MANAGED——wired 是 OBSERVE，cellular 是 SUSTAINED。

**要检查什么**：把 `OPRT_OK` 当成「链路已经断了」的调用方。这种代码本来就是错的，现在会被告知。`tuya_iot_destroy()` 里那三条 close 调用不检查返回值，所以 SDK 内部不受影响。

顺带注意：`netmgr_conn_get(NETCONN_AUTO, ...)` 会先把 `NETCONN_AUTO` 解析成当前活跃链路，再按**那条链路**的掩码筛。所以活跃链路是 wired 时，`netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_CLOSE, ...)` 同样得到 `OPRT_NOT_SUPPORTED`；活跃链路是 cellular 时 `NETCONN_CMD_MAC` 也一样（这一条以前就是驱动里的显式拒绝，没有变化）。

### 2.2 未注册的链路不再返回 `OPRT_OK`

`netmgr_conn_get()` / `netmgr_conn_set()` 走到链表末尾时，以前返回 `OPRT_OK` 而 `*param` 一个字节都没写，调用方于是格式化了未初始化的栈（`tal_cli` 的 ip 命令就在打印这个）。现在返回 `OPRT_NOT_FOUND`。

**要检查什么**：拿到 `OPRT_OK` 之后直接用 out 参数、而请求的链路类型在这个 build 里可能并不存在的代码——例如在无线板上问 `NETCONN_WIRED`。

### 2.3 `EVENT_LINK_STATUS_CHG` 的载荷是指针，不是值

netmgr 一直是 `tal_event_publish(EVENT_LINK_STATUS_CHG, &legacy_status)`，载荷是**指向** `netmgr_status_e` 的指针。本次修掉了两个 example 里把它直接强转成值的写法：

- `examples/multimedia/audio_player/music/src/tuya_app_main.c`
- `examples/protocols/mqtt_client/src/examples_mqtt_client.c`

抄过这个模式的 app 有同一个 bug：强转拿到的是地址，于是所有跟 `NETMGR_LINK_UP` 的比较永远为假，app 从来没有察觉到网络起来过。

**怎么自查**：在自己的代码里搜 `(netmgr_status_e)data`、`(netmgr_type_e)data` 这类形状。订阅 `EVENT_LINK_STATUS_CHG` 的回调必须先解引用：

```c
OPERATE_RET __link_status_cb(void *data)
{
    netmgr_status_e status;

    // netmgr 发布的是 &pub_status，data 是指向值的指针
    if (NULL == data) {
        return OPRT_INVALID_PARM;
    }
    status = *(netmgr_status_e *)data;
    ...
}
```

`EVENT_LINK_TYPE_CHG` 同理，载荷是指向 `netmgr_type_e` 的指针。两者的载荷都指向发布函数里的栈变量，而 `tal_event_publish()` 是在发布者线程上同步派发的，所以指针只在回调期间有效——需要留到以后用，必须自己拷一份。

---

## 3. 行为不变，但机制换了地方

### 3.1 wifi 重连退避

序列**一个字节都没变**：1 / 3 / 5 / 10 / 15 / 20 / 20 …（秒）。实现从 `netconn_wifi.c` 里重复两遍的算术搬进了 `netmgr_retry.c`，唯一定义是 `netmgr_retry_table_assoc`。

- `netconn_wifi.h` 没有改动；`conn.stat` / `conn.count` / `conn.table` / `conn.table_size` / `conn.timer` 都还在原处。
- `NETCONN_CMD_RECONN_TABLE` 照常可用，产品自己下发的表不会被覆盖：`netconn_wifi_open()` 只在 `table_size == 0` 时才去种子化，所以为低功耗场景配的长退避表在一次 `close()` / `open()` 之后也不会被悄悄改回短表。

**一个需要知道的解耦。** 以前静态初始化把 `table_size` 声明成 16 而表里只有 6 项，于是第 6 次之后代码用 `0` 去 arm 定时器，而 `tal_sw_timer_start()` 对 0 的处理是「保留上一次的间隔」；又因为 `conn.timer` 是和连接尝试超时**共用**的，饱和后的退避实际由 `WIFI_CONN_TIMEOUT_MAX`（`netconn_wifi.h`，值 20）决定，和 `table[5]` 相等纯属两个无关常量都是 20。现在每次 arm 都显式传间隔。**对你的影响**：改 `WIFI_CONN_TIMEOUT_MAX` 不再会顺带改动重连退避。如果你曾经靠调这个常量来改退避节奏，现在请改用 `NETCONN_CMD_RECONN_TABLE`。

按 hardening 看待这一项，不要按行为变更看待：整个尝试区间上的时序与旧代码逐项一致。

### 3.2 驱动上报改为串行化处理

驱动仍然调 `base.event_cb()`，但它现在落到一个 shim：写一个静态槽位，再往 `WORKQ_SYSTEM` 投一个**合并**后的 work item。状态机因此只在一个上下文里跑，而不是在 vendor WiFi 任务和 modem 回调里各跑一遍；`tal_net_route_set()` 因此只有一个写者。

**可观察的后果**：`netmgr_conn_set(..., NETCONN_CMD_IP, ...)` 返回时，新路由**还没有**安装——它和链路事件走同一个通道。树内没有调用方设这个命令。

`EVENT_NETMGR_CHG` 除 `NETMGR_CHG_REASON_INIT` / `NETMGR_CHG_REASON_DEINIT` 两种之外，都从 `WORKQ_SYSTEM` 线程发布。链路事件的订阅者因此**不能**调 `netmgr_deinit()`，见 §7。

### 3.3 出向 socket 绑定活跃链路的源地址

以前只有 `tcp_transporter` 会把 socket 绑到 netmgr 选定的地址，其它出向连接走的是协议栈自己挑的接口。现在 `tal_net_connect()` 统一做这件事：

- 只绑**还没被绑过**的 socket（地址和端口都为 0 才算未绑），已经显式 `tal_net_bind()` 过的调用方不受影响——`tcp_transporter` 自己的 `bindAddr` / `bindPort` 仍然优先，pjproject 的 connect 路径都是先 bind 的，全部跳过；
- 目的地是 loopback 时不绑，它需要 loopback 源地址；
- 绑失败只打日志然后继续，「没绑上的 socket」好过「连不上」；
- **Linux host build 不做这件事**：主机有 SDK 一无所知的策略路由，源地址绑定在那里破坏的比修的多。

**覆盖不到的两处，现在明确写在 `tal_network.h` 里**：DNS（lwIP 的 DNS pcb 是 file-static，在 `dns_init()` 里就绑好了，没有 socket 可绑）和无连接 UDP（在 `tal_net_send_to()` 上钉源地址会破坏刻意绑 `TY_IPADDR_ANY` 的广播路径）。两条链路同时 up 时，DNS 查询仍然可能从非活跃接口出去，而随后的连接从正确的接口出去。`tal_net_connect_raw()` 也不做绑定：目的地是一个这一层无法便携解析的平台 sockaddr。

另外，`tal_net_route_set()` 现在把「socket 后端」和「源地址」作为**一对**同时发布，观察者不会再看到半新半旧的组合。

---

## 4. 新增的 app 侧 API

签名以 header 为准，都在 `src/tuya_cloud_service/netmgr/`。

先说一个**不要用**的：`netmgr_reselect_request()` 声明在 `netmgr_priv.h` 里，那个 header 自己写着「不是公开 API，能从外面 include 到只是因为 netmgr 目录在 `LIB_PUBLIC_INC` 上」。它是 `netmgr.c` 和它的卫星文件之间的内部契约，app 不要调；需要让 netmgr 重新选路的正当途径是 `netmgr_policy_set()`（它会自己安排一次）或 `netmgr_policy_pin()`。`netmgr_cmd()` 同理，它是给 app 的 `cli_cmd.c` 注册用的入口，不是给业务代码调的。

### 4.1 策略（`netmgr_policy.h`）

| API | 要点 |
| --- | --- |
| `OPERATE_RET netmgr_policy_get(netmgr_policy_t *policy)` | 读当前生效的策略 |
| `OPERATE_RET netmgr_policy_set(const netmgr_policy_t *policy)` | 任何时候都能调，包括 `netmgr_init()` 之前；不 latch，所以配网结束后加长 dwell、OTA 期间关掉探测都是合法用法。`policy` 被拷贝，但 `policy->revalidate.entry` **不拷贝**，那个数组必须活得比 netmgr 长。传 `NULL` 恢复 `NETMGR_POLICY_DEFAULT_INIT`。改动在下一次 reselect 生效，而 netmgr 会立刻安排一次 |
| `OPERATE_RET netmgr_policy_pin(netmgr_type_e type)` | 手动钉住活跃链路，`NETCONN_AUTO` 释放。见 §5 的语义说明。`OPRT_OK`＝已钉住且现在就能承载；`OPRT_RESOURCE_NOT_READY`＝pin 已记下但链路还不能承载（不是失败）；`OPRT_NOT_FOUND`＝这个 build 里没有这条链路 |
| `OPERATE_RET netmgr_policy_pin_get(netmgr_type_e *type)` | 读 pin，没钉住时给 `NETCONN_AUTO` |
| `OPERATE_RET netmgr_policy_select_cb_set(netmgr_policy_select_cb_t cb, void *ctx)` | 用产品自己的排序替换内置排序（信号强度门限、时段偏好、「充电时不用 cellular」之类）。`NULL` 恢复内置。**hook 在持有 netmgr 状态锁时被调用**：只能对 `in` 做纯运算，不能调 `netmgr_conn_get/set`（非递归 mutex，自死锁）、不能 publish、不能阻塞。它可能需要的一切都已经在 `in` 里 |
| `void netmgr_policy_select_default(const netmgr_select_in_t *in, netmgr_select_out_t *out)` | 内置排序，暴露出来供 hook 只想覆盖一种情形时兜底 |
| `OPERATE_RET netmgr_link_state_get(netmgr_type_e type, netmgr_link_state_e *state)` | 读单条链路的内部状态（`DOWN` / `CONNECTING` / `UNVERIFIED` / `ONLINE` / `DEGRADED` / `BACKOFF`）。`NETCONN_AUTO` 表示活跃链路。**刻意不走** `netmgr_conn_get(NETCONN_CMD_STATUS)`：那条命令的契约是两值的公共 status，必须继续只回答那两个值，所以已有调用方不会收到没见过的值 |

`netmgr_policy_t` 的字段：`up_debounce_ms`、`down_grace_ms`、`min_dwell_ms`、`preempt`、`probe_enable`、`probe_demote`、`probe_reconnect`、`probe_bad_threshold`（默认 3）、`verify_timeout_ms`（默认 120000）、`revalidate`、`emit_up_switch`。默认值统一在 `NETMGR_POLICY_DEFAULT_INIT` 里，其中 `min_dwell_ms` / `probe_enable` / `probe_demote` 三项由上面那三个 Kconfig 符号推导。

### 4.2 探测（`netmgr_probe.h`，随 `ENABLE_NETMGR_PROBE` 编译）

| API | 要点 |
| --- | --- |
| `OPERATE_RET netmgr_probe_stat_get(netmgr_type_e type, netmgr_probe_stat_t *stat)` | 快照一条链路的判决累加器：`last` / `source` / `bad_count` / `good_total` / `bad_total`。`OPRT_NOT_FOUND`＝该链路未注册。**灰度第 2 步就是靠它**（连同不带参数的 `netmgr`）拿现场数据的 |
| `OPERATE_RET netmgr_probe_report(const netmgr_probe_result_t *result)` | 自己有别的探测手段时上报一条判决。任何上下文都可以调；它只记账并投同一个 work item，不做评估、不 reselect、不发布、不动路由。上报者永远不指定链路——它只知道包有没有到，不知道包从哪条链路走的 |
| `OPERATE_RET netmgr_probe_report_simple(netmgr_probe_verdict_e verdict, netmgr_probe_source_e source)` | 同上的便捷形式 |
| `uint32_t netmgr_probe_epoch_get(void)` | 路由代次。能跨自己那次网络操作保存状态的上报者，操作前读一次、上报时带回来，netmgr 会丢弃过期判决；做不到的传 `NETMGR_PROBE_EPOCH_ANY` |
| `OPERATE_RET netmgr_probe_backend_set(const netmgr_probe_backend_t *backend)` | 换掉内置 backend。**必须在 `netmgr_init()` 之前**调，之后返回 `OPRT_COM_ERROR` 而不是静默忽略。传 `NULL` 是「完全不探测」，但它**不能**让内置 backend 不被链接——那只有 `ENABLE_NETMGR_PROBE=n` 能做到 |

内置 backend 不发任何包：它把设备本来就在发布的 `EVENT_MQTT_CONNECTED` / `EVENT_MQTT_DISCONNECTED` 翻译成 good / bad 判决，所以在计费的 cellular 承载上也是可接受的。这也是它「打开是安全的」的全部理由。

### 4.3 链路注册表（`netconn_registry.h`，板级）

| API | 要点 |
| --- | --- |
| `OPERATE_RET netconn_registry_set_table(const netconn_desc_t *table, uint32_t count)` | 用板子自己的表整体替换默认链路表：名字、能力位、控制级别（`OBSERVE` / `SUSTAINED` / `MANAGED`）、默认优先级、socket 后端、以及 set/get 各自接受哪些 `NETCONN_CMD_*`。在 `board_register_hardware()` 里调（每个 app 都在 `netmgr_init()` 之前跑它）；`netmgr_init()` 之后调返回 `OPRT_COM_ERROR`。表由板子自己 `static const` 持有，**不拷贝**，必须活得比 netmgr 长 |
| `const netconn_desc_t *netconn_registry_get_table(uint32_t *count)` | 取当前生效的表（板级覆盖优先，否则默认表） |
| `const netconn_desc_t *netconn_registry_find(netmgr_type_e type)` | 单条链路的描述符；`NETCONN_AUTO` 不是链路，返回 `NULL` |

有了它，板子重调优先级和 socket 后端不再需要改驱动：注册时会把 `default_pri` 和 `provider` 拷进 conn 节点，这两个字段现在是缓存。

关于 `NETCONN_CAP_METERED`：树内的表在 cellular 那一行带这个能力位，不带参数的 `netmgr` 会渲染它，它也会通过 `netmgr_link_view_t.caps` 交到产品排序 hook 手上。但**内置排序不读任何能力位**——树内那张表是用 `default_pri` 表达「cellular 是兜底」的：cellular 0、wifi 1、wired 2。想让计费链路参与排序决策，得自己写 hook。

### 4.4 新事件 `EVENT_NETMGR_CHG`

事件名 `"netmgr.chg"`，定义在 `src/tal_system/include/tal_event_info.h`，载荷是 `netmgr_change_t *`（`netmgr_event.h`）。它带上了两个旧事件表达不出来的东西：

- `reason`——`netmgr_change_reason_e`，17 个取值，从 `INIT` / `LINK_UP` / `LINK_DOWN` 到 `PROBE_GOOD` / `PROBE_BAD` / `PROBE_TIMEOUT` / `REVALIDATE` / `PRI_CHANGED` / `PINNED` / `UNPINNED` / `DEBOUNCE` / `GRACE` / `DWELL` / `POLICY` / `ADDR_CHANGED` / `DEINIT`。日志和云端诊断因此可以指名道姓，不用猜；
- `subject`——这次变化**是关于哪条链路**的，不一定是新的活跃链路；
- `old_active` / `new_active` / `old_status` / `new_status`——后两个和 `EVENT_LINK_STATUS_CHG` 是同样的两个值，所以搬过来不需要改解释方式；
- `new_state`——`new_active` 的内部状态。这是公共 status 表达不出来的那一层：`new_status` 两边都是 `NETMGR_LINK_UP` 时，靠它区分 `ONLINE` 和 `DEGRADED`；
- `epoch` / `src_ip`——刚刚装上的路由代次和源地址，也就是订阅者下一个出向 socket 会绑的地址；
- `handover`——这次是不是一次切换（两条链路都是 up、不同、中间没有 down）。**它是无条件上报的**，所以识别切换不需要打开 `emit_up_switch`，这也是推荐的消费方式。

`emit_up_switch` 默认 `FALSE`，而且这个默认值是有原因的：`NETMGR_LINK_UP_SWITH` 会让「把 `net_status == NETMGR_LINK_UP` 直接算成 connected」的消费者在每次切换时显示「wifi 已断开」——`your_chat_bot` 的 `app_ui_helper.c` 就是这么写的。

载荷生命周期同 §2.3：只在回调期间有效，要留就拷。订阅者**不要**在回调里调 `netmgr_deinit()`。

---

## 5. 新增的 CLI 命令

`netmgr_cmd()` 的用法：

```
netmgr                            dump links, policy and probe stats
netmgr wifi up <ssid> [password]  join an AP, no password means open
netmgr wifi down                  leave the current AP
netmgr wifi scan                  list nearby APs
netmgr wired [up|down]            not supported, wired is observe-only
netmgr switch <name|auto>         pin the active link, auto releases
netmgr deinit                     tear netmgr down
netmgr init                       bring it back up
```

**不带参数的 dump 现在遍历注册表**，每条已注册链路一行，通过它的描述符打印：name、能力位、控制级别、优先级、驱动 status、netmgr 的 link state、provider；有判决的链路多打一行探测计数器（没有判决的不打，否则一屏零会把唯一有内容的那条埋掉）。策略也一并打印，包括 revalidation 表——按 netmgr 实际生效的方式渲染，而不是结构体字面内容（`{NULL, 0}` 的意思是「未设置，用内置表」，照字面打成空表会说成相反的意思）。

两列 status 会合理地互相矛盾，这正是这份 dump 的意义：`status` 是驱动的两值视图，`state` 是 netmgr 自己的状态机。`link_up degraded` 读作：链路有地址、在跑 LAN 流量，而 netmgr 有证据表明经它到不了云端。

**cellular 以前虽然已注册、已可选路，但在 console 上完全看不见**，wifi+4G 板子根本看不到自己的第二条链路。现在能看到了。新增链路类型只需要新增一行，不需要改 CLI。

`netmgr switch <name>` 走 `netmgr_policy_pin()`，名字来自注册表，所以 usage 行列出的就是这个 build 真实有的链路。语义：

- pin 高于优先级、状态分层、粘滞和 dwell——操作者比这些规则掌握更多上下文，包括「用一条 netmgr 认为已降级的链路」的权利；
- pin **不越过可用性下限**：钉一条 down 的链路不会让流量从它走，pin 会被**记住**，等它起来时才生效，返回值区分这两种情况；
- pin **不会去拨号**。`netmgr switch wifi` 在 wifi down 时只是把 pin 记下来，请用 `netmgr wifi up <ssid>`；
- pin 能活过链路事件、reselect 和策略变更，只有 `netmgr switch auto`（即 `netmgr_policy_pin(NETCONN_AUTO)`）和 `netmgr_deinit()` 能清掉它。

这条命令让策略层可以在硬件上被测试而不用改 app：钉一条链路，看路由移动，看判决进来，解钉，看排序接手。

`netmgr init` / `netmgr deinit` 在就绪检查**之前**分派，而且必须如此——`netmgr init` 的存在意义就是在 netmgr 已经下去的时候跑，放在就绪检查后面它就永远不可达，`netmgr deinit` 之后唯一能救回来的命令会拒绝执行。`netmgr init` 会用上一次配置的 type mask，而不是从 build 去猜。

反复敲这两条命令的人会最先撞上两个告警：

- 没有任何 TAL 入口能撤回驱动装上的回调，而 LINUX 的 `tkl_wired_set_status_cb()` 结尾是无条件 `pthread_create()` 一个 detached 轮询线程，所以第一次之后每次 init 都会多留一个线程在轮询同一条 wired 链路；
- `netmgr_init()` 在 CLI 线程的栈上跑所有 `conn->open()`，而那个栈默认只有 3072 字节，T5AI 上 `ENABLE_BLUETOOTH` 的 build 放不下。详见 §7。

**命令名。** 几个 app（`switch_demo`、`weather_get_demo`、`tuya_t5_pixel_weather`）在自己的 `cli_cmd.c` 里把 `netmgr_cmd` 注册成 `netmgr`；`src/tal_cli/src/cli_cmd.c` 另外注册了一个 `sys_netmgr` 转发到同一个函数。两者都能用上面的新子命令，app 不需要改。注意 `sys_netmgr help` 打印的是 `tal_cli` 自己那段旧文案（还写着 `switch ... (TBD)`）；真实用法请看不带参数或参数不识别时 `netmgr_cmd()` 打出的那一份。CLI 的输出走 log 口。

---

## 6. 多链路产品必读：`min_dwell_ms` 与它压的那个振荡

打开探测并允许降级之后，一块 wifi+4G 板子在 wifi 的 AP 掉了 WAN 时会振荡。这**是被动探测本身固有的，不是缺陷**：

1. wifi 被降级为 `DEGRADED`（`verify_timeout_ms` 走完仍然没有任何判决），路由移到 cellular。这是对的，也正是探测存在的意义；
2. 30 s 后 revalidation 把 wifi 提回 `NETMGR_LINK_STATE_UNVERIFIED`。它必须这么做：被动探测只能判断**活跃**链路，想知道 wifi 是否恢复，唯一办法就是再用它一次；
3. `UNVERIFIED` 和 `ONLINE` 在同一档（把它们分开会死锁——高优先级链路会去等一个只有被选中才能拿到的判决），于是 wifi 更高的优先级胜出，路由移回去；
4. `verify_timeout_ms` 再次走完，回到第 1 步。

每一摆会拆掉并重建 MQTT 会话**两次**，因为链路类型变化会让 `tuya_iot` reconnect。revalidation 退避表（30 / 60 / 120 / 300 / 600 s，再加上 `verify_timeout_ms`）会把周期压下来，最后稳定在**大约每十二分钟一个循环**并停在那里。有界，但在日志和云端会话计数里看得见。

`min_dwell_ms` 压的是这个序列早期那段快的部分：它**推迟**一次切换直到 dwell 走完，但从不**阻止**切换；而且活跃链路一旦不再可用，dwell 立刻被放弃，所以它不会把设备困在一条死链路上。有用区间 30000–120000 ms；远低于两分钟的 `verify_timeout_ms` 就没什么可压的了。默认 0 是 netmgr 历史上的时序——排序一变就立刻切。

想完全不再复验的板子，可以把 `netmgr_policy_t.revalidate` 设成 `entry` 非 `NULL` 而 `count` 为 0，代价是永远发现不了 wifi 恢复。

`netmgr_policy.h` 里 `min_dwell_ms` 的注释是这段的规范文本，`NETMGR_POLICY_MIN_DWELL_MS` 的 Kconfig help 是同一段的重复；两者若有分歧，以 header 为准。

---

## 7. `netmgr_init()` / `netmgr_deinit()` 的契约

### 7.1 `netmgr_deinit()` 是新的公开 API

声明在 `netmgr.h` 里 `netmgr_init()` 旁边。它关闭并注销每条链路，停掉 notify work item 和共享 deadline 定时器，停掉探测 backend，释放手动 pin，并把静态 conn 节点恢复成 `netmgr_init()` 见到的样子，以便后续 init 复用。**幂等**，在 `netmgr_init()` 从未跑过或半途失败之后调用也是安全的。

返回 `OPRT_OK`（包括无事可做）；`OPRT_TIMEOUT` 表示一个正在跑的 notify handler 没能在 drain 超时内结束——能安全拆的东西还是都拆了。

**调用方义务 1：不要从 `WORKQ_SYSTEM` 线程调它。** 它要等 notify handler 结束，会等到自己身上。这条规则同时排除了从 `EVENT_LINK_*` 订阅者里调它。

### 7.2 `netmgr_init()` 现在是幂等的

对一个已经初始化的 netmgr 调用它，打一条 warning 并返回 `OPRT_OK`，什么都不动。**要换 type mask，先 `netmgr_deinit()`**——第二次 `netmgr_init()` 不能把 mask 变宽，而悄悄半应用一个 mask 比拒绝更糟。

返回值：`OPRT_OK`（含已初始化的情形）；`OPRT_INVALID_PARM` 表示一条链路都没能注册；`OPRT_NOT_SUPPORTED` 表示这个 build 里没有任何链路驱动。任何错误路径上它都会通过 `netmgr_deinit()` 自己回滚。

### 7.3 调用方义务 2：栈要够大，而且这一条没有任何检查

`netmgr_init()` 在**调用者的栈上**跑完整个注册流程，包括每条链路的 `conn->open()`。那会一路进到 `tal_wifi_init()` 和 vendor 的 WiFi bring-up、netcfg 初始化、`tal_cellular_init()` 及其 modem AT 交互，以及 `ENABLE_BLUETOOTH` build 上的 `tuya_ble_init()` 和整个 BLE 栈构造。这些函数平时的调用者是 app 的主任务，栈很大，它们自己都不是按省栈写的。

**所以：从 app 的启动任务里调，不要从小 worker 线程里调。** 已知的反例，因为一个限制值得被指名：串口 CLI 线程的栈是 `SERIAL_CLI_STACK_SIZE`，默认 3072 字节（`src/tal_cli/Kconfig`）——`netmgr_deinit()` 放得下，T5AI 上 `ENABLE_BLUETOOTH` 的 `netmgr_init()` 放不下。

**没有任何运行时检查。** 一个调用方可以满足所有**写下来的**规则——不在 `WORKQ_SYSTEM` 线程上、netmgr 也没有已经起来——然后依然把栈冲掉。`netmgr.h` 的这段注释和 `netmgr init` 打出的那条 warning 是唯一的防线：拒绝执行需要一个栈余量检测，而那个检测自己就跑在它要保护的那个栈上。`netmgr init` 因此保留并如实告知，而不是假装能拦。

---

## 8. 验证

两个可复现的 config target，合起来覆盖两种 socket 后端：

| config | 平台 | socket 后端 | 链路 |
| --- | --- | --- | --- |
| `apps/tuya_cloud/switch_demo/config/Ubuntu.config` | LINUX / host | posix，`TAL_NET_PROVIDER_DEFAULT` → `TAL_NET_PROVIDER_POSIX` | 仅 wired |
| `apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config` | T5AI / `TUYA_T5AI_BOARD` | tkl，`TAL_NET_PROVIDER_DEFAULT` → `TAL_NET_PROVIDER_TKL` | wifi + cellular + bluetooth + ULP |

```
cd apps/tuya_cloud/switch_demo
tos.py config choice -c Ubuntu.config
tos.py build
```

`Ubuntu.config` 是唯一同时选中 posix 后端和 wired 链路的 config，也就是 `netconn_wired.c` 被注册、以及 `netmgr_cli.c` 的 wifi-absent 分支唯一被走到的地方。但它到不了另外一半：LINUX build 上 `netconn_wifi.c` 和 `netconn_cellular.c` 根本不参与编译，`tal_wifi_ulp` 不参与编译，netcfg 不链接，而且只有一条链路时策略层是 no-op——候选排序、`DEGRADED` 降级、`min_dwell_ms` 一行都不会执行。

`TUYA_T5AI_BOARD_CELLULAR.config` 补上那一半：tkl 后端、两条链路同时注册（策略层唯一真的有活干的形状）、`NETCONN_CTRL_SUSTAINED`（只有 cellular 用）、`netmgr_cli.c` 的 wifi 与 cellular 分支，以及 `ulp_apiq.c`（本次 M0 重命名过、而矩阵里没有别的 target 会编的文件）。它用 `CONFIG_ENABLE_T5AI_BOARD_CELLULAR_USB=y` 来选 `ENABLE_CELLULAR`——直接写 `CONFIG_ENABLE_CELLULAR` 也能编，但会漏掉 `tuya_t5ai_board.c` 里的 4G 上电胶水。

两个文件里各有一处「刻意没写」，抄这两个 config 之前值得知道：

- `Ubuntu.config` 里 bluetooth 是关的，不是偏好问题——`tuya_iot.c` 在 `ENABLE_BLUETOOTH` 下调 `netcfg_stop()`，而 netcfg 的源文件和 include 目录只在 `ENABLE_WIFI` 下才被加进 build，所以 bluetooth-without-wifi 今天编不过；
- `TUYA_T5AI_BOARD_CELLULAR.config` 里没有 `ENABLE_WIFI` 和 `ENABLE_BLUETOOTH`——T5AI 上这两个是 `boards/T5AI/TKL_Kconfig` 里没有 prompt、默认 `y` 的 bool，config 文件动不了它们，写上去反而像是做了什么。

---

## 9. 一页纸清单

升级一个已有 app 要做的事，按顺序：

1. **什么都不改**，先编过、跑一遍。默认配置下行为与升级前一致——这是本次重构的设计目标，不是巧合。
2. 搜自己代码里的 `(netmgr_status_e)data` / `(netmgr_type_e)data`，改成先判 `NULL` 再解引用（§2.3）。这是唯一一类「一直是坏的、但你可能一直没发现」的问题。
3. 搜 `NETCONN_CMD_CLOSE`，确认没有把 `OPRT_OK` 当成「链路已断」（§2.1）。
4. 搜 `netmgr_conn_get` / `netmgr_conn_set`，确认没有拿到 `OPRT_OK` 就用 out 参数、而链路类型在本 build 里可能不存在（§2.2）。
5. 如果曾经调 `WIFI_CONN_TIMEOUT_MAX` 来改重连节奏，改用 `NETCONN_CMD_RECONN_TABLE`（§3.1）。
6. **单链路产品到这里就结束了**，`ENABLE_NETMGR_PROBE` 保持 `n`。
7. 多链路产品（wifi+4G、wifi+wired）：按 §1.1 走三步灰度，第三步同时设 `NETMGR_POLICY_MIN_DWELL_MS`（§6）。
8. 想换排序规则或链路表的，看 §4.1 的 `netmgr_policy_select_cb_set` 和 §4.3 的 `netconn_registry_set_table`；想拿更细的变更通知的，看 §4.4 的 `EVENT_NETMGR_CHG`。
