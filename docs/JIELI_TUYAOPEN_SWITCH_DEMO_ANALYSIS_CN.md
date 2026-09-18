# TuyaOpen × Jieli AC7916A/WL82 switch_demo BLE+WiFi+Cloud 接入问题独立调研报告

**调研性质**：只读调研（调研过程未修改任何代码、未编译/烧录/启停串口、未执行任何 git 写操作）
**调研日期**：2026-09-17 ｜ **归档日期**：2026-09-18 ｜ **工作区**：`D:\tuya_proj\jieli`
**日志基线**：`apps\tuya_cloud\switch_demo\monitor_capture.log`（对应固件编译时间 Sep 17 2026 20:59:19）
**敏感信息处理**：uuid/authkey/PID/MAC 均已打码（保留首尾 3~4 字符）

---

## 1. 标题与调研范围

调研对象为 TuyaOpen `switch_demo`（v1.0.0）在杰理 WL82/AC7916A 上的三个失败域：

- **BLE 连接后 Tuya 协议收发失败**（`att_send need_size >= 6+1+188`、`ble packet len err:26440`）；
- **WiFi 配网链路不完整**（AP IP/DHCP 未配置、空 SSID 自动重连循环）；
- **云端激活阻塞**（license 读取失败、激活配置缺失）。

调研输入：

- 最新日志 `apps\tuya_cloud\switch_demo\monitor_capture.log`（612 行，完整通读）；
- 当前适配 `platform\JIELI\tuyaos\tuyaos_adapter\src\tkl_bluetooth.c`、`tkl_wifi.c`、`tuyaopen_license.c`、`jieli_build.py`；
- TuyaOpen 公共层 `src\tal_bluetooth\src\tal_bluetooth.c`、`src\tuya_cloud_service\ble\{ble_mgr.c, ble_protocol.h, ble_trsmitr.c, ble_cryption.c}`、`src\tuya_cloud_service\netcfg\ap_netcfg.c`；
- 厂商 SDK `platform\JIELI\AC79_AIoT_SDK`（AC79NN_SDK_V1.2.13_2026-04-20）关键头文件与 BLE 示例；
- 参考工程 `ipc_ac7916a`（曾跑通，经只读子代理调研）；
- 其他平台 `Z:\tyopen\TuyaOpen\platform`（本地实际存在 ESP32/LINUX/T3/T5AI；T2/LN882H/BK7231X 等仅 yaml 记录未下载）。

注：交接文档中 `monitor.log`、COM11、"17:39 旧日志"等表述已过时——当前工作区只有 `monitor_capture.log`（用户确认当前日志口为 COM3），"BLE 曾配网成功"的旧日志已不存在，**无法用现存文件复核该历史结论**。

---

## 2. 当前日志事实（monitor_capture.log，逐条可复核）

| # | 行号 | 事实 |
|---|------|------|
| F1 | 5, 25 | `WL82(AC791N) CHIP_ID 0x6f01, setup_arch Sep 17 2026 20:59:19`；SDK `AC79NN_SDK_V1.2.13_2026-04-20` → 日志对应最新构建 |
| F2 | 118–126 | switch_demo 1.0.0 / wl82 / AC7916A，与交接一致 |
| F3 | 95–99 | EDR MAC `74:cf:d4:**:**:**`（flash 读取有效）；BLE MAC 初始 `FF:FF:FF:FF:FF:FF`，由 `tkl_ble_stack_init` 内 `lib_make_ble_address` 修正（tkl_bluetooth.c:655-658） |
| F4 | 104 | 厂商 syscfg 默认 STA 配置 `wifi ssid config:GTSWIFI`（pwd 空）——实际重连日志显示 OID SSID 为空（len 0），未用 GTSWIFI |
| F5 | 127 | `lfs.c:1347 Corrupted dir pair {0x0,0x1}` → KV/littlefs 分区首刷为空，自动 format |
| F6 | 145–149 | `reset_netcfg.c:47/102 ret:-6`（-6=`OPRT_NOT_FOUND`，tuya_error_code.h:47）——首刷 KV 无键，正常 |
| F7 | 150–155 | `tuyaopen_license_read read failure`（平台 stub）→ 走 `tuya_config.h` 编译宏 fallback（uuid=`uuid****b98c` 20字符、authkey=`2MF****u5f8` 32字符，格式完整、有效性未知） |
| F8 | 158–165 | `use default rcs -6` / `psk config not available` / `tal_kv_get region fail:0xfffffffa(-6)` / `activate config not found:-6` → 全部为"KV 无数据"型首刷正常降级 |
| F9 | 266–267 | `get_flash_mac_addr valid` + `wifi use flash_uid+random mac` → WiFi MAC 非全零 ✓ |
| F10 | 281 | 启动时 `skip do auto reconnect...`（模块初始未带凭据） |
| F11 | 285–291 | LwIP 启动，`ip4 address: 192.168.1.1`（厂商库默认，早于 AP 配网） |
| F12 | 350–351 | `save_wifi_cur_info AP_MODE, ssid=SmartLife-B7CB` → 厂商 AP 模式进入成功 |
| F13 | 371–381 | ap_netcfg TCP 线程启动、bind 6668；`netcfg start 0x1(AP) ret:0`、`0x2(BLE) ret:0` → BLE+AP 双路配网均启动 |
| F14 | 378–380 | `LL Adv already destroy` + `[JIELI] BLE advertising enabled` + `ble_mgr.c:379 ble adv updated 0` → BLE 广播成功 |
| F15 | 391–445 | 每 ~2.7s 循环：`netlink not startup` → `put_ncmsg 13` → `_rpc_RtmpMlmeTask` create/kill → `Driver auto reconnect ... SSID - , len - 0` → `CntlOidSsidProc No matching BSS, start a new scan`（贯穿全日志，含 BLE 会话期间与之后） |
| F16 | 448–455 | 24.192s `CONNECT_REQ` → 24.208s `HCI_SUBEVENT_LE_CONNECTION_COMPLETE 0` → **24.220s `att_send need_size >= 6+1+188`** → `ble_mgr.c:964 Ble Connected` → LL 建链成功 |
| F17 | 459–534 | 大量 `[LE_BB]conn nack`（连接参数更新协商期间持续） |
| F18 | 539–541 | `ble_mgr.c:240 ble recv sub_pkg desc:3, no:1, pack_len:33, total_len:33` → `ble_mgr.c:311 ble packet len err:26440` → `ble_mgr.c:993 tuya_ble_data_proc fail. -2` → 手机写入数据完整到达应用层，但解密后长度字段为乱码 |
| F19 | 543–547 | 26.364s `LL_TERMINATE_IND reason:13`（手机侧主动断开，距首包失败 ~1.4s，符合 App 等响应超时特征）→ `att disconn` |
| F20 | 548–612 | 断开后 F15 循环继续；全日志无 STA 连接尝试、无 DHCP、无 MQTT/激活日志（BLE 协议层失败，配网从未拿到 SSID/token） |

