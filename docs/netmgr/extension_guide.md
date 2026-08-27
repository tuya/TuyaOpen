# netmgr 扩展指南

> 状态：随 netmgr M1–M4 重构落地，对应 `yj/feat-4G` 分支
> 目标读者：要给 netmgr **加东西**的人 —— 加一种链路技术、加一个 socket 后端、换掉选路排序、换掉可达性探测
> 覆盖代码：`src/tuya_cloud_service/netmgr/`（控制面）、`src/tal_network/`（数据面）
> **引用约定**：本文每条结论都对着代码核过。引用尽量写作 `文件 · 符号` 而不是 `文件:行` —— 这几个文件正在演进，行号会漂，符号名不会。少数逐行引用的位置，是核对过当时行号确实对得上的。

---

## 1. 架构：控制面与数据面

netmgr 这次重构只有一个目的：**让"加一种链路"变成加文件而不是改文件**。要理解为什么四条扩展路径长成现在这样，先看这两个平面的分工。

```
                 应用 / tuya_iot / tuya_lan
                            │
        netmgr_conn_get/set │ netmgr_init(type)
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 控制面  src/tuya_cloud_service/netmgr/                       │
│                                                             │
│  netconn_table.c ── 注册表：本次构建有哪些链路（唯一允许      │
│      │              #ifdef ENABLE_<TECH> 的文件）            │
│      ▼                                                      │
│  netmgr.c ── 注册 / 每链路状态机 / 排序 / 事件 / LAN 门控     │
│      │        （不认识任何具体技术）                          │
│      ├── netmgr_policy.c   给一组快照，选出该激活哪条链路      │
│      ├── netmgr_probe.c    被动可达性后端（MQTT 生命周期）     │
│      ├── netmgr_retry.c    连接退避算术                       │
│      └── netmgr_cli.c      `netmgr` 命令，泛化于注册表         │
│                                                             │
│  netconn_wifi.c / netconn_wired.c / netconn_cellular.c       │
│      驱动：只对接各自的 TAL，向上报状态                        │
└──────────────────────────┬──────────────────────────────────┘
                           │ tal_net_route_set()  ← 唯一一条向下的边
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 数据面  src/tal_network/                                     │
│  tal_net_provider.c  route =（socket 后端, 源地址）        │
│  tal_posix.c / tal_tkl.c  两个 socket 后端实现            │
│  tal_network.c  所有 socket 原语，经 tal_net_provider_ops│
└─────────────────────────────────────────────────────────────┘
```

| | 控制面 | 数据面 |
|---|---|---|
| 位置 | `src/tuya_cloud_service/netmgr/` | `src/tal_network/` |
| 决定 | **哪条链路**该承载流量 | 用**哪个 socket 后端**、绑**哪个源地址** |
| 输入 | 驱动状态、优先级、探测判决、策略参数 | 只有一个 `tal_net_route_t` |
| 状态 | `s_netmgr`，一把 `s_netmgr.lock` | `s_provider_registry`（file-private），一把 `s_route_lock` |

### 1.1 两条铁律

**铁律一：数据面从不回调控制面。**
`src/tal_network/` 全目录不 include 任何 netmgr 头文件 —— `netmgr` 这个词只出现在注释里（`tal_net_route.h`、`tal_net_provider.c`、`tal_network.h`、`tal_network.c`）。数据面拿到 route 就照做，不问是谁写的、为什么写。

这条铁律同时约束控制面：`netconn_registry.h · netconn_desc_t.provider` 之所以声明成 `uint8_t` 而不是 `tal_net_provider_id_t`，就是为了让这个控制面公共头**不需要**引入数据面的头 —— `netmgr_conn_base_t.provider` 早就遵守同一条纪律。驱动里要用 `TAL_NET_PROVIDER_DEFAULT`，得自己 include `tal_net_provider.h`（三个 in-tree 驱动都写了这句注释：`netmgr.h` 以前会捎带进来，现在不会了）。

**铁律二：控制面只通过一个函数写数据面。**
全树 `tal_net_route_set()` 只有一个调用点，在 `netmgr.c · __netmgr_push_route()` 里面。而 `__netmgr_push_route()` 只被 notify handler 和 `netmgr_init()` 的第一趟 pass 调用，两者都跑在同一个上下文序列上。任何能移动路由的事（链路事件、`NETCONN_CMD_PRI` 改优先级、`NETCONN_CMD_IP` 改地址）都走 `netmgr_notify_link()` 汇到这里，所以不存在"两个并发写者争谁最后落地"的问题。

> 加扩展时最容易犯的错，是想在自己的新代码里"顺手"改一下路由。以前有两个半更新入口（`tal_network_card_set_active()` / `_set_active_ip()`）能做到这件事，各自只碰路由的一半 —— 它们已经删除。今天路由**只能整体发布**，走 `tal_net_route_set()`，这正是为了让"后端和源地址各说各话"这个状态无法被表达出来。

### 1.2 一条报告通道

驱动向 netmgr 报状态，只有一条通道，而且是**异步**的：`netmgr_notify_link(type, status)`（`netconn_registry.h`）。它只做两件事 —— 在报告槽里打标记、往 `WORKQ_SYSTEM` 投一个 work item，然后返回。状态机因此永远只在 `WORKQ_SYSTEM` 这一个上下文里跑，天然与自己串行化，也就不可能从驱动里重入 netmgr 而自锁在 `s_netmgr.lock` 上。

它跑在 `WORKQ_SYSTEM` 而不是 `WORKQ_HIGHTPRI`，是因为状态机**需要能阻塞** —— 推路由要读 `conn->get(NETCONN_CMD_IP)`，在 cellular 上那是一次阻塞的 AT 交互，而高优队列的文档写明"不允许阻塞操作"。也因为 `WORKQ_SYSTEM` 本来就存在（每个 app 在 `netmgr_init()` 之前都调过幂等的 `tal_workq_init()`），所以这条通道**不新建线程、不多花栈**。

几个必须知道的性质：

- **报告会合并**。同一时刻最多排一个 work item，handler 跑之前来的第二次报告被吸收进同一趟。这是无损的 —— handler 压根不信报上来的值，它开头就 `(void)status`，然后逐条链路重新 `conn->get(NETCONN_CMD_STATUS)`。代价：一条 down-then-up 快过 handler 的链路，只会被观察到它稳定后的状态，订阅者看不到那个瞬态。
- **`status` 是咨询性的**，只进日志、只让 trace 好读；选路永远从驱动重新算。
- **`netmgr_init()` 里面发出的报告不会被丢**。LINUX 的 `tal_wired_set_status_cb()` 在返回前就回调，也就是从 `netmgr_init()` 内部报上来，而正是这第一条报告把有线链路的地址放进 route。丢掉它会让 init 自己那趟 route 播下 `src_ip = 0`。
- 只有三种情况才真丢：`netmgr_init()` 还没把状态种下、`netmgr_deinit()` 已经开始、或者这条链路没注册。
- 返回非 OK 只意味着 work 没投进去；pending 标记留着，下一次报告会重试。驱动侧记日志然后继续 —— 这里从来没有、也不需要驱动侧的恢复动作。

---

### 1.3 目录：每个子目录装的是一个"可替换件"

```
src/tuya_cloud_service/netmgr/
├── include/              公开契约。唯一被导出的目录
└── src/
    ├── conn/             扩展路径一：链路驱动 + 内置链路表
    ├── policy/           扩展路径三：默认排序，以及它用的退避表
    ├── probe/            扩展路径四：内置探测后端
    ├── cli/              调试命令，不是扩展点
    ├── netmgr.c          状态机本体
    └── netmgr_priv.h     库内共享声明，不导出
```

划分依据是**"我来这个目录是要换掉什么"**，不是文件类型。§2～§5 四条扩展路径里有三条各自对应一个子目录，装的都是那条路径要替换的那一件东西：`policy/netmgr_policy.c` 是 `netmgr_policy_select_cb_set()` 要替换的默认实现，`probe/netmgr_probe.c` 是 `netmgr_probe_backend_set()` 要替换的内置后端，`conn/` 是新增一种链路技术要照抄的模板。

**`netmgr.c` 在根目录，而且不会被拆开。** 它 3000 多行不是失控：`s_netmgr`、`s_netmgr.lock` 和 `sg_netmgr_gate_closed` 构成一条不变量（见 §3.1 的锁契约），而"锁只保护 `s_netmgr` 的字段访问，此外什么都不保护"这句话，**只有在所有访问都在同一个编译单元里时才是可审计的**。把它拆到几个文件里，就必须把这三样东西放进一个私有头暴露给多个 TU，那恰好毁掉这条契约唯一的执行机制。它不是可替换件，它是消费所有替换件的那个中心 —— 所以它不进子目录。

外层只有 `include/` 和 `src/`，这是这棵树在 component 层的既有写法（`src/tal_network/`、`src/tal_cellular/`、`src/tal_cli/`、`src/tal_system/`、`src/tal_wifi/` 都是这个形状），所以读者不需要学一套 netmgr 专用的布局。它还顺带收紧了一处：私有 include 路径现在指向 `netmgr/src` 而不是模块根，而模块根是**包含 `include/`** 的 —— 指向根意味着源文件可以写 `#include "include/netmgr.h"` 并且能编过，那是个不该存在的拼法。`netmgr/src` 里除了实现什么都没有。

**`include/` 故意不镜像 `src/` 下面的子目录。** `netmgr_policy.h` 在 `include/`、实现在 `src/policy/`，看起来像该"修"的错配，但它不是：整个模块只导出一个目录，这是 `netmgr_priv.h` 保持私有的机制（见 `src/tuya_cloud_service/CMakeLists.txt` 里 `LIB_PUBLIC_INC` 那段注释）。把头文件也按子目录切开，就得导出四个目录，那条保证随即消失。

---

## 2. 扩展路径一：加一种链路技术

这是主线场景。**结论：五处改动，`netmgr.c` 一行不动。**

### 2.1 改动清单

| # | 文件 | 改动 |
|---|------|------|
| 1 | `src/tuya_cloud_service/netmgr/netconn_<tech>.c` `.h` | 新驱动。一个 `netmgr_conn_<tech>_t s_netmgr_<tech>`，填好 `open/close/set/get` |
| 2 | `src/tuya_cloud_service/netmgr/src/conn/netconn_table.c` | 一个 `#if defined(ENABLE_<TECH>)` 块（include + `extern` + `HAS_` 宏）、两个 mask 宏、**表里一行** |
| 3 | `src/tuya_cloud_service/CMakeLists.txt` | 一个 `if(CONFIG_ENABLE_<TECH> STREQUAL "y")` 块，照文件里 wifi / wired / cellular 三段的样子写 |
| 4 | Kconfig | 一个 `ENABLE_<TECH>` 条目，见 §2.9 |
| 5 | `src/tuya_cloud_service/netmgr/include/netmgr.h` | `netmgr_type_e` 加一个枚举位（`1 << 4`）。**不需要**补 `NETMGR_TYPE_TO_STR()` —— 已注册链路的名字全部来自描述符，见 §8.1 |

**不需要改**：`netmgr.c`、`netmgr_policy.c/.h`、`netmgr_probe.c/.h`、`netmgr_retry.c/.h`、`netmgr_cli.c`、`netmgr_priv.h`。

