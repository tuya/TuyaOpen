# TuyaOpen × 杰理 AC7916A（wl82）OTA 适配交接文档

> 更新时间：2026-09-23
> 前置文档：`JIELI_TUYAOPEN_SWITCH_DEMO_HANDOFF_CN.md`（配网/云链路阶段）
> 本文档覆盖 **OTA 适配阶段**（2026-09-22 ~ 09-23）的全部内容

---

## 1. 项目总目标与当前坐标

**总目标**：TuyaOpen 在杰理 AC7916A（wl82，4MB flash + 8MB SDRAM）上跑通完整链路：
BLE 配网 → WiFi STA → 云激活 → MQTT → DP 控制 → **OTA 升级**。

| 链路 | 状态 | 备注 |
|---|---|---|
| BLE 配网 / WiFi / 云激活 / MQTT / DP | ✅ 2026-09-18 硬件验证通过 | 见前置交接文档 |
| 双备份 OTA 构建链路 | ✅ 构建成功，QIO+UG 双工件产出 | 本文档 §3 |
| Flash 分区方案（TUYA 预留区） | ✅ 烧录验证，KV init:0 | 本文档 §4 |
| BLE MAC 初始化（新 VM 环境） | 🔶 修复已烧录，**待启动日志确认** | 本文档 §6 |
| 云端 OTA 端到端 | ⏳ 未测（等 MAC 修复确认后） | 本文档 §7 |

---

## 2. OTA 调研核心结论（2026-09-22）

### 2.1 官方参照：`ipc_ac7916a`

`D:\tuya_proj\jieli\ipc_ac7916a\tuya_os_adapter\src\driver\tuya_os_adapt_ota.c` —— 涂鸦官方在该芯片上的 OTA 适配只有 40 行，全部桥接到厂商 `net_update` 文件流接口：

| Tuya 回调 | 厂商 API | 作用 |
|---|---|---|
| `ota_start_inform` | `net_fopen("update-ota.ufw","w")` | 初始化双备份升级 |
| `ota_data_process` | `net_fwrite(fd, data, len, 0)` | 流式写入备用 bank |
| `ota_end_inform(true)` | `net_fclose(fd, 0)` | 校验 → 烧 boot info → 2 秒后自动重启切 bank |

其 SDK 的 `apps/tuya_lock`（涂鸦官方 App，同 4MB flash）配置 `CONFIG_DOUBLE_BANK_ENABLE 1` + 编译 `net_update.c`。

### 2.2 厂商双备份机制（三层）

```
TuyaOpen tuya_ota.c（HTTPS 下载 + SHA256/HMAC 校验 + 进度上报，已有无需改）
   ↓ tal_ota → tkl_ota.c（我们写的 ~100 行）
net_fopen/net_fwrite/net_fclose   ← apps/common/update/net_update.c
   ↓ 4K 缓冲对齐
update.a: dual_bank_passive_update_init → write → verify → burn_boot_info
   ↓ 重启后 uboot 按 BootInfo{codeLength,baseAddress,version} 选 bank
（旧 bank 完好，升级中断/校验失败不影响运行，掉电安全）
```

### 2.3 ⚠️ 关键实测结论：bank2 是 flash 上半区（半分），不是紧跟固件

烧录日志 `Erase app2 core data [SUCCESS]` + 启动日志 KV 被擦/写保护证实：

- **bank0** = [0x2000, CODE_BOUNDARY)，运行区，烧录时被代码保护（PRCT_OPT=2）
- **bank2/app2** = [CODE_BOUNDARY, 第一个预留区)，**整个上半区**，同样被保护
- 旧 TuyaOpen 分区（KV@0x300000/UF@0x311000/RCD@0x391000）落在 bank2 里：被烧录器擦掉 + 变只读（littlefs `Superblock unwritable`，format 返回 -28）
- `CODE_BOUNDARY_LINE ≈ (第一个预留区起点 + 0x3000) / 2`（两次实测吻合）

### 2.4 固件工件约定

| 工件 | 内容 | 用途 |
|---|---|---|
| `{project}_QIO_{ver}.bin` | app.bin（objcopy 拼接的原始镜像） | **USB 烧录**（isd_download 现场打包） |
| `{project}_UG_{ver}.bin` | `db_update_files_data.bin`（isd_download `-update_files normal` 生成） | **上传涂鸦云做 OTA** |

⚠️ 烧录用 QIO、OTA 用 UG，**混用会升级失败**（厂商文档明确）。

---

## 3. 已落地的代码改动（全部未提交，待走 PR）