---

## 3. 当前架构与完整数据链路

```
手机App(TuyaBLE GATT client)
 └─ATT Write→ 0x0006(write char,UUID128 V2) ; CCCD 0x0009 订阅 0x0008(notify)
Jieli Controller(btctrler@CPU0) → btstack(#C0btstack task)
 └─att_write_callback(connection_handle, att_handle, transaction_mode, offset, buffer, buffer_size)
TKL  platform\JIELI\...\tkl_bluetooth.c:226  jieli_ble_att_write_callback()
 └─零拷贝直传: report.p_data=buffer, length=buffer_size (:262-263) → s_gatt_callback 同步回调(btstack线程)
TAL  src\tal_bluetooth\src\tal_bluetooth.c:333  TKL_BLE_GATT_EVT_WRITE_REQ → TAL_BLE_EVT_WRITE_REQ
 └─ble_mgr.c:1064 tal_ble_event_on_worq → ble_event_msg_copy(:900 深拷贝p_data) → WORKQ_HIGHTPRI(workq线程)
应用 src\tuya_cloud_service\ble\ble_mgr.c:981  TAL_BLE_EVT_WRITE_REQ
 └─ble_packet_recv(:276): trsmitr解码(:258 去变长头≥3B) → raw_buf=[mode(1)|IV(16)|cipher(N)]
    → tuya_ble_decryption(ble_cryption.c:289, AES-128-CBC,KEY_11=MD5(authkey32+uuid16+IV))
    → dec_buf=[SN(4)|ACK_SN(4)|CMD(2)|LEN(2,BE)|DATA|CRC16(2)] → ble_session_*
    发送反向: ble_packet_encode(:507 CBC加密) → trsmitr组包 → tal_ble_server_common_send(tal:895)
    → tkl BLE_CMD_ATT_SEND_DATA(tkl:959)
配网 src\tuya_cloud_service\ble\ble_netcfg.c:34  JSON{ssid,pwd,token} → netcfg_finish_cb
 └─netconn_wifi.c:248 __netconn_wifi_netcfg_finish: KV"netinfo" + EVENT_LINK_ACTIVATE
    + netcfg_stop(all) → ap_netcfg.c:931 ap停+切STA → __netconn_wifi_connect(:64)
TKL WiFi tkl_wifi.c:422  wifi_enter_sta_mode() (前置 set_work_mode(WWM_STATION):349)
Jieli WiFi 闭源 wl_wifi_*.a → 事件 wifi_event_callback → tkl_wifi.c:117 WFE_CONNECTED(仅DHCP_SUCC)
Cloud  token激活(tuya_iot.c STATE_TOKEN_PENDING→ENDPOINT_UPDATE→ACTIVATING:1018)
 └─iotdns(需region,来自token) → atop activate(PID+uuid+authkey) → MQTT
DP  TUYA_EVENT_DP_RECEIVE_OBJ(tuya_main.c:141) → 仅回显上报(tuya_iot_dp_obj_report:180)
 └─【无 GPIO/继电器/按键/独立DP上报代码——switch_demo src/ 下只有 tuya_main.c】
```