**组件外还有一处，别忘**：应用传给 `netmgr_init()` 的 type 掩码要 OR 上新的位。每个 app 都是这个写法（`apps/tuya_cloud/switch_demo/src/tuya_main.c:280-290`）：

```c
netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
...
netmgr_init(type);
```

`netmgr_init()` 的注册循环第一句是 `if (!(type & table[i].type)) continue;` —— 表里有行、掩码里没位，这条链路就被静静跳过。这是新增链路"看起来没生效"最常见的原因，而且没有任何日志。

### 2.2 "netmgr.c 一行不动"这句话，验证过程

不要相信我，也不要相信头注释。逐条核过的事实：

1. **`netmgr.c` 里没有任何 `ENABLE_<TECH>` 守卫。** 现存的条件编译是 `ENABLE_BLUETOOTH`（三处，BLE 栈归属问题）和 `ENABLE_NETMGR_PROBE`（一处，守着对 `netmgr_probe_backend_mqtt` 的唯一引用）—— 都与链路技术无关。
2. **`wifi` / `wired` / `cellular` 这些词在 `netmgr.c` 里出现几十次，全部在注释里**（`/*` 或 `//`）—— 没有一处是代码。
3. **链路名不再来自枚举翻译宏，而来自描述符**：`netmgr.c · __netmgr_link_name()` 走 `netconn_registry_find(type)->name`，找不到就返回 `"auto"` 或 `"unregistered"`。
4. **`netmgr.c` 的 include 列表里没有任何 `netconn_<tech>.h`**；它只 include `netconn_registry.h`。
5. **注册循环遍历表**：`netconn_registry_get_table()` → 逐行 `__netmgr_conn_register(&table[i])`（`netmgr.c · netmgr_init`）。

同理，`netmgr_cli.c` 的 dump 路径也不需要 `#ifdef` —— 它经 `netmgr_link_info_at(index, &info)` 按注册序迭代（`netmgr_priv.h · netmgr_link_info_t`），拿到的是描述符元数据 + 实时状态。这顺带修掉了旧 dump 的一个缺陷：它手写了一个 wifi 块和一个 wired 块，于是**从来不打印 cellular**。

### 2.3 描述符契约：`netconn_desc_t`

`netconn_registry.h · netconn_desc_t`。实例是 `const`，netmgr 只读；可变的每链路状态仍在 `conn` 指向的 `netmgr_conn_base_t` 里。

| 字段 | 类型 | 契约 |
|------|------|------|
| `name` | `const char *` | 小写短名，日志和 CLI 都用它。不能为 NULL |
| `type` | `netmgr_type_e` | 单个位；表内必须唯一；不能是 `NETCONN_AUTO` |
| `caps` | `netconn_caps_t` | `NETCONN_CAP_*` 的 OR。**编译期常量**，是关于"这项技术在这次构建里能做什么"的陈述，不是运行时状态 |
| `ctrl` | `netconn_ctrl_level_e` | netmgr 被允许驱动这条链路生命周期的程度。见 §2.4 |
| `default_pri` | `uint8_t` | 注册时**覆盖** `conn->pri`。数值大者优先 |
| `provider` | `uint8_t` | 注册时**覆盖** `conn->provider`。填 `TAL_NET_PROVIDER_DEFAULT`，除非板子真有第二个 socket 后端（见 §3） |
| `set_mask` | `netconn_attr_mask_t` | `conn->set()` 接受哪些命令。见 §2.5 |
| `get_mask` | `netconn_attr_mask_t` | `conn->get()` 接受哪些命令。见 §2.5 |
| `conn` | `netmgr_conn_base_t *` | 指向驱动自己那个静态实例的 `base` 成员。不能为 NULL，永不释放，所以 netmgr 可以跨 unlock 持有它 |

**所有权规则**：注册时 netmgr 做三次覆盖（`netmgr.c · __netmgr_conn_register`）：

```c
conn->pri       = desc->default_pri;
conn->provider  = desc->provider;
conn->event_cb  = __netmgr_event_shim;
```

也就是说**描述符是真相源**，`base.pri` 和 `base.provider` 退化成它的缓存。这正是"板子能重排优先级、换 provider 而不用改任何驱动"的机制所在 —— 驱动静态初始化里写的 `.pri = 2` 会被表里的 `.default_pri` 无声盖掉，别在驱动里调它。

描述符**故意不动** `netmgr_conn_base_t`，也不动 `netmgr_conn_{wifi,wired,cellular}_t`：`netmgr.h` 被树里 40 多个文件 include，`netconn_*.h` 在全局公共 include 路径上而且 app 代码在用它们的类型，所以把 `pri`/`status`/`next`/`event_cb` 从 base 里搬出来是另一件更宽的事。描述符**并列**在既有结构体旁边，只携带 netmgr 需要的元数据；base 结构体逐字节不变。

### 2.4 `ctrl` 等级：三档各自要求驱动实现什么

`netconn_registry.h · netconn_ctrl_level_e`。这三档**不是**按方便程度定的，而是严格按各技术的 TAL 层真正暴露了什么定的。等级是累积的：高一档允许低一档的一切。

| 等级 | 值 | TAL 必须提供 | netmgr 会做 | netmgr 不能做 | 在树里 |
|------|----|--------------|-------------|---------------|--------|
| `NETCONN_CTRL_OBSERVE` | 0 | 只有状态查询 + 状态回调 + get/set IP/MAC | 观察；在选路里偏好或回避 | **不能**让它起来，也不能让它下去 | wired：`tal_wired.h` 只有 `tal_wired_get_status()`、`tal_wired_set_status_cb()`、`tal_wired_{get,set}_ip()`、`tal_wired_{get,set}_mac()` —— 无 connect、无 disconnect、无 enable/disable |
| `NETCONN_CTRL_SUSTAINED` | 1 | 一个"启动"入口（在 `open()` 里调一次） | 启动一次，之后由驱动自己维持 | **不能**重试、不能退避、不能为省电停掉它 | cellular：`tal_cellular_init()` 拉起数据承载；`tal_cellular.h` 没有 connect/disconnect 对，**也没有 deinit** —— 这就是 `netconn_cellular_close()` 今天关不掉链路的原因 |
| `NETCONN_CTRL_MANAGED` | 2 | 完整的关联/解关联 | 随意关联、解关联，可重试、可退避、可交出射频、可主动下线 | — | wifi：`tal_wifi_station_connect()`、`tal_wifi_station_disconnect()`、`tal_wifi_set_work_mode()`、`tal_wifi_ap_start()/stop()`、`tal_wifi_lp_{enable,disable}()` |

**读法**：SUSTAINED 读作"netmgr 能把它启动起来"；它的 `close()` 在 TAL 长出 teardown 之前只能当作 best-effort。

这个字段不是装饰，代码有三处按它分叉：

- **每链路状态机**：`NETMGR_LINK_STATE_CONNECTING` 只有 MANAGED 链路可达 —— 因为只有它有"一次尝试"这回事（`netmgr_policy.h · NETMGR_LINK_STATE_CONNECTING`）。
- **退避**：`netmgr_retry.h` 开头就按三档说明，只有 MANAGED 有可重试的尝试。给 OBSERVE 链路配 `NETCONN_CMD_RECONN_TABLE` 是无意义的。
- **teardown 的诚实报告**：`netmgr_deinit()` 逐条调 `conn->close()`，然后**按 ctrl 分级**解释返回值（`netmgr.c · netmgr_deinit`）：

  ```
  MANAGED   有 disconnect 动词，所以失败就是真失败       → PR_ERR
  SUSTAINED tal_cellular.h 没有 deinit，承载按设计留着   → 链路本来 up 则 PR_WARN
  OBSERVE   tal_wired.h 压根没有 disconnect              → 链路本来 up 则 PR_WARN
  ```

  后两档的诚实报告不是错误而是**事实陈述**，而且只在链路本来是 up 的时候才值得说。

> 选档的判据只有一条：**你的 TAL 层真的有那个动词吗**。没有 disconnect 就不要写 MANAGED；写了 netmgr 就会在它身上安排重试和退避，而这些安排落到一个无法执行的驱动上，只会变成永远超时的死等。`netmgr_policy.h` 说得更直接：**要求一条链路做超出它等级的事是编程错误，不是运行时兜底。**

### 2.5 `set_mask` / `get_mask`：转写规则

两个 mask 是驱动里 `switch (cmd)` 的**逐位转写**：驱动里每一个**不返回 `OPRT_NOT_SUPPORTED`** 的 `case` 分支，对应 mask 里一位。netmgr 在派发前先筛（`netmgr.c · netmgr_conn_set` / `netmgr_conn_get`）：

```c
desc = netconn_registry_find(type);
if (NULL != desc && ((uint32_t)cmd >= 32 || 0 == (desc->set_mask & NETCONN_ATTR_BIT(cmd)))) {
    PR_DEBUG("netmgr conn [%s] does not support set %d", desc->name, cmd);
    return OPRT_NOT_SUPPORTED;
}
```

所以：**mask 里少一位，等于把驱动今天支持的命令变成 `OPRT_NOT_SUPPORTED`**。要从驱动重新推导，不要凭印象猜。注意那个范围测试放在前面不是啰嗦：`NETCONN_ATTR_BIT()` 是一次移位，移 32 位以上是未定义行为，所以越界的 `cmd` 绝不能走到它。

`netconn_table.c:61-82` 记着三条规则，全都会碰上：

1. 一个只 `break` 然后落到 `return OPRT_OK` 的分支，**算支持** —— 即使它什么都没做。
2. 一个显式回答 `OPRT_NOT_SUPPORTED` 的分支，**不算支持**。`netconn_cellular_get()` 的 `NETCONN_CMD_MAC` 就是这种（`netconn_cellular.c:181`）：把它筛在上一层，交给调用者的是同一个错误码，但只有一个地方回答。
3. 命令值必须密集且小于 32。`netmgr_conn_config_type_e` 今天是 `NETCONN_CMD_PRI(0)` 到 `NETCONN_CMD_RECONN_TABLE(10)`，无显式值、无空洞、11 项。`netconn_registry.h` 里那个
   ```c
   typedef char netconn_attr_mask_fits_in_u32_t[(NETCONN_CMD_RECONN_TABLE < 32) ? 1 : -1];
   ```
   把"将来加到第 32 个命令"变成编译错误，而不是一个被静默截断的 mask。

#### mask 必须与 `ctrl` 等级一致 —— 这条最容易搞错

`NETCONN_CMD_CLOSE` 是两者交汇的地方，`netconn_table.c:76-82` 专门点出来：

> 只有 `NETCONN_CTRL_MANAGED` 的链路才**能**兑现 close 命令，因此只有 wifi 带这一位。wired（OBSERVE）和 cellular（SUSTAINED）**故意**不带：两边的 TAL 层都没有任何办法让链路下去，所以它们过去回答的那个 `OPRT_OK` 是对 `tuya_iot_destroy()` 说的谎。这个 `OPRT_NOT_SUPPORTED` 更不方便，也更真实。

这不是理论洁癖，**这个不一致在本分支上被修过**（commit `8aaf9624 refactor(netconn): stop answering OPRT_OK for a close that cannot happen`）。修之前的状态是：