### 3.1 `platform/JIELI/jieli_build.py`
- `create_staging_tree` full_stack 分支：编译 `apps/common/update/net_update.c`；DEFINES 追加 `-DCONFIG_DOUBLE_BANK_ENABLE=1`（该宏驱动三处：app 双备份代码、`isd_config_rule.c` 生成 BR22_TWS_DB 布局、`download.c` 加 `-update_files normal`）
- 新增 `find_ug_artifact` / `tuya_ug_name` / `build_ota_package_command`
- 新增 `configure_tuya_reserved_area(ini)`：在 make pre_build 重新生成的 `isd_config.ini` 的 `[RESERVED_CONFIG]` 注入 `TUYA_ADR=0x3A0000; TUYA_LEN=0x55000; TUYA_OPT=1`（OPT=1 = 烧录不擦、不受代码保护、不受 OTA 波及）
- 新增 `sanitize_host_path`：过滤 PATH 中 `usr/bin` 条目，防止 Git Bash 发起的进程链让 vendor make 选中 sh.exe 吃掉 Windows 路径反斜杠（`C:\JL\pi32` → `C:JLpi32`，命令 127）

### 3.2 `platform/JIELI/build_example.py`
- Windows 构建路径新增 `_generate_ota_package`：make 后直接跑 isd_download 打包 OTA 工件（无 `-wait/-reboot`，无设备时 ~1 秒打印 "Device Offline" 退出，退出码 -11 但文件已生成 → **以工件存在判成功，不看退出码**）
- `build()`：make 后调用 `configure_tuya_reserved_area`；复制 QIO + UG 双工件到 `dist/` 和 `.build/bin/`
- `clean()`/`build()` 的 env PATH 走 `sanitize_host_path`

### 3.3 `platform/JIELI/build_setup.py`
- 烧录前校验从"必须存在 download.sh"（生成物，fresh checkout 没有）改为接受 `download.sh`/`download.bat`/`download.c`（源）

### 3.4 `platform/JIELI/platform_flash_bridge.py`
- **恢复了丢失的 keepvm 逻辑**（之前切分支时丢过一次）：烧录时生成 `isd_config_keepvm.ini`（VM_OPT=0→1），保住厂 VM 区；`JIELI_FLASH_ERASE_VM=1` 可恢复原厂擦除行为
- 工具目录不完整时给出可操作错误提示（"先跑一次 tos.py build，make pre_build 会重新生成 isd_config.ini"）——**isd_config.ini 必须与固件同次构建生成**（单/双备份布局不同，不能用别的目录的 ini）

### 3.5 `platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_ota.c`（重写）
- `get_ability`：`TUYA_OTA_FULL`，上限 `TKL_OTA_MAX_IMAGE_SIZE = 0x1A4000`（bank2 容量 0x3A0000-0x1FC000... 以最新布局 [0x1CD000, 0x397000) 计）
- start → `net_fopen(CONFIG_UPGRADE_OTA_FILE_NAME, "w")`；重复 start_notify 幂等（HTTP 断点重连会重复通知）
- process → `net_fwrite`，**严格校验 `pack->offset == s_written`**（厂商路径只追加不能 seek，断点续传 offset 跳变时 abort 让云端整包重传）
- end(reset=true) → `net_fclose(fd, 0)`（校验+烧 boot info+2 秒后自动重启）；end(false) → `net_fclose(fd, 1)`（不烧 boot info）
- 已验证代码在最终镜像中（LTO 会内联函数致符号表"消失"，以 `.rodata` 日志字符串和 `sdk.elf.resolution.txt` 为准）

### 3.6 `platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_flash.c`（分区迁移）
新分区表（全部在 TUYA 预留窗 [0x3A0000, 0x3F5000) 内）：

| 分区 | 地址 | 大小 |
|---|---|---|
| KV_KEY | 0x3A0000 | 4K |
| KV_DATA | 0x3A1000 | 64K |
| UF (littlefs) | 0x3B1000 | 192K（从 512K 缩） |
| RCD | 0x3E1000 | 80K（从 256K 缩） |

### 3.7 `platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_bluetooth.c` + `tkl_wifi.c`（MAC 死锁+漂移修复，最终方案 v5：Flash UUID 确定性派生）

> 完整调研与迭代过程见 `docs/JIELI_WL82_MAC_ISSUE_SUMMARY_CN.md`