| 层 | 入口 | 出口 | 数据格式/长度字段 | 缓冲区所有权 | 线程 | 主要风险点 |
|---|---|---|---|---|---|---|
| Jieli ATT | `att_write_callback` | `ble_op_att_send_data` | ATT payload 直传 | 厂商栈 buffer，回调内有效 | btstack(#C0btstack) | long-write 的 offset/transaction_mode 被忽略（tkl:232,267）——当前 MTU≥39 未触发 |
| TKL BLE | `jieli_ble_att_write_callback` tkl:226 | `tkl_ble_gatts_value_notify` tkl:962 | TKL 事件原样透传 | 零拷贝引用 | btstack 同步 | ATT 发送上下文参数错误（见 §4） |
| TAL | `tkl_ble_kernel_gatt_event_callback` tal:204 | `tal_ble_event_callback` | 转换字段名 | 入 workq 前深拷贝（ble_mgr:910-918）✓ | btstack→workq | — |
| ble_mgr RX | `ble_packet_recv` :276 | sessions :999 | LEN 大端（:308-309 读，:534-535 写，两侧一致） | raw_buf/dec_buf 静态 1024B | workq | CBC 无 MIC：错钥静默乱码（:311 现象） |
| ble_mgr TX | `ble_packet_resp` :581 | `tal_ble_server_common_send` | trsmitr 子包≤`ble_frame_packet_len`(初值1024:trsmitr.c:27，App QRY 后重设) | tal_malloc | workq(+20ms/包 sleep) | 子包长可能超 ATT 发送 payload 容量 |
| netcfg→WiFi | `ble_netcfg __handle_net_cfg` | `wifi_enter_sta_mode` tkl:428 | JSON | — | workq/netmgr | 停 AP 走 `wifi_off()`（见 §5） |
| WiFi 事件 | `jieli_wifi_event_cb` tkl:109 | `WFE_CONNECTED`(DHCP_SUCC) | — | — | 厂商回调线程 | DHCP_TIMEOUT 映射 DISCONNECTED（T5AI 映射 CONNECT_FAILED，次要差异） |

---

## 4. BLE 问题定位

**结论先行：BLE 建链成功、数据完整上行，存在两个相互独立的阻塞点——(A) ATT 发送通道初始化不完整导致设备完全无法 notify；(B) 解密乱码导致收包解析失败。A 有高置信度源码证据；B 的结构证据完整、根因（密钥失配）待一项诊断确认。**

### Q1 ATT 写回调 buffer/buffer_size 语义是否与 TAL/TKL 期望一致？

一致。参考工程同样零拷贝上抛（`ipc_ac7916a\tuya_os_adapter\src\driver\tuya_os_adapt_bt.c:308-310` `data.data=buf; data.len=buf_size`）；TAL 在入 workq 前深拷贝（ble_mgr.c:910-918），引用不会悬空。日志 F18（单包 33B subpackage 完整、total 一致）证明数据无截断、无重复、无错序。

### Q2 profile UUID/handle 是否与协议一致？

完全一致。当前 V2 静态表（tkl_bluetooth.c:118-148）与曾跑通的 `le_net_cfg_tuya.h:8-47` 逐项相同：Service 0xFD50@0x0004、write UUID128@0x0006、notify UUID128@0x0008、CCCD 0x2902@0x0009、read UUID128@0x000B。**排除 profile 不符假设**。

### Q3 默认 V1 还是 V2？

V2，且正确：TAL 默认 `TAL_BLE_SERVICE_VERSION=2`（tal_bluetooth.c:37-39,431-482），`tkl_ble_gatts_service_add`（tkl:879-914）已按上层传入 UUID 动态匹配 V1/V2 并回填固定 handle。无需改动。

### Q4 固定 handle 可靠吗？

可靠——静态 profile 表与 handle 宏同文件维护、由同一 `att_server_init` 解析，参考工程同构。仅需在维护约定中注明"改表必须同步改 handle 宏"。

### Q5 是否有等价 `ble_op_att_send_init`？连接/断开是否分别初始化/释放？

有且成对：连接 `BLE_CMD_ATT_SEND_INIT(handle, s_att_ram_buffer, sizeof, 128)`（tkl:550-552；与 `ble_api.h:541-542` 官方宏完全等价），断开零参释放（tkl:486，与参考 le_net_cfg_tuya.c:353 一致）。**但参数与参考不同，且缺配套调用（见 Q6）——这是阻塞点 A 的核心。**

### Q6 参数对比（当前 vs 曾跑通参考 `le_net_cfg_tuya.c`）

| 参数 | 参考（跑通） | 当前 tkl | 判定 |
|---|---|---|---|
| ATT payload（init 第4参） | `ATT_LOCAL_MTU_SIZE=200`（:63,:267） | `JIELI_ATT_LOCAL_PAYLOAD_SIZE=128`（tkl:47） | **不一致** |
| ATT_SEND_CBUF_SIZE | 512（:65） | 512（tkl:48） | 一致 |
| att_ram_buffer | 188+200+512=900（:68-69） | 188+128+512=828（tkl:49-52） | 跟随 payload |
| `ble_vendor_set_default_att_mtu(200)` 栈初始化 | **有**（:611） | **无**（全文 grep 无） | **缺失** |
| `ATT_EVENT_MTU_EXCHANGE_COMPLETE → ble_op_att_set_send_mtu(mtu-3)` | **有**（:372-377） | **无**（att packet handler 为空，tkl:162-168） | **缺失** |
| notify 发送 | `ble_op_att_send_data`+先查 CCCD(`att_get_ccc_config`)+查余量(`ble_op_att_get_remain`)（:472-494） | `BLE_CMD_ATT_SEND_DATA` 直发（tkl:951-960） | 可补强 |
| CCCD 写 | `att_set_ccc_config(h, buffer[0])`（:465） | 2 字节小端解析（tkl:236-238） | 两者皆可 |
| long write | 不处理 | 不处理 | 与参考持平 |

### Q7/Q8 两个错误的含义与定性

**`att_send need_size >= 6+1+188`**：188 与 6 正是闭源 btstack 库的固定常量 `ATT_CTRL_BLOCK_SIZE(188)`、`ATT_PACKET_HEAD_SIZE(6)`（`le_common_define.h:54-55`，注释 "fixed, libs use"；库符号 `att_send_need_bufszie/att_need_ctrl_ramsize/payload_min` 佐证存在最小容量校验）。错误出现在连接完成后 12ms（24.220），**早于手机首个数据写入（24.9）**，与 App 数据无关，指向 ATT 发送模块初始化/MTU 处理。buffer 总量 828≥195 本身不会触发该校验，最可能触发是：手机 MTU 协商后有效 ATT 载荷达 188（≈MTU 192-4），而发送模块按 128 注册 → 校验失败 → **所有后续 notify 被拒**。参考实现正是用"默认 MTU 200 + payload 200 + 协商后 set_send_mtu"三件套规避。精确判定条件在闭源库内（推断，置信度高）。**它是根因（阻塞点 A），不是连锁现象。**

**`[LE_BB]conn nack`**：字符串只存在于闭源库（全 SDK 源码零命中），是 Link Layer/baseband 层"连接事件上的数据 PDU 被对端 NACK"。时序上始于 ATT init 失败之后、贯穿连接参数更新与发现阶段，且在唯一一次收包失败后手机 1.4s 即主动断开（reason 13）。判定为**连锁现象**；其与 WiFi 空 SSID 扫描循环的射频共存关系（WL82 WiFi/BT 共存）为待验证假设，非本次根因。

### Q7 pack_len=33 → len err 26440 推算

- 33 字节 = `1(mode) + 16(IV) + 16(密文)`，密文 16 字节恰为未绑定设备首包 `FRM_QRY_DEV_INFO_REQ`（明文 SN4+ACK4+CMD2+LEN2+DATA2(PacketMaxSize)+CRC2=16，无需 padding）——**帧结构与长度完全吻合标准 Tuya BLE 加密帧**；
- `ble_packet_trsmitr` 解码成功（desc:3=END 单包、version≥2、total 一致）→ 排除"非完整帧/截断/重复/拼接错误/长度偏移错（偏移为编译期常量）/大小端错（编解码同为大端）"；
- `tuya_ble_decryption` 返回 0（否则会打印 `ble packet decrypt err`）但输出乱码：AES-128-CBC **无完整性校验**（ble_cryption.c:325），错钥必然静默解出乱码；`dec_buf[10..11]=0x67,0x68` → 26440；
- **最可能根因：KEY_11=MD5(authkey‖uuid‖IV)（ble_cryption.c:48-55）两侧不一致**——设备侧 uuid/authkey 是 `tuya_config.h` 编译宏（license stub 的 fallback），App 侧来自云端按 PID 下发的设备凭据；若该三元组未在涂鸦云注册/与 PID 不配套，两侧密钥必然不同。次要候选：App 首包加密模式非 0x11（无法从日志确认 mode 字节值）。**待一项诊断定案：开启 `tuya_ble_enable_debug(true)`（ble_mgr.c:125-128）抓 raw/dec 十六进制 dump，或换入有效授权后复测。**

### Q9 ipc_ac7916a BLE 能否直接移植？

- **可直接借鉴**：ATT 三件套参数与时机（Q6 表）、`ble_vendor_set_default_att_mtu`、`ATT_EVENT_MTU_EXCHANGE_COMPLETE` 处理、CCCD/发送余量检查模式、断开零参释放；
- **不能直接复制**：旧 TuyaOS 回调接口（`tuya_ble_write_callback`/`TY_BT_EVENT_*`）、闭源 `libtuya_iot.a` 内的协议解析（参考工程适配层本身无 0x55AA 解析，协议全在闭源库——而 TuyaOpen 的 `ble_mgr.c` 是开源等价物，已存在）、旧 SDK lwip 2.1.3 的常量；
- **需经 TKL/TAL 桥接**：厂商事件→`TKL_BLE_GAP/GATT_EVT_*`、notify→`tkl_ble_gatts_value_notify`——当前 tkl 骨架已正确完成桥接，只差参数与两处调用。

---

## 5. WiFi 问题定位

**结论先行：AP 配网"半通"（AP 起来了、TCP 6668 起来了，但 IP/DHCP 未按 netcfg 要求配置）；BLE 配网路径未走到 STA（被 BLE 阻塞点截断）；空 SSID 自动重连循环是"热切换未重武装驱动"造成的噪声，已证不阻塞 BLE，对 STA 的影响未验证。**

### Q1 `tkl_wifi_start_ap()` 完整性核对（tkl_wifi.c:239-252）

| 项 | 状态 | 证据 |
|---|---|---|
| SSID/password | ✓ | F12：`AP_MODE, ssid=SmartLife-B7CB`（名称由 ap_netcfg.c:832 按 MAC 生成） |
| 信道 | ✓ | `wifi_set_channel(cfg->chan)`（chan=6，ap_netcfg.c:835） |
| IP/网关/掩码 | **✗** | **完全忽略 `cfg->ip`**；ap_netcfg.c:825-830 要求 **192.168.176.1**，设备停在厂商默认 192.168.1.1（F11）。参考实现在进 AP 前经 `wifi_set_lan_setting_info`→`net_set_lan_info` 写入（tuya_os_adapt_wifi.c:243-314,351） |
| DHCP server | **✗（未显式）** | 厂商闭源栈推测按 `lan_setting` 自动起 DHCP（dhcp_srv.c:503-520 使用 SERVER_IPADDR/CLIENT_IPADDR；参考 ap_qlink.c:315 注释），当前从未设置 → 手机连 AP 能否拿到 IP 未验证 |
| netcfg HTTP/TCP server | ✓ | ap_netcfg bind 6668（F13）；但 AP IP 不符时手机侧入口不可达 |
| MAC | ✓ | F9：flash_uid+random，非全零 |
| AP/STA 模式切换 | ⚠ 热切换 | 直接在活模块上调 `wifi_enter_sta/ap_mode`；参考是 `wifi_off→set default_mode(AP,force=1)→wifi_on` 重武装（tuya_os_adapt_wifi.c:317-369,504-517） |
| 自动重连 | ⚠ 不完整 | 只调了 `wifi_set_sta_connect_best_ssid(0)`；参考用 `connect_best_network=0`（wifi_store_info 字段）+ default_mode 重武装，并设 `wifi_set_sta_connect_timeout(10)`、`wifi_set_smp_cfg_timeout(180)`、AP 后 `wifi_rxfilter_cfg(1)` |

### Q2 work_mode/station_connect/get_status 与厂商状态机匹配度

`set_work_mode` 仅记录状态（SOFTAP no-op/POWERDOWN wifi_off/STATION wifi_on，tkl:339-358）——可接受；`station_get_status` 映射合理（DHCP_SUCC→GOT_IP）；`WFE_CONNECTED` 仅在 DHCP 成功上报（tkl:117-121）——与 ESP32（GOT_IP）、T5AI（GOT_IP4）跨平台惯例一致 ✓。次要差异：DHCP_TIMEOUT 映射 DISCONNECTED（T5AI 映射 CONNECT_FAILED）。

### Q3 BLE 配网拿到 SSID/pwd 后是否一定进入 STA？

调用链完整（源码核实）：`ble_netcfg.c:103 → netcfg_finish_cb → netconn_wifi.c:248-279（KV 持久化+EVENT_LINK_ACTIVATE）→ netcfg_stop(all) → ap_netcfg.c:931-935 → tal_wifi_ap_stop()`。**风险：`tkl_wifi_stop_ap`=`wifi_off()`（tkl:255-258），`station_disconnect` 同样 `wifi_off()`（tkl:431-434）**——每次模式切换整体断电重启 WiFi 模块（重建 RtmpMlme/RtmpCmdQ 任务）。参考实现两者均为 **no-op**（tuya_os_adapt_wifi.c:1225-1228,1345-1349）。链路能走通但属于非参考行为，稳定性待验证。

### Q4 参考工程有而当前缺失的逻辑（按影响排序）

① `net_set_lan_info`（AP IP/DHCP 池）；② `wifi_set_default_mode(AP,force=1)` 重武装 + 模块重启式进 AP；③ 相同 SSID/pwd 短路返回（:1313-1317）；④ station_connect 无保存配置时的整体重启流程（:1293-1304）；⑤ 超时参数与 `wifi_rxfilter_cfg`。

### Q5 空 SSID 自动重连的影响（四选一作答）

**"AP 模式下仍然运行 STA 自动重连造成的（现阶段）无害噪声 + 潜在干扰源"。** 已证：不阻塞 BLE 连接（F16 发生在循环中）、不阻塞 AP/TCP 起服；未证：对后续 `wifi_enter_sta_mode` 的影响（日志无 STA 尝试）、与 `conn nack/LL delay` 的射频共存关联。机制：当前热切换未重武装驱动，闭源 STA 默认状态机带着空 OID SSID 每 2.7s 扫描（`CntlOidSsidProc` 等字符串仅存于 `wl_wifi*.a`，源码不可见）。**不会"实际阻塞"配网数据面，但应在 Phase 2 按参考方式解除。**

### Q6 证明 WiFi 完成的最小日志集

1. `ble recv req type 0x0000/0x0001`（ble_mgr:997，已有）＋ ble_netcfg token 解析日志；2. `tkl_wifi_station_connect` 入口（**需新增**，打印 ssid 长度，勿打印明文密码）；3. 厂商 `wifi_enter_sta_mode`/`WIFI_EVENT_STA_CONNECT_SUCC`；4. `WIFI_EVENT_STA_..._DHCP_SUCC`；5. `WFE_CONNECTED` 后 netmgr 上线日志；6. `iotdns`/endpoint + `tuya_iot` STATE 流转 + MQTT connected + 激活成功日志。

---

## 6. Tuya Cloud 激活问题定位

### Q1 是源码没提供授权还是 Flash/KV 读取失败？

**两者皆有但按设计如此**：平台钩子 `tuyaopen_license_read` 是恒返 `OPRT_NOT_SUPPORTED` 的 stub（tuyaopen_license.c:3-19，jieli_build.py:343 编入）；KV 键 `UUID_TUYAOPEN/AUTHKEY_TUYAOPEN` 首刷为空（-6）。实际凭据来自 `tuya_config.h` 编译宏 fallback（tuya_main.c:260-265）——这是 TuyaOpen 官方认可的 demo 授权路径（ESP32/LINUX 的 license 钩子同样返回 NOT_SUPPORTED）。

### Q2 fallback 能否支持云端激活？

**无法从仓库证明。** 宏值格式完整（uuid 20 字符、authkey 32 字符、PID 16 字符），但有效性（是否在涂鸦云注册、是否与该 PID 配套）只能由用户提供凭证或实测。若无效：BLE 侧表现为 key11 失配（§4 Q7 的乱码正是该症状的首次实证）、云侧在 `STATE_ACTIVATING` 循环 `http active error`（tuya_iot.c:1018-1031→221-260→STATE_RESTART）。若有效：rcs/psk/region 均自动降级或由配网 token 补齐，无额外阻塞。

### Q3 ipc_ac7916a 的授权/KV/Flash 方式能否迁移？

授权部分**无可迁移物**（旧工程授权在闭源 `libtuya_iot.a`，KV 用厂商 VM）；TuyaOpen 侧 KV 已正常工作（`tkl_flash.c:13-22` 硬编码 4MB 分区表：KV_KEY 0x300000/KV_DATA 0x301000/UF 0x311000(512K)；littlefs 自动 format；rst_cnt 写入成功）。

### Q4 平台是否需要额外实现持久化/region/PSK/证书？

**不需要**：KV 已具备；region 由配网 token 运行时写入（tuya_endpoint.c:225-237；仅当 KV 写失败才会卡死在 STATE_ENDPOINT_UPDATE 无限重试——当前 KV 可写，不构成阻塞）；rcs/psk 有编译期默认（tuya_register_center.c:343-358、tuya_cert_manager.c:702-706）。可选增强：实现 `tuyaopen_license_read`（读自家 flash 授权区，JSON 格式见 tuya_authorize.c:220-221 注释）或用 CLI `auth <uuid> <authkey> 0` 写 KV（tuya_authorize.c:388-451）。

### Q5 需要用户提供什么

一组**有效的 TuyaOpen 授权三元组**（PID/uuid/authkey，购买入口 platform.tuya.com/purchase/index?type=6，即日志 F7 中的提示），或书面确认当前 `tuya_config.h` 中三元组有效且与 PID 绑定。无需提供密钥文件/证书。

---

## 7. ipc_ac7916a 可借鉴内容汇总

| 类别 | 内容 | 位置 |
|---|---|---|
| ✅ 直接借鉴 | ATT 三件套：payload=200、cbuf=512、ram=900；栈初始化 `ble_vendor_set_default_att_mtu(200)`；MTU 协商完成 `ble_op_att_set_send_mtu(mtu-3)` | le_net_cfg_tuya.c:63-69,267,611,372-377 |
| ✅ 直接借鉴 | notify 前查 CCCD + 发送余量 | 同上 :472-494 |
| ✅ 直接借鉴 | WiFi 生命周期：AP 前 `net_set_lan_info`；`wifi_set_default_mode(AP,force=1)` 重武装；`stop_ap`/`station_disconnect` no-op（勿 wifi_off）；相同凭据短路；超时参数 | tuya_os_adapt_wifi.c:243-369,504-517,1192-1218,1225-1228,1264-1349 |
| ❌ 不可复制 | 旧 TuyaOS 回调接口（TY_BT_EVENT_*）、闭源 libtuya_iot.a 协议栈、旧 SDK lwip2.1.3 常量 | — |
| 🔁 需桥接 | 厂商事件→TKL/TAL 事件模型（当前 tkl 骨架已正确） | tkl_bluetooth.c |

---

## 8. 其他平台可借鉴内容（Z:\tyopen\TuyaOpen\platform，本地实际仅 4 平台）

| 平台 | 借鉴点 | 位置 |
|---|---|---|
| LINUX | TKL 层完整 GATT 范式（当前 JIELI 同款路线的权威参照）：UUID↔handle 注册序号双向映射、写回调同步零拷贝直传 | `platform\LINUX\tuyaos_adapter\src\tkl_bt\tkl_bluetooth.c:377-452`、bt_dbus_api.c:514-585,189-209 |
| ESP32 | `start_ap` 显式 `dhcps_stop→set_ip_info→dhcps_start`；断线事件按"曾否 CONNECTED"二分 | `platform\ESP32\...\tkl_wifi.c:1014-1101,151-181,224-237` |
| T5AI/T3 | `start_ap` 前设 netif ip4（含 DNS=gw）；**station_connect 显式 `disable_auto_reconnect_after_disconnect=true`（与杰理空 SSID 重连同源对策）**；POWERDOWN 先逐项退出的模式状态机 | `platform\T5AI\...\tkl_wifi.c:491-544,763-812,1268-1308,1154-1187` |
| 全部 | `WFE_CONNECTED`=STA 拿到 IP（与当前 JIELI 映射一致）；license 钩子 NOT_SUPPORTED 是官方惯例 | 各平台 tkl_flash.c |

ESP32/T5AI/T3 的 BLE 层均为纯 HCI 管道（GATT 在 SDK NimBLE host），**GATT 语义层可参考的只有 LINUX 平台**。

---

## 9. 结论三栏：事实 / 推断 / 待验证

| # | 事实（日志+源码直接证明） | 推断（证据充分的源码推演） | 待验证（缺证据，不得写成结论） |
|---|---|---|---|
| 1 | BLE 广播、LL 建链、ATT 写上行数据完整（F14/F16/F18） | — | — |
| 2 | `att_send need_size` 出现在连接后 12ms、早于任何 App 数据；6/188=库固定常量 | ATT 发送模块因"payload 128 + 未设默认 MTU + 未跟协商 MTU"校验失败，导致 notify 全废=**阻塞点 A** | 闭源库内精确判定条件（无法读源） |
| 3 | `conn nack` 为闭源 LL 库日志、始于 A 之后 | A 的连锁现象 | 与 WiFi 扫描的射频共存关联 |
| 4 | 33B=1+16+16 标准加密帧；CBC 无 MIC；26440 来自解密后 LEN 字段乱码 | KEY_11 失配（uuid/authkey 与云端下发不一致）=**阻塞点 B**；`tuya_config.h` 三元组有效性存疑 | mode 字节实际值、raw dump（开 `tuya_ble_enable_debug`）、换有效授权复测 |
| 5 | 手机 reason 13 主动断开、距失败 1.4s | App 等响应超时 | — |
| 6 | AP 模式进入成功、TCP 6668 起服；`tkl_wifi_start_ap` 忽略 `cfg->ip` | AP 配网路径（手机直连 192.168.176.1）当前不可用；DHCP 是否自动服务未知 | 手机连 SmartLife-B7CB 实测 |
| 7 | 空 SSID 重连循环贯穿全程、不阻塞 BLE/AP | 热切换未重武装驱动所致噪声 | 对后续 STA 连接的影响 |
| 8 | license 钩子为 stub、KV 首刷为空、fallback 宏格式完整 | 授权路径为"编译宏"，符合官方 demo 惯例 | 三元组是否有效（决定 B 与激活成败） |
| 9 | `wifi_off()` 在 stop_ap/station_disconnect 中被调用（与参考 no-op 相反） | netcfg 停 AP→切 STA 的 wifi_off/wifi_on 循环有失稳风险 | 实际 STA 转换日志 |
| 10 | switch_demo 无 GPIO/继电器/按键/独立 DP 上报代码 | "GPIO/继电器动作"验收项当前**无实现承载** | 用户是否要求补齐硬件联动 |

---

## 10. 最小修改方案（仅方案，未实施）

### M1（BLE，最高优先级，单文件）

`platform\JIELI\tuyaos\tuyaos_adapter\src\tkl_bluetooth.c`：

1. `JIELI_ATT_LOCAL_PAYLOAD_SIZE` 128→**200**（与参考一致，ram buffer 随之 900，tkl:47-52）；
2. `ble_profile_init()` 中 `att_server_init` 之后补 `ble_vendor_set_default_att_mtu(200);`（对齐 le_net_cfg_tuya.c:611）；
3. 实现 `jieli_ble_att_packet_handler`（现为空，tkl:162-168）：处理 `ATT_EVENT_MTU_EXCHANGE_COMPLETE` → `ble_op_att_set_send_mtu(mtu-3)`（对齐 :372-377）；
4. （可选加固）`jieli_ble_att_send` 发送前检查 `att_get_ccc_config(char_handle+1)` 与 `ble_op_att_get_remain`。

预期直接消除阻塞点 A（设备能回 notify）。

### M2（诊断，与 M1 同批）

临时开启 `tuya_ble_enable_debug(true)`（或等价 raw dump 开关）一次抓包，确认阻塞点 B 的 mode 字节与密文；随后用**有效授权三元组**（用户提供）更新 `tuya_config.h` 后复测——若 raw 帧合法且换钥后 `ble recv req type 0x0000` 出现，B 定案。

### M3（WiFi，Phase 2，建议顺序执行）

`tkl_wifi.c`：

1. `tkl_wifi_start_ap` 增加对 `cfg->ip/gw/mask` 的支持：进 AP 前调用厂商 `net_set_lan_info`（**先验证 lwip_2_2_0 是否导出该符号**；参考模式 tuya_os_adapt_wifi.c:243-314）；
2. `tkl_wifi_stop_ap`/`tkl_wifi_station_disconnect` 去掉 `wifi_off()`（改 no-op 或仅做 AP 停止），交由 `set_work_mode(POWERDOWN)` 承担断电语义；
3. `tkl_wifi_station_connect` 增加相同凭据短路；
4. 若空 SSID 循环仍存：按参考以 `wifi_set_default_mode(AP_MODE,force=1)`+模块重启方式进 AP（改动最大，最后做）。

### M4（授权，依赖用户输入）

确认/替换 `tuya_config.h` 三元组；长期可选实现 `tuyaopen_license_read`（读 flash 授权区）或用 CLI `auth` 写 KV。

---

## 11. 分阶段验证计划

- **P0（改前基线，只读）**：保留本次 monitor_capture.log 作为基线；记录手机 App 型号与版本。
- **P1（M1 后）**：重编译烧录，复测 BLE：预期日志 `att_send need_size` 消失、出现 `ble recv req type 0x0000`（QRY）与设备 notify 后 App 进入配网流程；若仍 `packet len err` → 执行 M2 诊断。
- **P2（M2/授权后）**：预期 `Ble is paired`（ble_mgr:725）、`ble dev info: state:0, pkg_len:xxx`、ble_netcfg 收到 JSON、`tkl_wifi_station_connect` 入口日志。
- **P3（WiFi）**：预期 `wifi_enter_sta_mode`→`STA_CONNECT_SUCC`→`DHCP_SUCC`→`WFE_CONNECTED`→netmgr 上线→iotdns→`STATE_ACTIVATING`→激活成功→MQTT connected→App 绑定成功；同时观察空 SSID 循环是否消失。
- **P4（M3 后回归）**：AP 直连配网路径实测（手机连 SmartLife-xxxx → 192.168.176.1:6668）；断电重启后 KV 恢复（免配网直连 + 已激活状态）验证。
- 每阶段一份完整新日志归档，未达预期禁止叠加修改（单变量原则）。

---

## 12. 完整 switch_demo 验收标准

| # | 验收项 | 判定日志锚点 | 当前状态 |
|---|---|---|---|
| 1 | BLE 可发现 | `ble adv updated` + 手机扫描到 | ✅ 已达成（F14） |
| 2 | BLE 可连接 | `CONNECT_REQ`/`Ble Connected` | ✅ 已达成（F16） |
| 3 | Tuya BLE 协议收发正常 | `ble recv req type 0x0000` + 设备 notify 成功（无 `att_send need_size`/`packet len err`） | ❌ 阻塞点 A/B |
| 4 | BLE 配网成功 | `Ble is paired` + ble_netcfg JSON + token | ❌ |
| 5 | WiFi AP/STA 切换正确 | netcfg_stop→ap stop→set_work_mode(STA) 无异常重启 | ⚠ 未走到/待验证 |
| 6 | WiFi 获取 IP | `DHCP_SUCC` + `WFE_CONNECTED` | ❌ 未走到 |
| 7 | 云激活并上线 | STATE_ACTIVATING 通过 + MQTT connected | ❌ 依赖 4/6 + 有效授权 |
| 8 | DP 下发 | `SOC Rev DP Cmd` + dpid/value 打印 | ❌ 依赖 7 |
| 9 | GPIO/继电器动作 | — | ❌ demo 无实现（需产品决策） |
| 10 | DP 状态上报 | `tuya_iot_dp_obj_report` 发送成功 | ❌ 依赖 7（现为回显式） |
| 11 | 重启后状态/云连接恢复 | 二次上电免配网、KV 恢复、直连 MQTT | 未验证 |
| 12 | 日志质量 | Tuya/厂商日志可读（本次 UART 交错明显，如 monitor_capture.log 320-345 行乱码区） | ⚠ 建议评估并发输出加锁 |

---

## 13. 下一位开发 agent 执行清单

1. **核对并实施 M1**（tkl_bluetooth.c 三处：payload/200、`ble_vendor_set_default_att_mtu(200)`、MTU exchange complete 处理）——先读 `le_net_cfg_tuya.c:262-272,372-377,601-612` 对照。
2. **加临时诊断**：`tuya_ble_enable_debug(true)` 或在 `ble_packet_recv`（ble_mgr.c:298-306）加 raw/dec hex dump，抓一次 26440 复现帧，回报 mode 字节与密文长度。
3. **向用户索取**：有效授权三元组确认（或购买）；确认是否需要 GPIO/继电器联动（决定是否补 board 代码）。
4. **WiFi（Phase 2）**：验证 `net_set_lan_info` 在 lwip_2_2_0 的可用性（grep `cpu\wl82\liba\lwip_2_2_0.a` 符号）；按 M3 顺序改造 `tkl_wifi.c`；新增 `tkl_wifi_station_connect` 入口日志（只打 ssid 长度）。
5. **不建议动**：SDRAM 配置、Flash 容量（交接文档明确）、`switch_demo` 应用层逻辑、GATT profile 表（已与参考一致）。
6. 每步烧录后**新开日志文件**，勿覆盖 `monitor_capture.log` 基线。

---

## 14. 关键文件路径与行号速查

- 日志：`apps\tuya_cloud\switch_demo\monitor_capture.log`（F1–F20 各行号见 §2）
- TKL BLE：`platform\JIELI\tuyaos\tuyaos_adapter\src\tkl_bluetooth.c` — 47-52(ATT参数) / 89-94(handle) / 118-148(V2表) / 162-168(空handler) / 226-269(写回调) / 271-293(profile_init) / 486,550-552(init/deinit) / 869-915(service_add) / 951-981(发送/MTU)
- TKL WiFi：`...\src\tkl_wifi.c` — 109-136(事件) / 158-170(init) / 239-258(start_ap/stop_ap) / 339-358(work_mode) / 422-434(connect/disconnect) / 445-469(status)
- License stub：`...\src\tuyaopen_license.c:3-19`；凭据宏：`apps\tuya_cloud\switch_demo\src\tuya_config.h:32-41`；fallback：`...\src\tuya_main.c:260-265`
- 应用/协议：`src\tuya_cloud_service\ble\ble_mgr.c`（240/311/507/725/964/981-1006）、`ble_cryption.c:48-55,289-330`、`ble_trsmitr.c:258-355`、`ble_protocol.h:112-122`
- TAL：`src\tal_bluetooth\src\tal_bluetooth.c:37-39,204-386,431-533,895-910`
- netcfg/AP IP：`src\tuya_cloud_service\netcfg\ap_netcfg.c:805-848`（825=192.168.176.1）、`ble_netcfg.c:34-119`、`netconn_wifi.c:64-95,248-308`
- 厂商常量：`AC79_AIoT_SDK\include_lib\btstack\le\le_common_define.h:54-55`（188/6）、`ble_api.h:541-542,638-639,360`、`att.h:21`
- 参考（ipc_ac7916a）：`AC79NN_SDK\apps\common\ble\le_net_cfg_tuya.c:63-69,262-272,372-377,444-494,601-612`、`include\le_net_cfg_tuya.h:8-47`、`tuya_os_adapter\src\driver\tuya_os_adapt_wifi.c:243-369,504-517,1192-1349`、`tuya_os_adapt_bt.c:297-316`
- 构建：`platform\JIELI\jieli_build.py:128-198(板级注入),300-363(源列表),410-442(库)`；`AC79_AIoT_SDK\apps\common\ble\le_net_cfg.c:61-64`（厂商通用示例 payload=128）

---

## 核心问题回答

**在当前 TKL/TAL 架构下，正确复用杰理 BLE/WiFi 底层能力的路径已经明确，且不需要换架构**：当前适配走的"TKL 层承载 GATT"路线与 LINUX 平台范式一致，GATT profile 与参考工程逐字节相同，BLE 上下行数据通路完好。所欠者三件——

1. 把 `ble_op_att_send_init` 的参数与配套调用（默认 MTU + 协商后 set_send_mtu）补齐到与曾跑通的 `le_net_cfg_tuya.c` 完全一致（消除 notify 全废，阻塞点 A）；
2. 提供有效授权三元组（消除 key11 解密乱码，阻塞点 B，并打通云激活）；
3. WiFi 侧按参考补 `net_set_lan_info`/生命周期语义（停 AP 勿 `wifi_off`、解除空 SSID 重连）。

三件事完成后，"BLE 可发现→可连接→协议收发→配网→STA 入网→拿 IP→激活上线→DP 收发→重启恢复"链路才具备逐段点亮的条件；GPIO/继电器联动在当前 demo 中无实现，需产品决策是否补齐。**在取得 P1–P3 各阶段完整新日志前，任何"已跑通"的结论都不成立。**