- `netconn_cellular_set(NETCONN_CMD_CLOSE)` 调那个空的 `netconn_cellular_close()` 然后返回 `OPRT_OK`；`tuya_iot_destroy()` 在 teardown 时下这个命令，被告知链路已关，而承载还在。
- `netconn_wired_get(NETCONN_CMD_CLOSE)` 是一个空分支，`break` 然后 `OPRT_OK`。关一条链路根本不是 getter 能读的属性，而且树里从来没有调用者问过它。
- 两个 mask 里都带着 `NETCONN_CMD_CLOSE` 位，**于是 `.ctrl` 变成纯装饰**。

修完的规则一句话：**mask 是驱动 switch 的转写，而驱动 switch 里不该有一个 TAL 层根本无法执行的动词的分支。** 加新链路时，从 `ctrl` 反推 —— 写了 `NETCONN_CTRL_OBSERVE` 或 `SUSTAINED`，`set_mask` 里就不该有 `NETCONN_CMD_CLOSE`，驱动里也不该有那个 `case`。

那次修改是**有意的行为变更**：一个把 `OPRT_OK` 当成"链路已经下去"的调用者本来就是错的，现在它会知道。

### 2.6 `caps` 位

`netconn_registry.h` 里四位，每一位都是为了替掉树里一处具体的猜测：

| 位 | 含义 | 谁在读 |
|----|------|--------|
| `NETCONN_CAP_LAN` | 这条链路上跑 LAN 直连（`tuya_lan`）有意义 | `netmgr.c · __netmgr_lan_gate()`：`netconn_registry_find(active)->caps & NETCONN_CAP_LAN` |
| `NETCONN_CAP_NETCFG_AP` | 这条链路能拉 SoftAP 配网 | `netconn_wifi.c:423` |
| `NETCONN_CAP_NETCFG_BLE` | 这条链路能走 BLE 配网。**只在构建真的链进了 BLE netcfg 后端时置位** | `netconn_wifi.c:435` |
| `NETCONN_CAP_METERED` | 流量按量计费 | **今天没有代码读它**，见 §8.2 |

`NETCONN_CAP_NETCFG_ANY` 是二者的并，用途是回答"这个镜像到底需不需要 netcfg"。

`NETCONN_CAP_LAN` 替掉的是**两层**错误判断，值得知道，因为新链路很容易重犯（`netmgr.c` 里"The LAN gate, and the two layers of the old one"那段）：

- **第一层**在 `netmgr_init()` 里，是 `#if !defined(ENABLE_CELLULAR) || (ENABLE_CELLULAR == 0)` 包住 LAN 定时器 —— 一个**编译期**开关，判的是"镜像里有没有 cellular 驱动"。于是 wifi+4G 构建连 wifi 链路上的 LAN 也一起丢了。这是"用整镜像的答案去回答每链路的问题"的典型后果。
- **第二层**在回调里面，是 `type & NETCONN_WIRED || type & NETCONN_WIFI`，其中 `type` 是 `netmgr_init()` 收到的**配置掩码**。wifi+4G 板子上这个掩码永久带两位，所以不管实际是哪条链路在承载流量，它都回答"要 LAN"。

正确的问法是对**当前激活链路的描述符**问一次，就一句。顺带地，那个 500 ms 的轮询定时器也一起没了：门控现在在每趟 reselect 末尾求值，外加 `EVENT_MQTT_CONNECTED` 一次（那是唯一一个不伴随链路事件而变化的输入 —— `client->is_activated`）。

### 2.7 驱动骨架

最小的模板是 `netconn_wired.c`（OBSERVE，200 行）和 `netconn_cellular.c`（SUSTAINED，192 行）。骨架：

```c
/* netconn_<tech>.c */
#include "netconn_<tech>.h"

#if defined(ENABLE_<TECH>) && (ENABLE_<TECH> == 1)
#include "tal_api.h"
#include "netmgr.h"
#include "tal_<tech>.h"

/* 为了下面的 TAL_NET_PROVIDER_DEFAULT。netmgr.h 以前会捎带进来，现在不会了 ——
 * 控制面的公共头不再依赖数据面。 */
#include "tal_net_provider.h"

netmgr_conn_<tech>_t s_netmgr_<tech> = {
    .base = {
        .pri      = 0,                        /* 会被 desc->default_pri 覆盖 */
        .type     = NETCONN_<TECH>,
        .status   = NETMGR_LINK_DOWN,
        .provider = TAL_NET_PROVIDER_DEFAULT, /* 会被 desc->provider 覆盖 */
        .open     = netconn_<tech>_open,
        .close    = netconn_<tech>_close,
        .get      = netconn_<tech>_get,
        .set      = netconn_<tech>_set,
    },
};

static void __netconn_<tech>_event(<TECH>_STAT_E event)
{
    netmgr_conn_<tech>_t *p = &s_netmgr_<tech>;

    if (/* 状态没变 */) {
        return;                        /* 不要报同状态，白跑一趟 pass */
    }
    p->base.status = (event == ...) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;

    netmgr_notify_link(NETCONN_<TECH>, p->base.status);   /* 新驱动直接用这个 */
}
#endif
```

驱动侧的几条硬约束：

- **`open()` 跑在调用者栈上。** `netmgr_init()` 在自己的栈上做完整个注册路径，`conn->open()` 也在里面 —— 直接伸进 vendor 的协议栈初始化。`netmgr.h · netmgr_init` 明确写了这一点，并给了反例：串口 CLI 线程的栈是 `SERIAL_CLI_STACK_SIZE`（默认 3072 字节，`src/tal_cli/Kconfig`），`netmgr_deinit()` 塞得下，`netmgr_init()` 在开了 BLE 的 T5AI 上塞不下。所以 `open()` 里别假设栈很宽裕 —— 全树**没有任何运行时检查**，那段注释和一行 warning 就是全部的防护。
- **`close()` 必须幂等**，而且要能在 `open()` 失败的路径上被调到。`netmgr_deinit()` 是幂等的，也会在 `netmgr_init()` 自己的错误路径上跑。
- **状态回调撤不回来就要承认。** `netconn_cellular_close()` 的注释是这方面的范本：TAL 层没有撤销状态回调的办法，所以 `__netconn_cellular_event()` 必须在 `close()` 之后**仍然安全进入** —— 它只碰本文件的静态状态和判过空的 `base.event_cb`，所以它是安全的。新驱动如果 `close()` 之后还可能被回调，就要保证同样的性质。
- **`open()` 里订阅的事件，`close()` 里必须退订。** `netconn_wifi.c` 在 M2 之前每个 init/deinit 周期泄漏两个订阅，这是有案底的坑。
- **报告用 `netmgr_notify_link()`。** 老驱动走 `base.event_cb`，netmgr 注册时把它指向一行的 `__netmgr_event_shim()` 转调 `netmgr_notify_link()`（详见 §8.3）。新驱动直接调 `netmgr_notify_link()` 更省事，也不必判空。如果你选择沿用 `event_cb`，**每一处都要判空** —— 树里三个驱动现在都判了，包括原先漏判的 `NETCONN_CMD_PRI` 那个 set 分支（`netconn_wifi.c · netconn_wifi_set`、`netconn_wired.c · netconn_wired_set`、`netconn_cellular.c · netconn_cellular_set`），照抄它们是安全的。

驱动可以**读自己的描述符**来回答"我这次构建被允许做什么"。`netconn_wifi.c:418-441` 就是范例：

```c
const netconn_desc_t *desc = netconn_registry_find(NETCONN_WIFI);
netconn_caps_t        caps = (NULL != desc) ? desc->caps : NETCONN_CAP_NONE;
if (NULL == desc) {
    PR_WARN("wifi has no registry row, skipping netcfg");
}

if ((netmgr_wifi->netcfg.type & TUYA_NETMGR_NETCFG_AP) && (caps & NETCONN_CAP_NETCFG_AP)) {
    ap_netcfg_init(&netmgr_wifi->netcfg);
    netcfg_start(NETCFG_TUYA_WIFI_AP, __netconn_wifi_netcfg_finish, NULL);
}
```

注意 NULL 描述符的处理：两个分支都当作"没有配网能力"而跳过，**绝不隐式当成 yes**。

### 2.8 板级覆盖：`netconn_registry_set_table()`

默认表在 `netconn_table.c:150-197`。板子要换掉整张表（重排优先级、改 provider、改 caps、去掉某条链路）时用：

```c
OPERATE_RET netconn_registry_set_table(const netconn_desc_t *table, uint32_t count);
```

规则，一条不能少：

- **在 `board_register_hardware()` 里调**，也就是 `netmgr_init()` 之前。每个 app 都会先跑前者。
- **表必须活得比 netmgr 长，什么都不复制**（`netconn_table.c:306-309`）。板子自己 translation unit 里的一个 `static const` 数组正好满足 —— 别传栈上的数组、别传 malloc 出来又释放的内存。
- **晚了会被拒，不是静默忽略**。`netconn_registry_get_table()` 会把 `s_table_taken` 闩上（`netconn_table.c:249`），之后再 set 返回 `OPRT_COM_ERROR`。"静默回落到默认表"正是这个 API 存在的意义，所以它宁可报错。
- **注册前一次性校验**（`netconn_table.c:288-303`）：每行 `name` 和 `conn` 非空、`type` 不是 `NETCONN_AUTO`、表内 `type` 唯一。一张畸形的板级表是构建期错误，责任在装它的那次调用，所以在这里查而不是之后一行行踩上去。
- 只读查询 `netconn_registry_find()` **故意不**闩 `s_table_taken`（`netconn_table.c:333-335`）—— netmgr、CLI、策略层都要用它，它不该去决定板级覆盖还允不允许。
- **行序有意义**：优先级相同的两条链路按注册序排，因为 `__netmgr_conn_register()` 追加到链表尾，而排序的次键是 `reg_index`。默认表的行序（wired、cellular、wifi）就是 M2 之前 `netmgr_init()` 的注册序，逐字节保留 —— 这是让重构"可证明地"而不是"大概地"行为中性的原因。

为什么是显式调用而不是 weak symbol、也不是 linker section —— 见 §6.1 / §6.2。

### 2.9 Kconfig 该放哪

树里两种写法，按语义选：

- **平台能力**：`ENABLE_WIFI` / `ENABLE_WIRED` 定义在各平台的 `platform/<PLAT>/Kconfig` 里 —— 意思是"这块芯片有这个射频/PHY"。
- **组件特性**：`ENABLE_CELLULAR` 定义在 `src/tuya_cloud_service/Kconfig · ENABLE_CELLULAR`，是 `menuconfig`，默认 `n`，带模组的板子在自己的 `boards/*/Kconfig` 里 `select ENABLE_CELLULAR`（例如 `boards/T5AI/TUYA_T5AI_BOARD/Kconfig:81`）。子选项（如 `CELLULAR_APN`）放在 `if (ENABLE_CELLULAR)` 里。

新链路如果是"外挂模组"性质，跟 cellular 一样走第二种。

**注意 guard 的写法。** `netconn_table.c` 里的守卫一律写成 `#if defined(X) && (X == 1)`，不是 `#ifdef X`，理由记在文件头注释里：驱动自己（`netconn_cellular.c:15`）就是这么守着它那个静态实例的，而 `netmgr.c` 以前用 `#ifdef ENABLE_CELLULAR`，于是显式的 `ENABLE_CELLULAR 0` 会产生一个未定义引用。新链路照抄 `defined(X) && (X == 1)`。

