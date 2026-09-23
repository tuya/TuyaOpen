# 杰理 wl82 蓝牙/WiFi MAC 问题调研与修复总结

> 时间：2026-09-22 ~ 2026-09-23
> 关联文档：`JIELI_TUYAOPEN_OTA_ADAPTATION_HANDOFF_CN.md`（OTA 适配交接）
> 涉及代码：`platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_bluetooth.c`、`tkl_wifi.c`、`include/driver/tkl_jieli_chip_mac.h`

---

## 1. 问题一：`wifi get mac pending...` 启动死锁

### 症状

设备启动后无限循环打印 `wifi get mac pending...`（每 200ms 一次），BLE 栈初始化永不返回，配网广播无法开启：

```text
[00:00:01.133][Info]: [SYSCFG]get_bt_mac_addr key_mac no crc
[00:00:01.144]wifi get mac pending...
[00:00:01.400]wifi get mac pending...   ← 无限循环
```

### 根因链（四环相扣）

```text
① tkl_ble_stack_init 调 lib_make_ble_address(ble_addr, bt_get_mac_addr())
        ↓
② 厂商 bt_get_mac_addr()（apps/common/config/user_cfg.c）先试 wifi_get_mac()
   —— VM 里没有有效 key_mac 条目（日志 "key_mac no crc"）
        ↓
③ wifi_get_mac 轮询等待 WiFi 模块上电
        ↓
④ 我们的 WiFi 是"延迟启动"设计（wifi_on 只在常驻 worker 里执行，
   提前上电会触发 AXI crash，2026-09-18 的教训）
        ↓
BLE 初始化在启动早期执行 → 死等一个永远不会上电的模块 → 死锁
```

**为什么 VM 里没有 MAC**：蓝牙 MAC 存在厂商 VM 区（syscfg `key_mac` 条目）。为 OTA 引入 TUYA 预留区后布局重排，**VM 从 0x3F5000 搬到 0x397000**——搬家后新地址是空的（旧数据留在旧地址不会迁移），开机 USER_CFG 只写了 `FF:FF:FF:FF:FF:FF` 占位。keepvm（VM_OPT=1）只保"同布局重烧不擦 VM"，救不了"布局变更导致的搬家"。

### 解决过程（4 轮迭代）

| 版本 | 尝试 | 结果 |
|---|---|---|
| v1 | `syscfg_read` 失败才生成随机 MAC | 无效——裸读"成功"读到 6 字节（FF 占位），跳过生成 |
| v2 | 读到也原值重写刷 CRC | 日志暴露值是 `ff:ff:ff:ff:ff:ff`，厂商仍拒绝 |
| v3 | FF/00 占位视为缺失 → 随机生成 + `syscfg_write` 持久化 | MAC 生成成功（`8a:a7:...`），但厂商仍不认写回的条目；且重启后读不回（MAC 漂移） |
| v4 | **彻底不调厂商 `bt_get_mac_addr`**，随机生成 BLE 地址 | ✅ 死锁消失，BLE 广播开启 |
| **v5（最终）** | **Flash UUID 确定性派生**（见 §5），替代随机生成 | ✅ 稳定且唯一 |

关键实验结论：**VM 的 syscfg 写入在这块板上不跨重启持久**（厂商和我们的写入一样失效；同一次启动内写入可见，重启后读不回）。厂商回退算法自己的持久化也依赖同一失效路径。

---

## 2. 问题二：WiFi MAC 每次重启漂移

日志实锤（两次不同启动）：

```text
启动 A：wifi use flash_uid+random mac[98:2b:1b:3f:69:e8]
启动 B：wifi use flash_uid+random mac[d4:c3:7b:51:d5:9c]   ← 变了
```

厂商 WiFi 驱动读 MAC 存储失败（`_flash_wifi_mac err, 0xfffffd01` / `set_flash_wifi_mac err`），回退到 `assign_macaddr.c` 的 **flash_uid XOR rand32()** 派生——随机部分每次开机重新生成，同一次启动内稳定、跨启动漂移。

**结论：WiFi MAC 与 BLE MAC 病根相同**——厂商把两者都存在 VM syscfg 区，这块板 VM 无产线写入数据，兜底路径又含随机数。

---

## 3. 概念澄清：芯片 MAC 是出厂写死的吗？

**不一定，分厂商流派**：

| 芯片 | MAC 来源 | 唯一性 |
|---|---|---|
| 乐鑫 ESP32 | **eFuse 出厂烧死**（base MAC + WiFi/BT 偏移） | IEEE OUI，全球唯一 |
| 博通集成 T5/BK7231 | 芯片内工厂校准区/OTP | 同上 |
| **杰理 wl82** | **flash VM 文件系统**——芯片里没有 | 依赖产线写入或运行时生成 |

杰理这边的证据（都在本仓库）：

1. **厂商源码自己承认没有固化 MAC**（`user_cfg.c` 的 `bt_get_mac_addr` 有"生成 + 写 flash"的完整兜底逻辑，若芯片有硬件 MAC 则不需要）
2. **杰理提供"软件 EFUSE"机制**（`isd_config_rule.c:112` 注释："支持EFUSE 烧写到flash最后4K"）——用 flash 保留区模拟 eFuse，出厂数据由产线工具写入
3. 旧 VM 里出现过的 MAC `74 CF D4 59 B7 CB`（U/L 位=0，全球管理地址）是之前烧录厂商 demo/产线时写入 VM 的，不是芯片硬件自带