- **问题一（死锁）**：新布局下厂 VM（0x397000，随布局移动）为空/含 FF 占位 → 厂商 `bt_get_mac_addr()` 校验失败（`key_mac no crc`）→ 回退 WiFi MAC → 与延迟启动 WiFi 死锁。v4 起彻底绕开该函数
- **问题二（漂移）**：VM syscfg 写入在此板不跨重启；WiFi MAC 兜底 `flash_uid+rand32()` 每次开机变化（两次启动实测 `98:2b:...` → `d4:c3:...`）
- **最终方案**：新增 `include/driver/tkl_jieli_chip_mac.h`，`jieli_chip_mac()` 从 **Flash UUID（出厂固化 16 字节，`get_norflash_uuid()`）** 加盐异或折叠出本地管理单播 MAC——无随机、无存储、重启/重烧/VM 搬家全部稳定。BLE 侧喂 `lib_make_ble_address`→`le_controller_set_mac`；WiFi 侧 STA worker 在 `wifi_on` 后、关联前 `wifi_set_mac` 钉死
- 日志：`[JIELI][BLE] chip mac ...` / `[JIELI][WIFI] chip mac ...`
- **量产增强点**：VM 有产线正式 MAC（`VM_TUYA_MAC_IDX`/`CFG_BT_MAC_ADDR`）时应优先采用，UUID 派生仅作兜底（与 ipc_ac7916a 生产行为对齐）

### 3.8 测试（`tests/platform/`，59 全过）
- `test_jieli_build.py`：+双备份 staging 契约、+find_ug_artifact、+tuya_ug_name、+sanitize_host_path、+configure_tuya_reserved_area
- `test_jieli_flash_bridge.py`：hermetic fixture 化（不依赖磁盘上的 SDK）、+keepvm 保留/恢复、+不完整工具目录提示
- `test_jieli_tal_contracts.py`：+OTA 桥接契约、+offset 顺序校验、+full 包能力、+分区必须在 TUYA 窗口内（数值解析 tkl_flash.c 宏）、+MAC 补种先于 bt_get_mac_addr

---

## 4. 当前 Flash 布局（4MB，双备份）

```
0x0002000  uboot
0x002000   bank0 运行区（当前固件 ~1.06MB）
0x1CD000   CODE_BOUNDARY → bank2/app2 OTA 目标区（~1.86MB 容量 ≥ 固件 1.09MB）
0x397000   厂 VM 32K（syscfg，含 key_mac）
0x39F000   BTIF 4K
0x3A0000   ★ TUYA 预留窗 348K（TUYA_OPT=1：烧录不擦、不受代码保护、OTA 不碰）
             ├ 0x3A0000 KV_KEY 4K
             ├ 0x3A1000 KV_DATA 64K
             ├ 0x3B1000 UF 192K
             └ 0x3E1000 RCD 80K
0x3F5000   USER 4K → 0x3F6000
（isd_download FLASH INFO 实测：VM/BTIF 会自动排在 TUYA 窗口下方）
```

⚠️ **布局变更不可 OTA 跨越**：本次 TUYA 窗口引入改变了 VM/USER 的位置，从旧固件直接 OTA 到新固件会校验失败（厂商 note.txt 明示预留区配置须一致）——所以本次必须 USB 重烧，之后的 OTA 都在此布局上迭代。

---

## 5. 已验证 ✅ / 待验证 ⏳

### 已验证（有日志/产物证据）
1. 双备份构建 + QIO/UG 双工件（BUILD SUCCESS × 多次）
2. `isd_config.ini` 含 `BR22_TWS_DB=YES` + `TUYA_ADR=0x3A0000 OPT=1`，FLASH INFO 确认布局
3. 烧录器 "Erase app2 core data [SUCCESS]"（不再碰 TUYA 窗口）
4. 启动日志 `[KV] init result:0`（新窗口可读可写可格式化）
5. OTA 代码在最终镜像（ELF 字符串 + LTO resolution）

### 待验证（下一步动作，按序）
1. **MAC 修复确认**：抓启动日志 → 应有 `[JIELI][BLE] bt mac <随机值>`（非 ff:ff:...）、无 `wifi get mac pending` 死循环、BLE 广播起来
2. **MAC 稳定性**：设备再重启一次，对比两次 `bt mac` 值。若每次都变（说明 USER_CFG 每次开机都写 FF 占位），需改用确定性派生源（如 flash UUID）
3. **配网**：App 扫码配网 → 设备上线
4. **烧录保数据**：配网成功后再烧一次同固件 → 设备应直接连 WiFi 上线（不重新配网）＝ TUYA OPT=1 + keepvm 双保险验证
5. **云端 OTA**：IoT 工作台上传 `apps/tuya_cloud/switch_demo/dist/switch_demo_1.0.0/switch_demo_UG_1.0.0.bin`（⚠️ 用 UG 不是 QIO）→ 升级设备 → 盯日志：
   - `[JIELI][OTA] start ...`（我们的 tkl 日志）
   - update 库打印的**升级目标地址**（实锤 bank2 区间 [0x1CD000, 0x397000)，最终确认与 TUYA 窗口无冲突）
   - 下载完成 → `switching bank` → 重启 → 云端版本号更新