### 2.10 怎么验证新链路接上了

`netmgr` 这条 CLI 命令是泛化于注册表的，dump 会自动多出一行：

```
netmgr: configured 0x1a, active wifi, status link_up, links 3
  idx name      pri status    state      ctrl      prov caps
  ----------------------------------------------------------
   0  wired       2 link_down down       observe      0 lan
   1  cellular    0 link_down down       sustained    0 metered
  *2  wifi        1 link_up   unverified managed      0 lan,netcfg-ap
```

要核的东西：新链路**出现在 dump 里**（注册成功）、`pri`/`prov`/`caps`/`ctrl` 与你表里那一行**一致**（描述符生效）、`state` 列**会走动**（报告通道通了）。`netmgr switch <name>` 按注册表里的**名字**解析（`netmgr_cli.c · __netmgr_cli_name_to_type()`），所以新链路的 `name` 立刻就能用。

两列状态**故意都在**，而且它们合法地互相矛盾：`status` 是驱动的两值视角（`conn->status`），`state` 是 netmgr 自己的状态机。`link_up degraded` 读作"链路有地址、在跑 LAN 流量，而 netmgr 有证据表明经它到不了云"。

---

## 3. 扩展路径二：加一个 socket provider

### 3.1 分工

| 谁 | 决定 | 怎么表达 |
|----|------|----------|
| 控制面（netmgr） | 用**哪条链路** | 排序 → `s_netmgr.active` |
| 数据面（tal_network） | 用**哪个 socket 后端**、绑**哪个源地址** | 一个 `tal_net_route_t` |

两者的接缝就是描述符的 `provider` 字段：注册时抄进 `conn->provider`，选路选中这条链路后由 `netmgr.c · __netmgr_snap_provider()` 读出来放进 route 的 provider 半边。

数据面拿着 route 干两件事：

- `tal_net_provider_ops()` 用 `route.provider` 索引 `providers[]`，返回那个后端的 `TAL_NETWORK_OPS_T *`。`tal_network.c` 里所有 socket 原语（`TAL_NET_EXEC_OP` 宏，三十多个）都从这里拿函数表。
- `tal_net_connect()` 在真正 connect 之前，用 route 的源地址把 socket 绑上去（`tal_network.c · __net_connect_bind_active_src()`）。四条设计约束，都值得知道：
  - **只在 connect 上做，不在 `socket_create` 上做**：刚建的 socket 还没有方向，它可能变成 listener，在那里绑单播地址会毁掉每一个 server socket。connect 明确是出向的，所以只有它是安全的位置。
  - **只在没人绑过时做**：自己管本地地址的调用者必须赢。pjproject（ICE/STUN/TURN）为收集候选自己绑每一个 socket，而在 lwIP 上对一个仍处于 CLOSED 的 pcb 二次 bind 会成功并静默移动本地地址，所以无条件绑会悄悄毁掉候选收集而不是响亮地失败。判"绑过没有"时**端口也算**，因为 `bind(ANY, 0)` 是一次真的 bind。
  - **绑失败不致命**：退回让协议栈自己选源，单接口设备照样连得上。让 connect 失败等于把一个路由偏好变成一次中断。
  - **Linux host 构建整段编译掉**：host 有策略路由、回环和这个 SDK 一无所知的接口，在那里做源绑定破坏的比修的多。

### 3.2 route 是唯一真相源

```c
typedef struct {
    uint8_t provider;      /* 哪个 socket 后端，取 TAL_NET_PROVIDER_* */
    TUYA_IP_ADDR_T src_ip; /* 出向 socket 绑的源地址，0 = 不绑 */
} tal_net_route_t;

OPERATE_RET tal_net_route_set(const tal_net_route_t *route);
OPERATE_RET tal_net_route_get(tal_net_route_t *route);
```

**两半必须一起动。** 一次链路切换如果把新后端和新地址当成两次独立的存储发布出去，就留下一个窗口：socket 已经建在新后端上，却还绑着刚刚消失的那条链路的地址。`tal_net_route_set()` 用一次带锁更新关掉这个窗口，`tal_net_route_get()` 永远返回**一致的快照**而不是各取一半。

历史上这就是两次独立的推送（provider 在锁内、地址在锁外），commit `1fde338c fix(netmgr): publish the backend and source address as one route` 把它们并成一个值。

控制面推它的三步，两个调用点都一样（`netmgr.c` 里"Pushing the active route down to the data plane"那段）：

1. **取锁之前**：`tal_net_route_get()` 读当前装着的 route，这样一个解析不出链路的 type 会保留它已有的后端；
2. **锁内**：`__netmgr_snap_provider()` 走连接链表，解析出 provider 并把连接指针交回来；
3. **放锁之后**：`__netmgr_push_route()` 经 `conn->get(NETCONN_CMD_IP)` 读地址（cellular 上是阻塞的模组交互，所以绝不能在锁内跑），然后一次装上两半。

地址读不到、或链路是 down 时，`src_ip` 置 0，含义是"不要绑" —— **一个不绑的 socket 好过一个绑在链路已不再拥有的地址上的 socket**。

### 3.3 锁的语义：保护"一对字段的一致性"，不是"一个字段的可见性"

这是本节最容易被"顺手修好"而修坏的地方。`tal_net_provider.c:67-101` 把纪律写在代码旁边：

> `s_route_lock` 的存在是为了让 **(provider, src_ip) 这一对**保持一致。它**不是**为了让其中任一字段单独可见，而且那也不需要它。

于是：

| 函数 | 取锁？ | 为什么 |
|------|--------|--------|
| `tal_net_route_set()` | 取 | 成对移动就是它的全部契约 |
| `tal_net_route_get()` | 取 | 成对读取就是它的全部契约 |
| `tal_net_route_src_ip()` | **不取** | 单字段读，一个字并不会撕裂，且不看另一半 |
| `tal_net_provider_ops()` | **不取** | 热路径。见下 |

`tal_net_provider_ops()` 是最要紧的那个：`tal_network.c` 的 `TAL_NET_EXEC_OP` 从**每一个** socket 原语里调它 —— send、recv、recvfrom、select、fd_isset 以及另外三十来个，其中好几个在紧凑的 select 循环里。给它加一把 mutex，等于**在每个 socket 操作下面塞一个内核级操作**，在 RTOS 上还多出一个全新的优先级反转来源。在这份状态被合并之前，那条路径本来就是一次朴素的数组读，现在依然是：

```c
TAL_NETWORK_OPS_T *tal_net_provider_ops(void)
{
    uint8_t provider = s_provider_registry.route.provider;
    tal_net_provider_t *entry = s_provider_registry.providers[provider];
    if (NULL == entry) {
        return NULL;
    }
    return &entry->ops;
}
```

它成立靠三件事：`provider` 是单字节、不会撕裂；`providers[]` 静态初始化之后不可变；这两个都不需要与 `src_ip` 相符。**索引一定在范围内**，因为每个写者都对着 `TAL_NET_PROVIDER_MAX` 校验过 provider。

所以：**不要"修好"这些不取锁的读者。** 那只会把热路径成本加回来，而没有让任何东西更正确。真的需要两个字段互相吻合的调用者，该调 `tal_net_route_get()` —— 它就是为这个存在的。

还有一处细节：`s_route_lock` 在 `tal_net_provider_init()` 之前是 `NULL`，写者此时退化成不加锁访问（`__route_lock()` / `__route_unlock()` 判空）。这不是漏洞：mutex 没法在静态初始化期创建，而 route 在 init 之前就必须可用；那么早只有单线程的启动路径在跑，而写者（netmgr）在 init 之后很久才存在。同理，后端表是**静态初始化**的而不是在 `tal_net_provider_init()` 里填的 —— 早期的 socket 使用者会在 init 跑之前就到 `tal_net_provider_ops()`，它们必须在那里找到一个能用的后端。

### 3.4 加一个 provider 的步骤

今天树里只有两个后端，而且**一次构建只链进一个**：

```c
typedef uint8_t tal_net_provider_id_t;
#define TAL_NET_PROVIDER_POSIX    (0)
#define TAL_NET_PROVIDER_TKL      (1)
#define TAL_NET_PROVIDER_AT_MODEM (2)
#define TAL_NET_PROVIDER_MAX      (3)

#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
#define TAL_NET_PROVIDER_DEFAULT TAL_NET_PROVIDER_POSIX
#else
#define TAL_NET_PROVIDER_DEFAULT TAL_NET_PROVIDER_TKL
#endif
```

`tal_posix.c` 和 `tal_tkl.c` 各自用同一个 `ENABLE_LIBLWIP` / `OPERATING_SYSTEM` 判断把自己整体条件编译掉，所以恰好一个 provider 会存在。`TAL_NET_PROVIDER_DEFAULT` 这个名字的意义就是**让那个判断只出现在一个地方**。

`TAL_NET_PROVIDER_AT_MODEM` 是个**只有 `#define` 没有实现**的常量 —— 全树没有代码真的发布过这个值；唯一提到它的地方是它自己的定义，以及几处解释这段历史的注释。这也是 §6.6 那条"死掉的 4G 分支"的根源。

要加第三个后端：

1. **`tal_net_provider.h`**：加一个 `TAL_NET_PROVIDER_<X>` 常量，**并把 `TAL_NET_PROVIDER_MAX` 加一**。忘了加 `MAX` 的后果是 `tal_net_route_set()` 直接拒掉你的 provider（`route->provider >= TAL_NET_PROVIDER_MAX` → `OPRT_INVALID_PARM`）。
2. **新一个 `src/tal_network/src/tal_<x>.c`**，定义 `tal_net_provider_t tal_net_provider_<x>`，填满 `TAL_NETWORK_OPS_T` 的函数表（35 个函数指针）。`src/tal_network/CMakeLists.txt` 用的是 `aux_source_directory`，新文件自动进编译，**不用改 CMake**。
3. **改 `tal_net_provider.c` 的静态初始化器**，把新 provider 挂进 `providers[]`：
   ```c
   static tal_net_provider_registry_t s_provider_registry = {
       .route                                = {.provider = TAL_NET_PROVIDER_DEFAULT, .src_ip = 0},
       .providers[TAL_NET_PROVIDER_DEFAULT] = &TAL_NET_PROVIDER_DEFAULT_OBJ,
       /* 新增： */
       .providers[TAL_NET_PROVIDER_<X>]     = &tal_net_provider_<x>,
   };
   ```
   **这一步漏了最难查**：`tal_net_route_set()` 会接受你的 provider（在范围内），然后 `tal_net_provider_ops()` 拿到 `NULL`，全部 socket 原语开始返回失败，而且没有一条日志说明是为什么。表是静态初始化的、之后永不改写，这正是读者不需要加锁的前提，所以新条目也必须放进**静态初始化器**里，不要在 `tal_net_provider_init()` 里补。
4. **让某条链路用它**：把那条链路的 `netconn_desc_t.provider` 从 `TAL_NET_PROVIDER_DEFAULT` 改成 `TAL_NET_PROVIDER_<X>`。**改驱动的静态初始化没用**（会被描述符盖掉），要么改 `netconn_table.c` 的默认表，要么走 §2.8 的板级覆盖 —— 后者是"板子真有第二个后端"的正规表达方式。