**关于唯一性**：MAC 唯一分两层——
- **全球唯一（OUI）**：向 IEEE 购买前缀，产线批量分配（杰理/涂鸦产线 MAC 属于这类）
- **本地管理地址（Locally Administered）**：首字节 bit1 置 1，IEEE 允许自分配；我们的派生特意设了这一位（`mac[0] |= 0x02`），合规

派生用的 **Flash UID 是 JEDEC 标准的芯片出厂唯一序列号**（每个 NOR 颗粒不同）。细节：本板日志 `Flash UUID: 45 4A 4D 03 62 06 24 88 45 4A 4D 03 62 06 24 88` 前 8 字节 = 后 8 字节（`45 4A 4D` = ASCII "EJM"，杰理标识），真正区分设备的是中间变化字节，熵远超碰撞所需。

---

## 4. 官方参考工程 `ipc_ac7916a` 的 MAC 机制

### WiFi MAC（`AC79NN_SDK/apps/common/net/assign_macaddr.c:320-370`，实际生效分支）

```c
int init_net_device_mac_addr(char *macaddr, char ap_mode)
{
    syscfg_read(VM_TUYA_MAC_IDX, mac_id, 6);        // ① 读涂鸦专用 VM 条目
    if (mac_is_null(mac_id)) {
        // ② 兜底：flash_uid + JL_RAND 生成 → syscfg_write(VM_TUYA_MAC_IDX) 持久化
        //    打印 "wifi use flash_uid+random mac"
    } else {
        // ③ 正路："wifi use tuya mac"，wifi_set_mac 设置进驱动
    }
}
```

**正路是 `VM_TUYA_MAC_IDX` 条目**——由涂鸦产线/授权流程写入（涂鸦从自己的 IEEE OUI 池分配正式 MAC，产线烧录时连同 uuid/authkey 写入）。注意 `mac_is_null()` 校验**拒绝本地管理地址**（`mac_id[0] & 0x03` 必须为 0）——参考工程只认"真正的 OUI MAC"。

### BLE MAC（`AC79NN_SDK/apps/common/config/user_cfg.c:102`）

```c
#if 0
    wifi_get_mac(mac_addr);        // ← 参考工程把这段【#if 0 掉了】！
#endif
{
    if (syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6) == 6 && memcmp(mac_addr, bc_mac, 6)) {
        return mac_addr;           // ① 读 VM 条目（拒绝 FF×6）
    }
    // ② 兜底：flash_uid + rand32() → syscfg_write
}
```

### 参考工程与我们 Gitee 公开 SDK 的两处关键差异

| | `ipc_ac7916a` 的 AC79NN_SDK（涂鸦内部版） | Gitee 公开版 AC79_AIoT_SDK（我们用的） |
|---|---|---|
| `bt_get_mac_addr` 的 WiFi 回退 | **`#if 0` 禁用** → 永不死锁 | **启用** → VM 无 MAC 时死锁（我们踩的坑） |
| WiFi MAC 存储 | 专用 `VM_TUYA_MAC_IDX` + "wifi use tuya mac" 正路 | 同一套代码，但**产线从未写入过** → 直接走随机兜底 |

### 结论

`ipc_ac7916a` 生产设备 MAC 稳定唯一的保障是**产线写入**（涂鸦产线把正式 MAC 写进 VM），不是芯片硬件。我们这块开发板未经涂鸦产线授权流程（uuid/authkey 是编译期 fallback），VM 里没有产线 MAC——开发期只能自建稳定地址源。

---

## 5. 最终修复方案（v5，已实现）

新增 `platform/JIELI/tuyaos/tuyaos_adapter/include/driver/tkl_jieli_chip_mac.h`：

```c
static inline void jieli_chip_mac(uint8_t mac[6])
{
    // Flash UUID（出厂固化 16 字节，get_norflash_uuid()）逐字节加盐异或折叠成 6 字节
    // 置本地管理位（|=0x02）+ 单播位（&=0xFE）
    // 无随机、无存储：重启 / 重烧 / VM 搬家全部稳定
}
```

| 层 | 用法 |
|---|---|
| BLE（`tkl_bluetooth.c`） | `jieli_local_bt_mac()` → `jieli_chip_mac()` → `lib_make_ble_address`（纯计算）→ `le_controller_set_mac`；**彻底绕开厂商 `bt_get_mac_addr`**（同时消除死锁） |
| WiFi（`tkl_wifi.c`） | STA worker 在 `wifi_on` 成功后、关联前调 `wifi_set_mac(chip_mac)`——**空中广播 MAC 与云端上报 MAC 钉死且每次开机相同** |

契约测试（`tests/platform/test_jieli_tal_contracts.py`）：
- BLE 函数体内不得出现 `bt_get_mac_addr`、全文件不得依赖 `syscfg_read`
- WiFi worker 中 `wifi_set_mac` 必须先于 `wifi_enter_sta_mode`
- 头文件不得出现随机源（防回归断言）

## 6. 量产路线建议

- **开发期**：Flash UUID 派生（当前方案）——稳定、合规、实际唯一
- **量产期正路**：走涂鸦授权/产线流程，把正式 OUI MAC 写入 `VM_TUYA_MAC_IDX` / `CFG_BT_MAC_ADDR`；届时 TKL 层应增强为"**VM 有正式 MAC 则优先采用，UUID 派生仅作兜底**"（与 `ipc_ac7916a` 生产行为对齐；当前代码可按此扩展）