6. **失败场景**（可选）：OTA 过程断电 → 重启应仍在旧 bank 正常运行

---

## 6. 已知风险与注意点

| 项 | 说明 |
|---|---|
| BR22_TWS_VERSION | `cpu/wl82/tools/isd_config_rule.c` 里的版本号每次发 OTA 版本要 +1，否则"OTA 后工具烧 bin 还是旧程序"。bring-up 手动，产品化需从 APP_VERSION 自动注入 |
| tkl_ota 上限 | `TKL_OTA_MAX_IMAGE_SIZE 0x1A4000` 按当前 bank2 推算；若固件涨到 ~1.6MB+ 需重估（UF/RCD 还可再缩） |
| UF 缩容 | 512K→192K，KV 存储对 switch demo 足够；若加 DP 数量大的产品需评估 |
| OTA 期间 MQTT | `net_fclose` 后 2 秒自动重启；`tuya_ota` 的 TUS_UPGRD_FINI 上报在 end_notify 之前发出，时序与官方参照一致 |
| 构建环境 | ① tos.py 必须用 `.venv\Scripts\python.exe` 跑（系统 python 找不到 venv 的 cmake/ninja）② 平台 commit 不匹配提示选 `n`（保持本地 fork 版本）③ 构建必须从 PowerShell 语境发起（已由 sanitize_host_path 兜底，但直连 make 仍会踩 sh.exe 坑）④ 修改 Kconfig 需 `tos.py clean -f` |
| 烧录 isd_config.ini | 必须来自与固件同一次构建（pre_build 生成）；目录里没有 = 先跑一次 build |
| Git 工作流 | **一律 PR，不允许直接 push**（用户明确要求）。当前所有改动未提交；主仓 platform/JIELI submodule 指针也待更新。提交用底层命令避免 partial clone 卡死（见 memory：jieli-repo-layout-and-git-recovery） |

---

## 7. 环境与命令速查

```powershell
# 构建（在 apps/tuya_cloud/switch_demo 下；'n' 应答平台 commit 提示）
'n' | ..\..\..\.venv\Scripts\python.exe ..\..\..\tos.py build

# 烧录（设备进烧录模式：按住烧录键上电 / 重插 USB）
'n' | ..\..\..\.venv\Scripts\python.exe ..\..\..\tos.py flash
#   烧录保 VM（默认）；恢复原厂擦除：$env:JIELI_FLASH_ERASE_VM='1'

# 产物
#   .build/bin/switch_demo_QIO_1.0.0.bin   ← USB 烧录镜像
#   .build/bin/switch_demo_UG_1.0.0.bin    ← 涂鸦云 OTA 固件
#   dist/switch_demo_1.0.0/                ← 同上两件的发布目录

# 单测
python -m pytest tests/platform/ -q        # 59 passed

# 仅重新生成 OTA 打包（离线 1 秒，可反复跑）
cd platform/JIELI/chip/wl82/AC79_AIoT_SDK/cpu/wl82/tools
./isd_download.exe isd_config.ini -gen2 -tonorflash -dev wl82 -boot 0x1c02000 \
  -div1 -uboot uboot.boot -app app.bin cfg_tool.bin -res cfg -extend-bin -update_files normal
```

## 8. 关键文件索引

| 文件 | 作用 |
|---|---|
| `platform/JIELI/jieli_build.py` | 多芯片构建桥：staging 生成、TUYA 预留区注入、工件定位、PATH 清洗 |
| `platform/JIELI/build_example.py` | 构建入口：make → TUYA 注入 → postbuild → QIO/UG 复制 |
| `platform/JIELI/platform_flash_bridge.py` | USB 烧录桥（isd_download 驱动、keepvm） |
| `platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_ota.c` | OTA TKL（net_fopen 三件套 + offset 顺序校验） |
| `.../src/driver/tkl_flash.c` | TuyaOpen flash 分区表（TUYA 窗口内） |
| `.../src/driver/tkl_bluetooth.c` | BLE TKL（含 MAC 补种 `jieli_seed_bt_mac_once`） |
| `chip/wl82/AC79_AIoT_SDK/cpu/wl82/tools/isd_config.ini` | 构建时生成（含 TUYA/双备份布局）；`isd_config_keepvm.ini` 烧录时生成 |
| `chip/wl82/AC79_AIoT_SDK/include_lib/update/dual_bank_updata_api.h` | 厂商双备份 API |
| `ipc_ac7916a/tuya_os_adapter/src/driver/tuya_os_adapt_ota.c` | 官方 OTA 桥接参照 |
| 厂商文档 | https://doc.zh-jieli.com/AC79/zh-cn/master/module_example/system/update.html |