> `src_ip` 之所以放在 route 里而不是放在 provider 上：多条链路可以共用一个 provider（T5AI 上 wifi 和 cellular 都是 `TKL`），所以源地址是**激活链路**的属性，不是 provider 的属性。`tal_net_provider_t` 因此只剩一张函数表 —— 它曾经带过 `.name` / `.type` / `.ipaddr` 三个字段，全都只写不读，已删除。

---

## 4. 扩展路径三：换掉选路排序

### 4.1 钩子

```c
typedef void (*netmgr_policy_select_cb_t)(const netmgr_select_in_t *in,
                                          netmgr_select_out_t *out, void *ctx);

OPERATE_RET netmgr_policy_select_cb_set(netmgr_policy_select_cb_t cb, void *ctx);
```

这是让板子表达参数表达不了的东西的扩展点 —— 一个信号强度门槛、一个时段偏好、一条"充电时不用 cellular"的规则。传 `NULL` 恢复内置排序。`ctx` 是不透明的，netmgr 从不解引用它。

安装时机不限，`netmgr_init()` 之前也行。有锁时在锁下写入（`netmgr.c · netmgr_policy_select_cb_set`），因为一趟 pass 会同时读 cb 和 ctx，**一个 ctx 配错的钩子比一个晚装一瞬的钩子更糟**。第一次 `netmgr_init()` 之前既没有锁也没有 pass，那时一次朴素存储就是正确的。

**输入是一份平坦快照，不是指向 netmgr 活状态的指针**，所以钩子拿不到连接链表、调不了驱动、也没法死锁。它被允许知道的一切都在 `in` 里：

| `netmgr_select_in_t` | |
|---|---|
| `links` / `count` | 候选数组，**按注册序**；`count > 0` 时 `links` 不为 NULL |
| `active` | 当前激活链路，没有则 `NETCONN_AUTO` |
| `pinned` | `netmgr_policy_pin()` 装的手动覆盖，或 `NETCONN_AUTO` |
| `now_ms` | 单调毫秒；本结构里所有 `*_ms` 共用这个基准 |
| `active_since_ms` | 当前链路何时变为激活（无激活链路时等于 `now_ms`）。这加上 `policy.min_dwell_ms` 就是完整的 dwell 计算，所以 dwell 不需要自己的状态 |
| `policy` | 生效中的策略，钩子不必再查一次 |

| `netmgr_link_view_t` | |
|---|---|
| `type` | |
| `pri` | **实时** `conn->pri`，`NETCONN_CMD_PRI` 可能已经改过它。每趟在这里重读，正是 `NETCONN_CMD_PRI` 那个缺陷的修法：没有缓存的顺序可以跟它矛盾了 |
| `reg_index` | 注册序位置，0 起，也就是 `s_netmgr.report[]` 的下标。次排序键，越小越优 |
| `state` | `netmgr_link_state_e` |
| `caps` / `ctrl` | 来自描述符 |
| `eligible_at_ms` | `up_debounce_ms` 何时走完，0 表示现在就够格。由 `netmgr.c` 算（它拥有 up 时间戳），这里只是遵守 |

输出只有两个字段：

- **`choice`**：应该激活的链路，或者 `NETCONN_AUTO` 表示"没有链路该承载流量"。`NETCONN_AUTO` 是合法答案，netmgr 会照办 —— 那本来就是没有链路 up 时 `s_netmgr.active` 已经持有的值。
- **`recheck_ms`**：多少毫秒后再问我一次，0 表示"我自己没有截止时间"。这是钩子**不用自己拿定时器**就能获得时序的唯一手段 —— netmgr 会把它折进那唯一一个共享 deadline 里。任何 hysteresis 都只需要这一个机制。

`out` 在调用前被预置成 `{NETCONN_AUTO, 0}`，所以**什么都不写的钩子等于回答了"没有链路"**。

### 4.2 资格底线：被校验、且不可覆盖

netmgr **校验**钩子的答案而不是相信它（`netmgr.c · netmgr_policy_select()`）：

```c
if (NETCONN_AUTO == out->choice) {
    return;                       /* 合法答案，原样接受 */
}
for (i = 0; i < in->count && NULL != in->links; i++) {
    if (in->links[i].type == out->choice && NETMGR_LINK_STATE_IS_UP(in->links[i].state)) {
        valid = TRUE;
        break;
    }
}
if (!valid) {
    PR_ERR("netmgr policy hook chose [%s], which is not an eligible candidate; using the built-in ranking", ...);
    netmgr_policy_select_default(in, out);
}
```

精确地说，被校验的是：

1. `NETCONN_AUTO` **原样接受**。
2. 其他任何值必须**出现在 `in->links` 里**，**并且**满足 `NETMGR_LINK_STATE_IS_UP()`。这就是**资格底线**，唯一一条什么都不能覆盖的规则 —— pin 不能，所以钩子也不能。
3. 两项测试任一失败：打错误日志，整体替换成 `netmgr_policy_select_default()`，并且**连钩子给的 `recheck_ms` 一起丢掉** —— 一个 netmgr 无法兑现的决定，不携带任何值得保留的截止时间。

资格底线的单一定义在 `netmgr_policy.h`：

```c
#define NETMGR_LINK_STATE_IS_UP(s) \
    ((s) == NETMGR_LINK_STATE_UNVERIFIED || (s) == NETMGR_LINK_STATE_ONLINE || (s) == NETMGR_LINK_STATE_DEGRADED)
```

注意 `DEGRADED` **通过**底线：一条降级链路在它是唯一选择时仍然是最好的选择。排除它会得到 `NETCONN_AUTO`、推下 `src_ip = 0`、并对一台 LAN 好用只是暂时连不上云的设备发布 `NETMGR_LINK_DOWN`。

**debounce 故意不对钩子强制**：`eligible_at_ms` 是给钩子的**输入**，产品排序有权忽略自己的 hysteresis。一条处于 debounce 中的链路本身是 up 的，所以采纳这种选择不会把流量送上一条死链路 —— 而后者才是这段校验要保护的性质。

这个 dispatch 之所以实现在 `netmgr.c` 而不是 `netmgr_policy.c`：**校验一个选择需要活的候选集，而只有 `netmgr.c` 有**。同一条接缝也决定了 `netmgr_probe_backend_set()`、`netmgr_probe_report()`、`netmgr_probe_epoch_get()`、`netmgr_probe_stat_get()`、`netmgr_link_state_get()`、`netmgr_reselect_request()` 全都住在 `netmgr.c` 里 —— 它们各自需要一份 `netmgr.c` 独占的状态。

### 4.3 钩子在 `s_netmgr.lock` 下运行

这是塑造钩子形状的那条约束。`netmgr.c` 顶部的锁契约禁止在锁下做驱动回调和 `tal_event_publish()`，钩子受同一条规则约束、理由完全相同。于是钩子**必须是对 `in` 的纯算术**：

| 不能做 | 后果 |
|--------|------|
| 调 `netmgr_conn_get()` / `netmgr_conn_set()` | 非递归 mutex 上自锁。`tkl_mutex_create_init()` 只在 port 明确要求时才映射到递归原语（FreeRTOS 各 port 用 `configUSE_RECURSIVE_MUTEXES` 门控；LINUX port 总是设 `PTHREAD_MUTEX_RECURSIVE`）—— 也就是说在 RTOS 上这是**硬死锁** |
| `tal_event_publish()` | 订阅者可能同步回调进 netmgr（tuya_iot 的订阅者就会调 `tuya_iot_reconnect()`） |
| 阻塞，或调任何可能阻塞的 `tal_*` | 把整个状态机和所有 netmgr 调用者停在一把锁上 |
| 保存 `in` 里的指针留着以后用 | `in` 是本趟 pass 的快照，出了这次调用无效 |

它想要的一切都已经在 `in` 里 —— 这正是 `in` 被做成快照而不是一组访问器的原因：**用类型把规则变成可执行的，而不是劝告性的**。

同样出于这条纪律：`netmgr_policy.c` 里的 `netmgr_policy_get()` 和 `netmgr_policy_pin_get()` 是**无锁**的，这样它们才能在这把锁里面被安全调用。这个性质是承重的，不是碰巧 —— 它是整个模块只有一把 mutex 的原因。

### 4.4 只想改一种情形：委派给内置排序

```c
void netmgr_policy_select_default(const netmgr_select_in_t *in, netmgr_select_out_t *out);
```

它是公开的，正是为了让"只想覆盖一种情形"的钩子不必把 tie-break 规则重新实现一遍。也是 netmgr 在钩子给出不可用答案时的兜底路径。

内置规则全文见 `netmgr_policy.h · netmgr_policy_select_default` 的注释（七条），要点：

1. 丢掉所有不满足 `NETMGR_LINK_STATE_IS_UP()` 的、以及 `eligible_at_ms` 还在未来的 —— **这两条就是资格底线，什么都不覆盖它，pin 也不行**；
2. `in->pinned` 若够格，选它，停。pin 是操作员指令，压过下面所有自动考量；
3. 存活者分两层：NOT-SUSPECT（`UNVERIFIED`、`ONLINE`）和 SUSPECT（`DEGRADED`）。任何 not-suspect 胜过所有 suspect。`policy.probe_demote` 为 FALSE 时只有一层；
4. 同层内 `pri` 大者胜；`pri` 相同则 `reg_index` 小者胜（M2 的 tie-break，原样保留）；
5. 赢家不是当前激活链路、且当前激活链路仍够格、且 `policy.preempt` 为 FALSE 时，保持当前 —— **除非**当前激活链路自己是 SUSPECT 而存在 not-suspect 的替代者，此时放弃粘滞、照切。这个例外不是精修，它是防止第 5 条把整次重构静默废掉：`DEGRADED` 能过资格底线，没有这个例外，一个为了路由稳定而设 `preempt = FALSE` 的产品会**连故障切换一起丢掉** —— wifi 挂在一个没有 WAN 的 AP 上变成 DEGRADED，因为粘滞而留任，本该接手的 cellular 永远选不上。粘滞的正确读法是"不要在同样好的链路之间乱跳"，绝不是"无视当前链路已经坏了"。这个例外挂在 `probe_demote` 上，因为关掉降级时 `DEGRADED` 按定义就不是排序信号，那它也不该打破粘滞 —— 一个标志，一个含义；
6. 赢家不是当前激活链路、且当前仍够格、且距 `active_since_ms` 还没到 `min_dwell_ms` 时，保持当前并把 `recheck_ms` 设为剩余 dwell。注意第 5、6 条共享一个前提：**一条已经不够格的激活链路永远赢不了这两条**，所以粘滞和 dwell 都不可能把设备困在死链路上；
7. 把 `recheck_ms` 设成被第 1 条以 debounce 为由丢掉的那些链路中最近的那个 `eligible_at_ms`，这样一条正在成熟的链路会在成熟时被重新考虑。

`UNVERIFIED` 与 `ONLINE` 为什么同层，是这里最微妙的一条：**把它们分层会让被动探测死锁** —— 被动探测只能判断**激活**链路，所以一条永不被选中的链路永远无法被验证，而高优先级链路会等一个只有被选中才能挣到的判决。

SUSPECT 层为什么是**降级**而不是**排除**：一条降级链路在它是唯一可用的东西时仍然是最好的东西。

### 4.5 关于策略参数本身

排序的**参数**（不是排序本身）通过 `netmgr_policy_set()` 调，随时可调，**不闩** —— 策略是调优而非拓扑，产品完全可能在配网结束后加长 dwell、在 OTA 期间关掉探测。改动在下一趟 reselect 生效，而 netmgr 会立刻安排一趟，调用者不必等链路事件。

`policy` 被复制；`policy->revalidate.entry` **不复制**，那个数组必须活得比 netmgr 长（跟 `netmgr_retry_table_t` 同一条规则）。

策略故意**不**挂在 `netmgr_conn_set()` 下面：策略是**设备**的属性而不是某条链路的属性，而每个 `NETCONN_CMD_*` 都是发给某一条链路的。加一个 `NETCONN_CMD_POLICY` 会让"哪条链路的策略"变成一个没有答案的问题。

> **默认值的来源正在变。** 本分支的工作树里，`NETMGR_POLICY_DEFAULT_INIT` 的 `probe_enable` / `probe_demote` / `min_dwell_ms` 已改为从 Kconfig 取值（`src/tuya_cloud_service/Kconfig` 的 `ENABLE_NETMGR_PROBE`、`NETMGR_PROBE_DEMOTE`、`NETMGR_POLICY_MIN_DWELL_MS`），默认全部关闭 / 为零。写代码时以 `netmgr_policy.h · NETMGR_POLICY_DEFAULT_INIT` 的当前内容为准。
>
> 这个默认的中性论证值得记住：`NETMGR_POLICY_DEFAULT_INIT` 里每个时序参数为 0，**不是调优选择而是中性论证本身** —— 为零就没有 deadline 要 arm，共享定时器永不启动，debounce 和 grace 分支短路成"现在"，`netmgr_policy_select()` 退化成"够格链路中 `pri` 最大的、注册序打破平手"，也就是**一个真的能工作的优先级排序版的 `__get_active_conn()`**。

---

## 5. 扩展路径四：加一个探测后端

### 5.1 这个模块要解决的那一个比特

`conn->get(NETCONN_CMD_STATUS)` 说 `NETMGR_LINK_UP`，含义只有"有关联、有地址"，仅此而已。一台加入了没有 WAN 的 AP 的设备（强制门户、上行断了、路由器还在协商），按 netmgr 能看到的一切度量都是 UP —— 于是在 wifi 默认优先级(1) 高于 cellular(0) 的 wifi+4G 板子上，设备**永久钉死在无用链路上**，4G 承载一个字节都不走。

补上这个缺口需要一个 netmgr 没有的比特：**这条链路上的流量到底有没有到达任何地方**。`netmgr_probe.h` 就是这个比特的来源。

### 5.2 后端契约

```c
typedef struct {
    const char *name;                 /* 日志用短名，不能 NULL */
    OPERATE_RET (*start)(void);       /* 开始观察。NULL 表示不需要 setup */
    void (*stop)(void);               /* 停止并释放 start() 创建的一切。NULL 允许 */
} netmgr_probe_backend_t;

extern const netmgr_probe_backend_t netmgr_probe_backend_mqtt;  /* 内置被动后端 */
OPERATE_RET netmgr_probe_backend_set(const netmgr_probe_backend_t *backend);
```

| 规则 | 说明 |
|------|------|
| 生命周期 | netmgr 在 `netmgr_init()` 末尾 `start()`，在 `netmgr_deinit()` 开头 `stop()`。可以在 `start()` 里创建资源，但必须在 `stop()` 里全部释放 |
| 锁 | 两个钩子都在 **`s_netmgr.lock` 释放**的状态下被调，与 `conn->open()` / `close()` 同一条规则。它们是向外的调用，允许阻塞 —— 所以后端可以在 `start()` 里 `tal_event_subscribe()`，内置后端就是这么做的 |
| `start()` 失败 | 记日志，`netmgr_init()` **继续**。一台无法验证链路的设备仍然必须能用这条链路；把这里当致命错误会把一个诊断特性变成开机失败 |
| `stop()` 幂等 | 必须。`netmgr_deinit()` 是幂等的，而且可能在 `netmgr_init()` 自己的错误路径上跑，所以 `stop()` 可能在**没有配对的成功 `start()`** 的情况下被到达 |
| 存储 | 实例是 `const`、由产品提供。netmgr 只读它，并在一整个 init/deinit 周期里持有这个指针，所以存储必须活得更长 —— 产品 translation unit 里的一个 `static const` |
| 安装时机 | **必须在 `netmgr_init()` 之前**，之后返回 `OPRT_COM_ERROR` 而不是静默忽略。与 `netconn_registry_set_table()` 同一条纪律、同一个理由 |
| 传 `NULL` | 意思是"这次运行不装任何后端"。**它不能让内置后端不被链接** —— 那是一个调用点，而调用点做不了链接期决定：`netmgr_init()` 在这次调用引导你避开的那个分支里仍然点了 `netmgr_probe_backend_mqtt` 的名字，对象照样被拉进来。要让镜像里根本没有被动后端，是**不 select Kconfig 的 `ENABLE_NETMGR_PROBE`**，那会把 `netmgr_probe.c` 整个从构建里去掉 |

另外要分清两个"关"：

- `netmgr_probe_backend_set(NULL)` = 这次运行没有后端在产生判决；
- `netmgr_policy_t.probe_enable = FALSE` = 判决照收但一律丢弃，`verify_timeout_ms` 不 arm，`NETMGR_LINK_STATE_DEGRADED` 不可达。运行时开关用这个。
- Kconfig 的 `ENABLE_NETMGR_PROBE` = 编译期决定 `netmgr_probe.c` 在不在镜像里，同时决定上面那个策略位的默认值。

> 提供**主动**后端的产品仍然要把 `probe_enable` 打开（select `ENABLE_NETMGR_PROBE`，或运行时 `netmgr_policy_set()`），否则它的判决会撞在同一道门上。

### 5.3 判决语义：GOOD 与 BAD 不对称

```c
typedef enum {
    NETMGR_PROBE_UNKNOWN = 0,   /* 没人上报过；零值结构体持有的值 */
    NETMGR_PROBE_GOOD    = 1,   /* 流量到达了云端 */
    NETMGR_PROBE_BAD     = 2,   /* 流量没到达云端 */
} netmgr_probe_verdict_e;
```

- **GOOD 立即且无条件被信**：一个 GOOD 清掉所有累计的 BAD，把链路推到 `NETMGR_LINK_STATE_ONLINE`。因为 GOOD 是**正面证据** —— 有一个字节回来了 —— 一条坏链路造不出它。
- **BAD 是证据而非证明**：计入 `netmgr_policy_t.probe_bad_threshold`，只有计数达到阈值链路才进 `DEGRADED`。BAD 是"证据的缺席"，而缺席的原因有很多不是链路。

`NETMGR_PROBE_UNKNOWN` **没有人上报**，它是零值结构体持有的值，也是 netmgr 内部"还没有判决"的值 —— 这样 `NETMGR_LINK_STATE_UNVERIFIED` 和一个清零的累加器天然一致。

`netmgr_probe_source_e`（`MQTT` / `ATOP` / `TIMEOUT` / `CUSTOM`）**只进日志，netmgr 从不据它分叉**。它存在是因为调试一条老被降级的链路时最难的事就是查出**是谁降的它**，而一行 "wifi degraded, 3 bad from mqtt" 立刻回答了这个问题。自定义后端用 `NETMGR_PROBE_SRC_CUSTOM`。

上报接口刻意**不让上报者点名链路**：

```c
OPERATE_RET netmgr_probe_report(const netmgr_probe_result_t *result);
OPERATE_RET netmgr_probe_report_simple(netmgr_probe_verdict_e verdict, netmgr_probe_source_e source);
```

上报者不知道是哪条链路载着它的包，它只知道包有没有到。让它点名，等于允许一个调用者去降级一条它从未用过的链路。归属由 netmgr 按 epoch 算。

**线程模型不可协商**：`netmgr_probe_report()` 在 `s_netmgr.lock` 下记录判决，然后投出与 `netmgr_notify_link()` **同一个合并 work item**。它**不**评估判决、不 reselect、不发布、不碰路由。具体后果：内置后端的调用者在 `tuya_iot_yield()` 线程上，那也是 MQTT keepalive 的泵，在那里跑一趟 reselect 会把一个阻塞的 `conn->get(NETCONN_CMD_IP)` 模组 AT 交互塞进 keepalive 路径。

判决的合并方式与链路报告**不同**：判决是**累加**而不是覆盖。链路报告可以被丢掉换成稳定态，因为 handler 会重读每个驱动；判决不能，因为**没有东西可重读** —— 连续 BAD 的计数**本身就是**状态。所以 handler 跑之前到的 GOOD 仍然清掉累计的 BAD，两个 BAD 仍然记两次。

被"故意丢弃"并返回 `OPRT_OK` 的四种情况：`netmgr_init()` 还没种下状态、`netmgr_deinit()` 已开始、没有激活链路、策略关掉了探测。**上报者对这四种都没有恢复动作，也不该把它们当错误** —— 它在陈述一个关于世界的事实，不是在请求一个动作。

### 5.4 会话 epoch：没有 epoch 的判决为什么不可信

```c
#define NETMGR_PROBE_EPOCH_ANY ((uint32_t)0)
uint32_t netmgr_probe_epoch_get(void);
```

netmgr 每装一个**不同的**路由（不同的激活链路，或同一链路换了源地址）就把一个计数器加一，回绕时跳过 0。判决是关于**载过那些流量的路由**的，不是关于报告到达时恰好装着的路由 —— 而这两者在切换与上报赛跑时必然不同。

它防的失败很具体：

> wifi 是激活链路，它的网络是死的；MQTT 超时；netmgr 的 verify timeout 已经把 wifi 降级并切到 cellular；然后 MQTT 断连回调才触发并上报 BAD。**没有 epoch，这个 BAD 落在什么都没做错的 cellular 头上。**

netmgr 的处理是丢弃陈旧判决（`netmgr.c · netmgr_probe_report`）：

```c
if (NETMGR_PROBE_EPOCH_ANY != result->epoch && result->epoch != s_netmgr.route_epoch) {
    /* 丢掉，并打一行说明 epoch 不匹配的 debug */
}
```

所以后端的义务是：**在观察发生的那一刻**读 epoch，而不是在上报的那一刻读。内置后端就是这么做的（`netmgr_probe.c · __probe_mqtt_connected_cb`）：

```c
uint32_t epoch = netmgr_probe_epoch_get();
s_session_epoch = epoch;              /* 记住这个会话的 epoch，给结束它的那次 teardown 用 */

const netmgr_probe_result_t result = {
    .verdict = NETMGR_PROBE_GOOD, .source = NETMGR_PROBE_SRC_MQTT, .epoch = epoch,
};
(void)netmgr_probe_report(&result);
```

断连回调带的是**结束的那个会话的** epoch，不是重新读一次（`netmgr_probe.c · __probe_mqtt_disconnected_cb`）。这样一次由 netmgr 自己的切换引起的 teardown 会被 netmgr 当作陈旧丢弃，而不是记到刚被选中的链路账上。

拿不到 epoch 时传 `NETMGR_PROBE_EPOCH_ANY`，含义是"我没有 epoch，归给现在激活的那条"。**这个窗口即使不设防也能活**，值得先知道再决定要不要做更精巧的东西：一次错误归属最多让新链路的计数器多加一次，`probe_bad_threshold` 大于 1，而下一个 GOOD 就会清零 —— 一个 MQTT 周期内自我纠正。epoch 存在的意义，是让一个**主动**后端（它能跨越自己的探测保存状态）**免费**获得精确归属。

`netmgr_init()` 跑过之后 epoch 永不为 0（种在 1）；回绕不是事件，因为只测相等。`netmgr_deinit()` 之后重新 init 会把它放回 1 —— 一个握着旧 epoch 的上报者因此会被判为陈旧，这正是想要的。

### 5.5 后端**不能**引入云头文件

这是这个模块整个形状的由来。netmgr 位于 tuya_iot **下面**，依赖方向是 cloud → netmgr，必须保持这个方向：`tuya_iot.c` 调 `netmgr_conn_set()`，`tuya_lan.c` 和 `ble_mgr.c` 调 `netmgr_conn_get()`，而 netmgr 为了连通性不 include 任何云头文件。

让 netmgr 去调 `tuya_mqtt_connected()` 或读一个 `tuya_iot_client_t` 就会闭合这个环，而这个环不是假想的 —— `netmgr.c` 之所以在 `s_netmgr.lock` 之外发布 `EVENT_LINK_TYPE_CHG`，正是因为 tuya_iot 的订阅者会同步调 `tuya_iot_reconnect()` 回到 netmgr。从 netmgr 内部读云状态，会在同一个环上再加一条更硬的边。

`netmgr_probe_report()` 把方向反了过来：**知道判决的人把它推下来**；netmgr 什么都不 include、什么都不向上调。这也是为什么 API 收的是一个**判决**而不是一个传输句柄 —— **netmgr 必须没有能力去问问题，只能被告知答案。**

内置后端如何在这条纪律下工作，是新后端可以照抄的样板：

- 它的 include 只有 `netmgr_probe.h` 加 `tal_event.h`、`tal_log.h`，就这些。
- `EVENT_MQTT_CONNECTED` / `EVENT_MQTT_DISCONNECTED` 是 `src/tal_system/include/tal_event_info.h` 里的字符串宏，那个文件在 netmgr **下面**，订阅它们不引入任何云 include、不增加任何依赖边 —— `tal_wifi_ulp` 做的是同一件事。所以内置后端**在本模块之外零改动**，这也正是这两个信号被选中、而不是选那些更丰富的信号的全部原因。
- 事件载荷是一个 `tuya_iot_client_t *`。**这个文件绝不解引用它** —— 解引用就需要云头文件，就会闭合那个环。两个回调都完整忽略 `data`（`(void)data;`），而它们确实忽略了，正是"零改动"成立的原因。
- 它本身也**不带 `#if`**：一个空的 translation unit 并不比一个不存在的文件更小，而在文件内部加守卫会静默产出一个符号缺失且没有任何解释的构建。门开在构建系统上，那里可以把它和它的代价写在一起。

新后端如果需要一个 tal 层还没有的信号，正确的做法是**在 tal 层加一个事件名**（`tal_event_info.h` 在 netmgr 下面），让云层去发布它、后端去订阅它 —— 而不是让后端向上 include。

### 5.6 被动默认与主动后端

内置后端**不发任何包**，这是有意的选择而非将就。一个主动探测（DNS 查询、TCP connect、ICMP echo）是显而易见的实现，也是错误的默认：它按 netmgr 选的节奏而不是产品选的节奏消耗射频时间，而这棵树里有两个子系统的全部目的就是不做这件事 —— `src/tal_wifi_ulp/` 把设备在云端往返之间停下来，`src/tuya_pm/` 给唤醒次数做预算。一个每 N 秒唤醒射频去检查一条 ULP 路径刚刚故意让它睡下的链路的探测，**不是特性，是一个起了好名字的回归**。在计费的蜂窝承载上它还要花钱。

设备本来就在不停地做可达性测试，为了它自己的理由：**它在跟云说话**。每个 MQTT CONNACK 都是链路可用的证明，每个 keepalive 超时都是它不可用的证据。复用这些一分钱不花，而且**严格地比合成探测更有信息量**，因为它测的是设备真正需要的那条路径，不是它的代理。

写主动后端时，有两件事被动默认帮你摸清了：

- **`probe_bad_threshold` 是给主动后端的旋钮。** 被动默认下它几乎不可达：每个 BAD 都需要一次会话 teardown，而每次会话建立都发一个 GOOD 把计数清零，所以流是 GOOD/BAD/GOOD/BAD，永远到不了 2。被动路径的降级走的是 `verify_timeout_ms`（`ONLINE --(BAD)--> UNVERIFIED --(timeout)--> DEGRADED`）。能连续吐 BAD 而中间没有 GOOD 的，只有主动后端。
- **被动后端有一类东西看不见**：`mqtt_client_connect()` 失败时关掉 transporter 就返回，压根不调 `on_disconnected`，所以对着一个死 WAN 的重连循环可以完全无声地转下去。主动后端正是能看见这个的东西。

还有一批**明确被否决、不要再翻案**的信号（`netmgr_probe.h` 记着理由）：PINGRESP（coreMQTT 显式跳过应用回调，没有钩子可用，除非改 vendored 库）、PUBACK / SUBACK（都是真正的往返证明，也都是最好的**将来**补充，但各需要一个新事件名并改 `mqtt_service.c`）、`tuya_dev_evt_notify()`（只有 ACTION_BEFORE/AFTER 不带结果码，而且那唯一一个回调槽已经被 ULP 唤醒锁管理器占了）。

---

## 6. 不要这样做：被否决的方案及原因

一个被否决的选项连着它的理由，比推荐做法更省下一个人的时间。以下全部有代码注释在案。

### 6.1 用 linker section 收集驱动

**收益**：驱动自注册，连表都不用写。
**为什么否决**：那个 section 得加到**五个厂商链接脚本**里去（`netconn_registry.h:296-297`）。而这棵树里 vendor 目录是要跟上游同步的，于是每支持一个新平台就要重做一次这件事。

### 6.2 用 weak symbol 做板级覆盖 / 策略钩子

**为什么否决**（`netconn_registry.h:292-296`，`netmgr_policy.h · netmgr_policy_select_cb_set`）：覆盖会住在板级静态库里，而**一个 weak 默认只有在链接器本来就有理由拉入那个 archive member 时，才会输给里面的 strong 定义**。这正是"板子静默回退到默认值"的那种失败 —— 编译过、链接过、启动过，行为却是默认的，而且**没有任何一行日志**。显式调用不可能这样失败。

所以两个扩展点都是显式的函数：`netconn_registry_set_table()` 和 `netmgr_policy_select_cb_set()`。而且这一族"配置调用"都**宁可报错也不静默忽略**：晚于 `netmgr_init()` 的表覆盖返回 `OPRT_COM_ERROR`，晚于 `netmgr_init()` 的探测后端安装也一样。

### 6.3 链路切换时停掉 LAN 服务

看上去最"干净"的做法：路由移到一条没有 `NETCONN_CAP_LAN` 的链路时把 LAN 停掉，回来时再启。**否决，四条独立理由，任何一条都足够**（`netmgr.c` 里 LAN 门控那段的 "What this deliberately does NOT do"）：

1. `tuya_lan_disable()` 会关掉共享 socket loop 上**每一个** reader fd，而 AI monitor 也在用那个 loop，**没有引用计数**。它跑完之后 AI monitor 解引用 `g_sloop == NULL`。
2. `tuya_lan_disable()` 会**阻塞最多 3000 ms**。从这里调它会把系统工作队列停这么久，并且吹掉 `netmgr_deinit()` 那 2000 ms 的 drain 预算。
3. **正确性上不需要**。两个 LAN server socket 都绑在通配地址上，而 LAN 每个包都经 `netmgr_conn_get()` 读一次激活地址，所以一次链路切换会自愈。
4. 那个"干净"的替代方案**今天就是坏的**，采纳它等于用一个能工作的行为换一个崩溃。

所以门控**只管启动**。一台从 cellular 启动的设备压根不打开 LAN 端口，这就是那个 `#if` 想要的全部。

### 6.4 每条链路一个定时器

**为什么否决**（`netmgr_policy.h` 顶部 "The single shared deadline"）—— 而且理由不是省 `TIMER_T` 那 48 字节：

- `tal_sw_timer.c` 没有数量上限，所以每链路一个定时器**不会被拒**，只是会更糟；
- 进程里所有定时器共享一个 `THREAD_PRIO_0` 的 `sys_timer` 线程，`__timer_dispatch()` 串行跑回调。定时器越多，同样的信息要唤醒这个线程越多次 —— 而这正是 ULP 路径要避免的成本；
- 这些 deadline **不是独立的**。任何一个触发的 reselect 都会重新评估全部，所以 N 个定时器会产生 N 次唤醒，而一次就够。

debounce、grace、dwell、probe timeout、revalidation 全部由 `netmgr.c` 里**一个** `tal_sw_timer` 服务，armed 到全部链路中最近的那个。它的回调跑在 `sys_timer` 线程上（**不是** `WORKQ_SYSTEM`），所以它做的事和 LAN 定时器回调一模一样、不多一件：查门、投那个合并的 notify work item、返回。状态机仍然只在一个上下文里跑，**这里没有引入任何新的并发源**。

模块的定时器总数前后都是 2（原来是 LAN 轮询 + wifi 重连，现在是共享 deadline + wifi 重连）—— LAN 轮询定时器被删掉，正好付了新定时器的账。

### 6.5 在 socket 热路径上加锁

见 §3.3。不要给 `tal_net_provider_ops()` / `tal_net_route_src_ip()` 加锁。

### 6.6 让控制面向数据面问控制面的问题

被删掉的那段代码（`netconn_wifi.c:395-408` 记着原文）：

```c
tal_net_provider_id_t active_type = <active provider, read from the data plane>;
if (netcfg.type & TUYA_NETMGR_NETCFG_AP && active_type != TAL_NET_PROVIDER_AT_MODEM)
```

（那个读取入口本身也已经删除，见 §3.3 的锁表；这里保留原文的形状是为了说明它错在哪。）

意图是"4G 是激活链路时不要开 AP 配网"。它同时犯了两个错：

- **架构上**，这是控制面向数据面问一个控制面的问题；
- **事实上，它是死代码**。`route.provider` 今天的写入者只有 `tal_net_route_set()`（由 netmgr 从 `conn->provider` 喂）；当年还有一个半更新入口，但它全树零调用者，现已删除。而每个驱动都把 `provider` 设成 `TAL_NET_PROVIDER_DEFAULT`，后者展开成 `TAL_NET_PROVIDER_POSIX` 或 `TAL_NET_PROVIDER_TKL`，**永远不会是** `TAL_NET_PROVIDER_AT_MODEM` —— 全树提到这个常量的只有它自己的 `#define`。所以这个条件是恒真式，**它守着的那个 4G 分支一次都没走过**。

替代品是 `NETCONN_CAP_NETCFG_AP`：驱动自己声明能不能拉 AP，而 netmgr 永不向数据面问控制面的问题。这个位今天是行为中性的（wifi 那一行置了它），它的价值在于**一个拉不起 AP 的模组现在只要清掉描述符里一个位就能关掉这个分支** —— 这是原本的意图第一次真正可执行。

### 6.7 为一个做不到的操作返回 `OPRT_OK`

见 §2.5。`netconn_cellular_set(NETCONN_CMD_CLOSE)` 曾经调那个 no-op 的 close 然后回 `OPRT_OK`，对 `tuya_iot_destroy()` 说链路已关而它还开着。**不方便但真实的 `OPRT_NOT_SUPPORTED` 胜过方便的谎话**，因为后者是调用者无法据以行动的。

同一条纪律的另一面：`netmgr_deinit()` 不再丢弃 `conn->close()` 的返回值，而是按 `ctrl` 分级解释它（§2.4）—— 丢弃返回值只是把谎话搬了个地方，没有除掉它。

### 6.8 给策略加"每个旋钮一个命令"的 API

**为什么否决**（`netmgr_policy.h · netmgr_policy_t`）：一次调用设整个策略，`netmgr.c` 读到的才是一组**一致的**参数；每旋钮一个 API 会允许一次 reselect 插在两个相关写入之间。

### 6.9 给 `netmgr_conn_config_type_e` 加一个通用 "connect" 动词

**为什么否决**（`netmgr_policy.h · netmgr_policy_pin`）：加了这个动词，**每个**驱动都要长出一个对应分支，而三个里有两个只能回答 `OPRT_NOT_SUPPORTED` —— OBSERVE 和 SUSTAINED 链路根本没有可拨的东西。所以 `netmgr switch wifi` 在 wifi 没起来时只会 arm 一个 pin，别的什么都不做；要拨号用 `netmgr wifi up <ssid>`。

pin 的语义也照这个诚实性设计：pin **压过**优先级、分层、粘滞和 dwell（操作员掌握的上下文比它们任何一个都多，包括使用一条 netmgr 认为已降级的链路的权利），但**不覆盖资格底线**。pin 一条 down 的链路不会让流量从它出去，所以 pin 被**记住**，等链路起来时再生效，`netmgr_policy_pin()` 用返回值区分这两种情况（`OPRT_OK` = 已 arm 且现在就够格；`OPRT_RESOURCE_NOT_READY` = 已 arm 但还不能载流量，**不是失败**），这样 CLI 能把话说清楚。

### 6.10 让驱动上报"我开始尝试连接了"

**为什么否决**（`netmgr.c` 里状态机那段）：没有驱动会报这个 —— `netconn_wifi.c` 只在 `WFE_CONNECTED` 和失败时打 `base.event_cb()`。发明一个新的上报动词意味着改 `netconn_wifi.h`，而它在全局公共 include 路径上。`NETMGR_LINK_STATE_CONNECTING` 因此是 netmgr 从它已有的输入推出来的，不是驱动报上来的。

---

## 7. 速查

### 7.1 文件

| 文件 | 职责 | 能出现技术名吗 |
|------|------|----------------|
| `netconn_registry.h` | 描述符契约、caps、ctrl、attr mask、注册表 API、`netmgr_notify_link()` | 不能 |
| `netconn_table.c` | 默认表 + 板级覆盖存储 | **唯一允许 `#ifdef ENABLE_<TECH>` 的文件** |
| `netconn_<tech>.c/.h` | 驱动 | 就是它自己 |
| `netmgr.c` | 注册、每链路状态机、排序调度与校验、route 推送、事件、LAN 门控 | 不能（只在注释里） |
| `netmgr_policy.c/.h` | 策略参数、内置排序、pin | 不能 |
| `netmgr_probe.c/.h` | 可达性契约 + 内置被动后端 | 不能 |
| `netmgr_retry.c/.h` | 退避算术 | 不能 |
| `netmgr_cli.c` | `netmgr` 命令，泛化于注册表 | 不能 |
| `netmgr_priv.h` | 模块内部 API（`netmgr_link_info_t`、`netmgr_reselect_request()` 等） | 不能 |
| `netmgr_event.h` | 事件契约 | 不能 |
| `tal_net_provider.c/.h` | socket 后端表 + route 状态 | 不能 |
| `tal_net_route.h` | route 契约 | 不能 |
| `tal_posix.c` / `tal_tkl.c` | 两个 socket 后端 | 就是它自己 |

CMake 侧（`src/tuya_cloud_service/CMakeLists.txt`）：`netmgr.c`、`netconn_table.c`、`netmgr_cli.c`、`netmgr_policy.c`、`netmgr_retry.c` **无条件**编译；`netconn_wifi.c` / `netconn_wired.c` / `netconn_cellular.c` 按各自的 `CONFIG_ENABLE_*` 门控；`netmgr_probe.c` 按 `CONFIG_ENABLE_NETMGR_PROBE` 门控。

`netconn_table.c` 无条件的理由是它**就是**注册表本身：行在文件内部按技术 `#ifdef`，所以哪个链路驱动都不开时这个文件照样编得过。策略和退避两个模块无条件的理由比"方便"更强：**`netmgr.c` 无条件引用它们，gate 掉得到的是未定义引用而不是更小的镜像**。（`netmgr_probe.c` 能被 gate 掉，正是因为 `netmgr.c` 对 `netmgr_probe_backend_mqtt` 的那一处引用带着配套的 `#if`。）

### 7.2 四个扩展点

| 想加什么 | 用什么 | 时机限制 |
|----------|--------|----------|
| 一种链路技术 | 新 `netconn_<tech>.c` + `netconn_table.c` 一行 | 编译期 |
| 一张板级链路表 | `netconn_registry_set_table()` | **`netmgr_init()` 之前**，闩死 |
| 一个 socket 后端 | 新 `TAL_NET_PROVIDER_*` + provider + 静态初始化器条目 + 某行的 `.provider` | 编译期 |
| 一套选路排序 | `netmgr_policy_select_cb_set()` | 随时；在 `s_netmgr.lock` 下被调 |
| 一组策略参数 | `netmgr_policy_set()` | 随时；不闩 |
| 一个可达性后端 | `netmgr_probe_backend_set()` | **`netmgr_init()` 之前** |
| 手动选链 | `netmgr_policy_pin()` / `netmgr switch <name>` | 随时；不覆盖资格底线 |

### 7.3 链路数量与类型数量的上限

`NETMGR_LINK_MAX` 是 8（`netmgr.c`），超过就注册失败并报 `PR_ERR`。`netmgr_type_e` 是位掩码，所以类型数受 32 位限；`netconn_attr_mask_t` 那个编译期断言限制的是**命令**数而不是类型数。

---

## 8. 注释与代码的一致性

写这份指南时逐条核对了注释，查出六处不符。其中**五处已在 `db9bd74a` / `bde2bc30` 修掉**，只剩 §8.2 一条仍然成立。已修的仍然记在这里：§8.1 单列，因为它修掉之后还留下一条需要知道的规则；其余四处并成 §8.3 的清单。留着是因为旧措辞可能还留在别处的引用或分支里。

### 8.1 `NETMGR_TYPE_TO_STR()`：已修，但那个宏不会消失

原先的问题：`netmgr_cli.c` 用这个宏打链路名，而宏只枚举了树里三种技术，其余一律 `unknown`。于是板级通过 `netconn_registry_set_table()` 加的链路会被打成 `unknown` —— 连 `netmgr switch <newlink>` 的确认信息也是，尽管这条命令刚刚**按名字**从注册表里把它解析出来。

现在 `netmgr_cli.c` 走 `__netmgr_cli_type_name()`：有描述符就用 `desc->name`，没有才回落到宏。所以**加一条链路不必碰 `NETMGR_TYPE_TO_STR()`**，§2.1 第 5 行已相应改掉。

但那个宏保留，而且必须保留 —— 有两处调用点命名的是**故意不在注册表里**的 type，根本没有描述符可取名：

- `netmgr_policy.c` 里 `netmgr_policy_pin()` 拒绝一个未注册 type 的那一支，它正是 `netconn_registry_find()` 返回 NULL 才会走到；
- `netmgr_cli.c` 里 `netmgr switch` 的 `OPRT_NOT_FOUND` 一支，即"查到名字之后、pin 之前链路被拆掉"的竞态。

两处都在注释里写明了原因，免得下一个人"把活干完"。

### 8.2 `NETCONN_CAP_METERED`：内置排序仍然不读它（**唯一未修项**）

这一条是真缺口，不是文档问题，所以留在这里。

`netmgr_policy.c` 里 `caps` 一次都没有出现 —— 内置排序看不见能力位。`netconn_registry.h` 原来声称"M3 的选路策略用这个位把 metered 链路当兜底而不是对等选项"，那句话是错的，已经改成实话并写清了要实现需要动哪里。

现状是三件事：置位在 `netconn_table.c` 的 cellular 行上；`netmgr_cli.c` 会把它显示出来；它通过 `netmgr_link_view_t.caps` 交给产品排序钩子，所以**板级可以自己实现**这个语义而不必改 netmgr。

树里表达"cellular 只做兜底"用的是 `default_pri`（cellular 0 / wifi 1 / wired 2）。这能用，但那是优先级而不是能力：它不携带任何计费含义，而且板级一旦重排优先级，这层意图就悄悄消失了。要真正实现，`netmgr_policy_select_default()` 需要在优先级比较之上多一个档位；"活跃链路是 metered 时压制可选流量"那半则需要今天只问"有没有网"的那些调用方配合。

### 8.3 已修：三处注释与一处代码

留清单而不是删掉，因为旧措辞可能还留在别处的引用或分支里。

| 原问题 | 现状 |
|---|---|
| `netmgr_probe.h` 说默认被动后端"做不到跨自己的网络操作保存值"，只能传 `NETMGR_PROBE_EPOCH_ANY` | 错的，而且是**低估**了代码。`netmgr_probe.c` 有 `s_session_epoch`，在 CONNACK 时读下 epoch，断连时带着那个会话的 epoch 上报；只有"没见过 CONNACK 的断连"才回落 `EPOCH_ANY`。归属窗口比原注释说的窄得多 —— 低估一个防护会引诱下一个人去放宽它，所以这条也算错 |
| `netconn_registry.h` 说 `netmgr_notify_link()` "替代了 `base.event_cb`" | 语义成立、机制不成立：树里三个驱动没有一个调它，都还在调 `base.event_cb`，而注册时那个指针被指向 `netmgr.c` 里一行的 shim。这层间接是有意的迁移杠杆（换线程模型没动任何驱动），但它意味着被实际走的仍是老路径，所以**新驱动应当直接调 `netmgr_notify_link()`**，不要照抄现有驱动的 `event_cb` 写法 |
| 三个驱动的 `NETCONN_CMD_PRI` 分支调 `base.event_cb` 不判空，同文件状态路径都判 | 三处都补上了。经 `netmgr_conn_set()` 进来时不可达，但这些 `netconn_*_set()` 在全局公共 include 路径上 —— "今天没人这么调"不等于"没人能这么调" |
| `netmgr_probe_backend_set(NULL)` 被说成可以做出"根本不链接后端"的构建 | 做不到：传 NULL 是调用点，调用点做不了链接期决定。这件事现在归 `ENABLE_NETMGR_PROBE` 管，它把 `netmgr_probe.c` 从构建里去掉。§5.2 已按修正后的语义写 |
