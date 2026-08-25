# netmgr / tal_network 术语重命名迁移计划：`card` → `provider`

> 状态：**S1–S3 已执行**，`3b55d419..1935f2e3`（13 个 commit，每一步都在两个验证 target 上编过）。S4 未做 —— 它的触发条件是一次带 release note 的 tag 加足够的时间，不是树内状态，见 §4.5。
> 写作日期 2026-08-24，写作基线 `0dff53e0`；执行日期 2026-08-25，执行起点 `3b55d419`。执行中被证伪的地方已就地更正，每处都在正文里说明了是怎么发现的。
> 目标读者：要**执行**这次重命名的人，以及要 **review** 它的人
> 与另三篇的分工：
> - [`extension_guide.md`](extension_guide.md) 讲怎么往 netmgr 上面加东西，其中 §3「扩展路径二：加一个 socket provider」已经在用 `provider` 这个词讲这个概念；
> - [`release_notes.md`](release_notes.md) 讲升级后调用方能观察到什么变化，本文的 S4 会给它加一节；
> - [`known_gaps.md`](known_gaps.md) 讲重构撞到但故意没修的东西，本文 §6 把它们**逐条排除**在这次重命名之外；
> - 本文只讲一件事：把「socket 后端」这个概念的名字从 `card` 改成 `provider`，怎么分步落地，以及哪些东西不许跟着一起进来。

**计数约定**：本文每一个数字后面都跟着产生它的命令。**重跑它，不要相信它。** 一个看起来很权威的过期数字比没有数字更糟。所有数字的基线是 commit `0dff53e0`，在仓库根目录执行。

---

## 0. 一页纸结论

| 问题 | 答案 |
| --- | --- |
| 要改的标识符 | 17 个（§2.1），命中 **13 个源文件、96 行**，**全部在 `src/`**（§2.2 的并集 PAT 执行中被证伪一次，见该节） |
| `boards/` + `platform/` 命中 | **0**（§2.2；注意 `platform/*/` 被 `.gitignore` 排除，§2.4） |
| `apps/` + `examples/` 命中 | 代码 **0**；两个 `.config` 的头注释各有几行（§2.2） |
| `netmgr.h` 的 include 者 | **44 个参与编译的 `.c`/`.h`**，其中 **26 个在 `src/` 外**（§2.3） |
| 树里那句「44 files include it」 | **是对的**，四处都对。但它**不在 `netmgr.h` 里**（§2.3） |
| 44 / 40 / 32 三个传闻 | 44 对；「40 多个」对（扩展指南 §2.3 的原话）；32 与本事无关，它是 `netconn_attr_mask_t` 的位宽上限（§2.3） |
| 最难的一项 | `netmgr_conn_base_t.card_type` —— 结构体成员名**没有任何别名机制**，废弃窗口的长度由它一个人决定（§3.5、§4.7） |
| 绝不能变的东西 | 三个常量的**数值** 0/1/2 和 `MAX`=3。别名消失是一次响亮的编译错误；数值换意思是一次静默的错误行为（§4.7） |

---

## 1. 问题：一个词被用在两个概念上，现在是两个词用在一个概念上

`src/tal_network/include/tal_network_register.h:68-72` 今天长这样：

```c
typedef uint8_t TAL_NETWORK_CARD_TYPE_E;
#define TAL_NET_TYPE_POSIX    (0)
#define TAL_NET_TYPE_PLATFORM (1)
#define TAL_NET_TYPE_AT_MODEM (2)
#define TAL_NET_TYPE_MAX      (3)
```

POSIX / PLATFORM / AT_MODEM 不是三张网卡，是三个 **socket ops 后端**：`tal_posix.c` 的 lwip/socket 实现、`tal_platform.c` 的 tkl 实现、以及一个只有 `#define` 没有实现的 AT 模组占位。同一个头文件里 `TAL_NETWORK_CARD_T` 装的是 `{name, type, ipaddr, ops}` —— 一张 35 个函数指针的函数表加三个元数据字段，也不是网卡。

真正的「网络接口」在 `src/tuya_cloud_service/netmgr/netconn_*`：wifi、wired、cellular，各自有链路状态机、优先级、探测结果。两个概念都叫 card，这是这个组件难以推理的根因。

**重构没有修它，而是绕开了它**：`netconn_desc_t` 的字段叫 `provider`（`netconn_registry.h:273`），`tal_net_route_t` 的字段也叫 `provider`（`tal_net_route.h:35`），`netmgr_link_info_t` 的字段还是叫 `provider`（`netmgr_priv.h:261`），但数据面的类型仍然叫 `CARD`，netmgr 的字段仍然叫 `card_type`。于是今天 `netmgr.c:1895` 这一行是整棵树里这个矛盾最短的证据：

```c
conn->card_type = desc->provider;
```

一次赋值，左边一个词，右边另一个词，同一个概念。**这比统一用任何一个词都糟。**

还有两处树内的自相矛盾，它们是本次命名决策的证据，不是趣闻：

1. **`TAL_NET_PROVIDER_DEFAULT` 已经在 `PROVIDER` 命名空间里了**（`tal_network_register.h:88/90`），而它展开出来的是 `TYPE` 命名空间的值：
   ```c
   #define TAL_NET_PROVIDER_DEFAULT TAL_NET_TYPE_POSIX
   ```
   一个宏名承认了概念叫 provider，它的值却还叫 type。重命名之后这一行会变成 `#define TAL_NET_PROVIDER_DEFAULT TAL_NET_PROVIDER_POSIX` —— 在自己的命名空间里展开。
2. **`TAL_NET_TYPE_PLATFORM` 的名字骗过了写它文档的人。** 在 commit `d69d2c74` 里，`TUYA_T5AI_BOARD_CELLULAR.config` 的头注释写的是 `TAL_NET_TYPE_TKL`，一个**树里不存在的符号**；commit `0dff53e0` 把它改成了 `TAL_NET_TYPE_PLATFORM` 并留下一句自白（`apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config:11-13`）：

   > `TAL_NET_TYPE_PLATFORM` (value 1) ... The macro is PLATFORM, not TKL - an earlier version of this comment named a symbol that does not exist

   ```
   git log --all -S'TAL_NET_TYPE_TKL' --oneline --name-only
   git grep -n 'TAL_NET_TYPE_TKL'      # 今天全树零命中
   ```

---

## 2. 影响面，实测

### 2.1 逐标识符

排除 `docs/`（文档在 §2.6 单独算）。「文件」列是**出现该标识符的文件数**，「行」列是**命中行数**。

```bash
# 逐标识符：文件数（总 / src / boards+platform / apps+examples）与行数
for id in TAL_NETWORK_CARD_T TAL_NETWORK_CARD_TYPE_E TAL_NET_TYPE_POSIX \
          TAL_NET_TYPE_PLATFORM TAL_NET_TYPE_AT_MODEM TAL_NET_TYPE_MAX \
          TAL_NET_PROVIDER_DEFAULT TAL_NETWORK_CARD_DEFAULT TAL_NETWORK_CARD_MANAGER_T \
          tal_network_card_manager tal_network_card_init tal_network_card_set_active \
          tal_network_card_get_active_type tal_network_get_active_ops \
          tal_network_card_set_active_ip tal_network_card_get_active_ip; do
  printf '%-34s %3s %3s %3s %3s %4s\n' "$id" \
    "$(git grep -lw "$id" -- ':!docs' | wc -l)" \
    "$(git grep -lw "$id" -- 'src'    | wc -l)" \
    "$(git grep -lw "$id" -- 'boards' 'platform' | wc -l)" \
    "$(git grep -lw "$id" -- 'apps' 'examples'   | wc -l)" \
    "$(git grep -cw "$id" -- ':!docs' | awk -F: '{s+=$NF} END {print s+0}')"
done
```

| 标识符 | 文件·总 | 文件·`src/` | 文件·`boards/`+`platform/` | 文件·`apps/`+`examples/` | 行 |
| --- | --- | --- | --- | --- | --- |
| `TAL_NETWORK_CARD_T` | 4 | 4 | 0 | 0 | 8 |
| `TAL_NETWORK_CARD_TYPE_E` | 5 | 5 | 0 | 0 | 10 |
| `TAL_NET_TYPE_POSIX` | 6 | 5 | 0 | 1 | 7 |
| `TAL_NET_TYPE_PLATFORM` | 5 | 4 | 0 | 1 | 6 |
| `TAL_NET_TYPE_AT_MODEM` | 3 | 3 | 0 | 0 | 5 |
| `TAL_NET_TYPE_MAX` | 2 | 2 | 0 | 0 | 5 |
| `TAL_NET_PROVIDER_DEFAULT` | 11 | 9 | 0 | 2 | 24 |
| `TAL_NETWORK_CARD_DEFAULT` | 1 | 1 | 0 | 0 | 3 |
| `TAL_NETWORK_CARD_MANAGER_T` | 1 | 1 | 0 | 0 | 2 |
| `tal_network_card_manager` | 1 | 1 | 0 | 0 | 9 |
| `tal_network_card_init` | 4 | 4 | 0 | 0 | 7 |
| `tal_network_card_set_active` | 3 | 3 | 0 | 0 | 4 |
| `tal_network_card_get_active_type` | 4 | 4 | 0 | 0 | 6 |
| `tal_network_get_active_ops` | 3 | 3 | 0 | 0 | 8 |
| `tal_network_card_set_active_ip` | 2 | 2 | 0 | 0 | 2 |
| `tal_network_card_get_active_ip` | 3 | 3 | 0 | 0 | 3 |

`apps/`+`examples/` 那一列的非零项**全部是 `.config` 文件的头注释**，不是代码：

```bash
git grep -nw -e TAL_NET_TYPE_POSIX -e TAL_NET_TYPE_PLATFORM -e TAL_NET_PROVIDER_DEFAULT -- 'apps' 'examples'
# apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config:11,12
# apps/tuya_cloud/switch_demo/config/Ubuntu.config:9,10
```

第 17 个标识符是结构体成员 `card_type`，它需要单独限定范围（裸词在树里到处都有，见 §2.5）：

```bash
git grep -lw card_type -- 'src/tuya_cloud_service/netmgr' | wc -l   # 7
git grep -cw card_type -- 'src/tuya_cloud_service/netmgr'
```

| 文件 | 行 |
| --- | --- |
| `netmgr.h` | 1 |
| `netmgr_priv.h` | 1 |
| `netconn_registry.h` | 5 |
| `netmgr.c` | 5 |
| `netconn_wifi.c` | 2 |
| `netconn_wired.c` | 1 |
| `netconn_cellular.c` | 1 |
| **合计** | **16** |

### 2.2 并集：13 个文件，96 行，全部在 `src/`

**这条 PAT 本身在执行中被证伪过一次，先说这个，再看数字。** 写作时的 PAT 是：

```
TAL_NETWORK_CARD_T|TAL_NETWORK_CARD_TYPE_E|TAL_NETWORK_CARD_MANAGER_T|TAL_NETWORK_CARD_DEFAULT|tal_network_card_[a-z_]*|TAL_NET_TYPE_[A-Z_]+|card_type
```

两个缺陷，都在真的拿它做重命名时才暴露：

1. `tal_network_card_[a-z_]*` 要求 `tal_network_card_` 后面紧跟这个词干，但 `tal_network_get_active_ops` 中间插了 `get_`，没有这一段就匹配不上；结构体成员 `active_card` 同理（`card` 在词尾，不在 `tal_network_card_` 这个前缀里）。S2a 落地时才发现 `src/tal_network/` 单独一个目录漏了 9 行没改全，其中 4 行是 `tal_network.c` 的热路径调用点，2 行还在 `TAL_NET_EXEC_OP` 宏里——即全部 socket 原语共用的那条路径（`413fe17d refactor(tal_network): move the data plane onto the provider names`，commit message 里写的是「49 行的计划，实测 58 行」）。
2. `TAL_NET_TYPE_[A-Z_]+` 要求后面接标识符字符，但散文里指「这一族常量」时用的是字面的 `TAL_NET_TYPE_*`（星号），根本不是标识符，匹配不上。S2a、S2b 都拿这条 PAT 做过完整性检查并且通过了，实际漏了 4 行：`tal_net_route.h` 两处、`netconn_registry.h` 一处、`netmgr.h` 一处，直到 S2c 之后专门核对才发现并改掉（`d807df8a refactor(tal_network): finish the four mentions the S2 pattern could not match`）。

把 PAT 换成：

```
PAT='TAL_NETWORK_CARD|tal_network_card_[a-z_]*|tal_network_get_active_ops|TAL_NET_TYPE_|active_card|card_type'
```

在执行起点 `3b55d419` 上重新测量（`git grep <commit>` 是唯一正确的用法——工作区已经按新名字改完了，不能拿工作区测这条历史 PAT）：

```bash
git grep -lE "$PAT" 3b55d419 -- '*.c' '*.h' ':!docs' ':!apps/games' | wc -l   # 13
git grep -cE "$PAT" 3b55d419 -- '*.c' '*.h' ':!docs' ':!apps/games'
git grep -lE "$PAT" 3b55d419 -- 'boards' 'platform' | wc -l                   # 0
```

（`:!apps/games` 是必须的：`apps/games/lvgl_games` 里有一套跟本事无关的 `card_type` / `card_typedef`，见 §2.5。）

| 文件 | 行 |
| --- | --- |
| `src/tal_network/include/tal_net_route.h` | 2 |
| `src/tal_network/include/tal_network_register.h` | 15 |
| `src/tal_network/src/tal_network_register.c` | 38 |
| `src/tal_network/src/tal_platform.c` | 2 |
| `src/tal_network/src/tal_posix.c` | 2 |
| `src/tal_network/src/tal_network.c` | 5 |
| `src/tuya_cloud_service/netmgr/netmgr.c` | 7 |
| `src/tuya_cloud_service/netmgr/include/netconn_registry.h` | 11 |
| `src/tuya_cloud_service/netmgr/netconn_wifi.c` | 7 |
| `src/tuya_cloud_service/netmgr/include/netmgr.h` | 3 |
| `src/tuya_cloud_service/netmgr/netmgr_priv.h` | 2 |
| `src/tuya_cloud_service/netmgr/netconn_cellular.c` | 1 |
| `src/tuya_cloud_service/netmgr/netconn_wired.c` | 1 |
| **合计** | **96** |

**这个 pattern 缺陷本身，是这次执行最值得记的一条教训。** 用标识符拼出来的 pattern，会忘记散文引用的是标识符「族」（`TAL_NET_TYPE_*`），也会忘记同一族里前缀不规整的成员（`tal_network_get_active_ops` 没有 `card_` 这一段）。而 §4.3 把这条 grep 称为「S2 完成度唯一的机械证明」——**一条有缺陷的完整性证明，比没有证明更危险，因为它会给出通过的信号。** S2a、S2b 当时都是绿的。

**这 96 行里有相当一部分是注释。** 这不是可以少改的部分：注释里那些「card」正是这次要修的东西，它们比代码更容易骗人（§1 里那个不存在的符号就是一条注释；§2.2 上面那条被证伪的 PAT 漏掉的也几乎全是注释）。

分布上有一个对计划很重要的事实：`src/tal_network/`（含 `tal_net_route.h`）64 行、`src/tuya_cloud_service/netmgr/` 32 行，两半可以各自独立编过（§4.3）。

### 2.3 `netmgr.h` 的 include 者：44 是对的

三个传闻里只有一个跟这件事有关，而它是对的。

```bash
# 参与编译的文件（.c/.h），这是「多少文件 include 它」唯一有意义的读法
git grep -l '#include "netmgr.h"' -- '*.c' '*.h' | wc -l              # 44
git grep -l '#include "netmgr.h"' -- 'src/*.c' 'src/*.h' | wc -l      # 18
git grep -l '#include "netmgr.h"' -- 'apps/*.c' 'apps/*.h' | wc -l    # 20
git grep -l '#include "netmgr.h"' -- 'examples/*.c' 'examples/*.h' | wc -l  # 6
git grep -l '#include "netmgr.h"' -- 'boards' 'platform' | wc -l      # 0
git grep -l '#include "netmgr.h"' -- '*.c' '*.h' ':!src' | wc -l      # 26
```

| | 文件数 |
| --- | --- |
| `src/` | 18 |
| `apps/` | 20 |
| `examples/` | 6 |
| `boards/` + `platform/` | 0 |
| **合计（参与编译）** | **44** |
| **其中在 `src/` 外** | **26** |

不加 `'*.c' '*.h'` 限定会得到 **45**，多出来的第 45 个是 `docs/netmgr/extension_guide.md` —— 文档里引了一段 include 骨架。它不参与编译，不该计入。

结论，逐条：

- **「44」是对的。** 而且在磁盘上（含未跟踪文件）重数一遍还是 44：
  ```bash
  grep -rl '#include "netmgr.h"' --include=*.c --include=*.h . | wc -l   # 44
  ```
- **但这句话不在 `netmgr.h` 里。** 它在四个别的地方，四处都对：
  ```bash
  git grep -n '44 files' -- 'src'
  # src/tuya_cloud_service/netmgr/include/netconn_registry.h:21
  # src/tuya_cloud_service/netmgr/netmgr.c:437
  # src/tuya_cloud_service/netmgr/netmgr_priv.h:47
  # src/tuya_cloud_service/netmgr/netmgr_priv.h:195
  ```
  `netmgr.h` 自己一个数字都没写。**没有需要更正的已提交声明** —— 「`netmgr.h` 自己的注释说 44」这个印象本身是记错了位置，不是记错了数字。
- **「40 多个」也是对的。** 扩展指南 §2.3（`docs/netmgr/extension_guide.md:147`）写的是「`netmgr.h` 被树里 40 多个文件 include」，44 落在这句话里。
- **「32」与本事无关。** 树里的 32 是 `netconn_attr_mask_t` 的位宽上限（`docs/netmgr/extension_guide.md:183/195/197`、`netconn_registry.h` 的编译期断言），限制的是 `netmgr_conn_config_type_e` 的**命令个数**，不是任何 include 计数。
- **这四处「44」会随这次重命名一起漂。** S2 不动 `netmgr.h` 的 include 者集合，所以 44 不会变；但如果将来有人拆 `netmgr.h`，这四处要一起改。它们已经被数过一次了，本文是第二次 —— **第三次应该改成一条命令而不是一个数字**（建议：把注释改成「a header the whole tree includes」，把精确数字留给这份文档和它的命令）。

### 2.4 `git grep` 看不见的两块地方

这一节比上面那些数字更重要，因为它决定了 §4.7 的结论。

**（1）`platform/*/` 被 `.gitignore` 排除。**

```bash
git ls-files platform/ | wc -l                 # 1  （只有 platform_config.yaml）
git ls-files platform/T5AI | wc -l             # 0
find platform/T5AI -type f | wc -l             # 13321
git check-ignore -v platform/T5AI              # .gitignore:34:/platform/*/
```

也就是说：**任何只用 `git grep` 得出的「`platform/` 命中 0」都是无效结论**，因为 git 根本没在看那里。必须用磁盘 grep 重做一遍：

```bash
grep -rlE 'TAL_NETWORK_CARD_T|TAL_NETWORK_CARD_TYPE_E|tal_network_card_|TAL_NET_TYPE_' platform/ boards/ | wc -l   # 3
```

3 个命中，**全部是构建产物**：`platform/LINUX/build/switch_demo`（ELF）、`platform/T2/t2_os/tuya.map`、`platform/BK7231X/beken_os/tuya.map`。**厂商适配层源码里一处引用都没有**，这是这次重命名唯一一个真正的好消息：vendor 树不参与。

**（2）树外调用方，谁也数不出来。** 见 §4.7。

### 2.5 机械替换的碰撞名单

`card_type` 这个裸词在磁盘上出现在 **31 个 `.c`/`.h`** 里，其中只有 **7 个**是 netmgr 的：

```bash
grep -rlwE 'card_type' --include=*.c --include=*.h . | wc -l    # 31
```

剩下 24 个必须一个都不能碰：

| 位置 | 是什么 |
| --- | --- |
| `apps/games/lvgl_games/src/pvz/pvz.c`（2 行） | 植物大战僵尸的卡片类型枚举 |
| `platform/LINUX/.../alsa/include/**/hdspm.h`（8 个文件） | ALSA 的 RME HDSPM 声卡头文件 |
| `platform/{T2,T3,T5AI,BK7231X}/**/sdcard.c`、`sd_card_types.h`、`sd_card_driver.c`、`fs_init.c`、`platform/ESP32/**/sdmmc_mmc.c`（14 个文件） | SD 卡驱动 |

**结论：S2 禁止全树 `sed`。** 替换路径要么显式列出那 7 个文件，要么把范围限定成 `src/tuya_cloud_service/netmgr` + `src/tal_network` 两个目录。一个 `sed -i 's/\bcard_type\b/provider/g' $(grep -rl ...)` 会安静地改坏一个 LVGL 小游戏和十四个 SD 卡驱动，而两个验证 target 里没有一个会编到它们 —— **这正是「编过了」证明不了完整性的第一个例子**。

### 2.6 文档也要跟着改

```bash
git grep -cE "$PAT" -- 'docs'
# docs/netmgr/extension_guide.md:35
# docs/netmgr/release_notes.md:2
```

扩展指南 §3 整节（「加一个 socket provider」）在讲这个概念，35 行命中里包括两段代码块的原文引用。**它们必须和代码在同一个 commit 里改**，否则指南会教人写一个不存在的符号 —— 这个错误这棵树已经犯过一次了（§1 第 2 条）。

---

## 3. 新名字

### 3.1 为什么是 `provider`，以及它是否也适合做类型名

`provider` 不是新造的词，它已经是这棵树表达这个概念的词：

| 位置 | 用法 |
| --- | --- |
| `netconn_registry.h:273` | `uint8_t provider;`（描述符字段） |
| `tal_net_route.h:35` | `uint8_t provider;`（route 的一半） |
| `netmgr_priv.h:261` | `uint8_t provider;`（CLI 快照字段） |
| `tal_network_register.h:88/90` | `TAL_NET_PROVIDER_DEFAULT` |
| `netconn_table.c:161/185/198` | `.provider = TAL_NET_PROVIDER_DEFAULT` |
| `netmgr.c:646` | `__netmgr_snap_provider()` |
| `docs/netmgr/extension_guide.md:355` | 「§3 扩展路径二：**加一个 socket provider**」 |

**它适合做类型名，理由是它已经在做这件事了。** 三个字段、一个宏、一个函数、一整节文档都叫它 provider；今天唯一还叫 card 的是**类型和数据面的内部命名**。把类型也改成 provider，不是引入一个新词，而是**删掉一个多余的词**。字段名和类型名用同一个词，是 §1 那个 `conn->card_type = desc->provider;` 唯一的解。

反过来考虑过 `backend`（`tal_network_register.h:74-76` 的注释就在用它：「The socket ops backend this build talks to」）。否决理由：`backend` 在这棵树里还被 netmgr_probe 用来指探测后端（`netmgr_probe_backend_set()`、`netmgr_probe_backend_t`），把 socket 后端也叫 backend 会在同一个模块里造出第二次「一个词两个概念」—— 那正是这次要修的病。**`provider` 只有一个含义，保持这样。**

### 3.2 大小写：跟最新的走，不跟最旧的走

`tal_network` 这个模块里两种风格并存：老的 `TAL_NETWORK_OPS_T` / `TAL_NETWORK_CARD_T`（TuyaOS 风格的全大写 `_T`），新的 `tal_net_route_t`（本次重构在 `tal_net_route.h` 里引入）。netmgr 整个模块是小写（`netmgr_conn_base_t`、`netconn_desc_t`、`netmgr_link_view_t`）。

**用小写。** 理由是方向性：小写是这个模块新增代码的既有选择，而这次重命名的产物会和 `tal_net_route_t` 放在同一段代码里被同时读到（`tal_net_route_set()` 的参数就是 provider）。代价是废弃窗口期间一个头文件里两种风格并存 —— 这个代价是有限的、且在 S4 结束。

### 3.3 映射表

**数值一个都不变。** 0/1/2/`MAX`=3 全程不动，这是 §4.7 那条最重要约束的落点。

| 今天 | 建议 | 说明 |
| --- | --- | --- |
| `TAL_NETWORK_CARD_TYPE_E` | `tal_net_provider_id_t` | 「哪一个 provider」的标识符。仍然是 `typedef uint8_t`，见 §3.6 |
| `TAL_NETWORK_CARD_T` | `tal_net_provider_t` | provider 本体：`{name, type, ipaddr, ops}`。和 `_id_t` 分开，因为一个是记录一个是索引 |
| `TAL_NETWORK_CARD_T.type` | **不变** | C 的结构体成员名没有别名机制——这正是 §3.5 论证 `.card_type` 是「唯一没有别名可用的一项」时用的理由，但 §3.5 漏了 `.type`，它是**第二个**。一旦 `TAL_NETWORK_CARD_T` typedef 成 `tal_net_provider_t`，`tal_platform.c:47` 和 `tal_posix.c:1131` 的 `.type = ...` 立刻编不过，S1 就得去碰调用方——而「不碰任何调用方」正是 §4.1 的 gate，也是 S1 存在的全部理由。这个字段只写不读，已经排在「S4 之后删三个只写不读字段」里；给一个马上要删的字段改名，换来的是第二个没有兜底的破坏点，收益为零 |
| `TAL_NETWORK_CARD_MANAGER_T` | `tal_net_provider_registry_t` | 文件内部类型 |
| `tal_network_card_manager` | `tal_net_provider_registry` | 它今天是**非 `static`** 的全局（`nm` 里是 `D tal_network_card_manager`），给一个有外部链接的符号加 `s_` 前缀是撒谎。S2a 改成这个名字（保留模块前缀，如实反映它有外部链接）；`s_provider_registry` 留给 §6.2 那个单独的、排在 S4 之后的「加 `static`」commit——加 `static` 是链接属性变更，不是重命名，见 §6 |
| `TAL_NETWORK_CARD_MANAGER_T.active_card[]` | `.providers[]` | |
| `TAL_NETWORK_CARD_DEFAULT` | `TAL_NET_PROVIDER_DEFAULT_OBJ` | 指**对象**（`tal_net_provider_posix` / `_tkl`），与指**标识符**的 `TAL_NET_PROVIDER_DEFAULT` 区分开。今天这两个名字差一个 `CARD`/`PROVIDER`，读起来像同一个东西的两种写法，其实一个是 id 一个是对象 |
| `tal_network_card_posix` | `tal_net_provider_posix` | |
| `tal_network_card_platform` | `tal_net_provider_tkl` | 见 §3.4 |
| `TAL_NET_TYPE_POSIX` (0) | `TAL_NET_PROVIDER_POSIX` (0) | |
| `TAL_NET_TYPE_PLATFORM` (1) | `TAL_NET_PROVIDER_TKL` (1) | 见 §3.4 |
| `TAL_NET_TYPE_AT_MODEM` (2) | `TAL_NET_PROVIDER_AT_MODEM` (2) | 只重命名，**不删**，见 §6 |
| `TAL_NET_TYPE_MAX` (3) | `TAL_NET_PROVIDER_MAX` (3) | |
| `TAL_NET_PROVIDER_DEFAULT` | **不变** | 它已经对了；改完之后它在自己的命名空间里展开 |
| `tal_network_card_init()` | `tal_net_provider_init()` | |
| `tal_network_get_active_ops()` | `tal_net_provider_ops()` | 4 个调用点（`tal_network.c:54,87,415,787`），其中 `:54`/`:87` 在 `TAL_NET_EXEC_OP` 宏里，即全部 socket 原语的热路径 |
| `tal_network_card_get_active_ip()` | `tal_net_route_src_ip()` | 1 个调用点（`tal_network.c:347`）。它读的是 route 的一半，名字应该说 route 而不是 card |
| `tal_network_card_set_active()` | **不重命名**，S4 删除 | 零调用方（§5.2） |
| `tal_network_card_get_active_type()` | **不重命名**，S4 删除 | 零调用方（§5.2） |
| `tal_network_card_set_active_ip()` | **不重命名**，S4 删除 | 零调用方（§5.2） |
| `netmgr_conn_base_t.card_type` | `.provider` | 唯一没有别名机制的一项，见 §3.5 |
| `netconn_desc_t.provider` | **不变** | |
| `tal_net_route_t.provider` | **不变** | |
| `netmgr_link_info_t.provider` | **不变** | |
| `TAL_NETWORK_OPS_T` | **不变** | 它本来就叫 ops，名字里没有 card |
| `tal_network_register.[ch]`、`tal_platform.c` | **文件名不改**，见 §6 |

三个零调用方函数**不重命名而是排队删除**，理由：`tal_network_register.c:7` 已经把它们的身份写清楚了 ——「the `tal_network_card_*` entry points are **compatibility wrappers** over that same state」。给一个兼容 wrapper 起一个新名字，等于把它变成一个新的公开 API，这跟它存在的理由相反。留旧名字到 S4，然后一起删。

### 3.4 枚举值要改，而 `PLATFORM` → `TKL` 有四条独立证据

值要不要改？**要。** `TAL_NET_PROVIDER_DEFAULT` 展开成 `TAL_NET_TYPE_POSIX` 是 §1 那个矛盾在宏层面的复现；只改类型不改值，`TAL_NET_PROVIDER_*` 和 `TAL_NET_TYPE_*` 会永久并存，读代码的人得记住哪个是哪个。

`PLATFORM` 尤其要改，因为它**不表达它的意思**。「PLATFORM」在这棵树的其他地方指「某个芯片平台」（`platform/T5AI`、`platform_config.yaml`、`tos.py` 的 platform 概念）。这里它指的是「tkl 那一层」。四条证据：

1. **那张 card 自己叫 tkl。** `src/tal_network/src/tal_platform.c:46` —— `.name = "tkl"`。
2. **头文件注释叫它 tkl。** `tal_network_register.h:76` —— 「the platform **tkl** layer otherwise」。
3. **实现全是 tkl。** `tal_platform.c` 的 35 个函数指针全部接到 `tkl_net_*`；文件顶上是 `#define NET_USING_TKL 1`。
4. **写文档的人第一反应就是写 TKL。** commit `d69d2c74` 的 config 注释写的是 `TAL_NET_TYPE_TKL`，一个不存在的符号，到 `0dff53e0` 才被更正。**一个名字如果连给它写文档的人都会写错，那是名字的问题，不是那个人的问题。**

`POSIX` 保留（那张 card `.name = "posix"`，一致）。`AT_MODEM` 保留（它没有实现，但语义清楚）。

### 3.5 `netmgr_conn_base_t.card_type`：唯一没有别名可用的一项

`netmgr.h:101`：

```c
typedef struct netmgr_conn_base {
    uint8_t pri;
    netmgr_type_e type;
    netmgr_status_e status;
    /* ... Typed uint8_t, the very type TAL_NETWORK_CARD_TYPE_E is a typedef of, so
     * this control-plane header needs no include from the data plane. */
    uint8_t card_type;
    ...
} netmgr_conn_base_t;
```

C 语言里 typedef 可以别名，宏可以转发，**结构体成员名不能**。理论上能用匿名 union 同时暴露两个名字，但那是为一个纯命名问题给一个公开结构体加一个 ABI 形状，而且 C11 匿名成员在这套工具链矩阵里不是无条件可用的。**否决。**

`.card_type` 不是唯一一个没有别名机制的成员，`TAL_NETWORK_CARD_T.type` 是另一个（见 §3.3），处置方式相同：不改名。

树内的暴露面是零：

```bash
git grep -nw netmgr_conn_base_t -- 'apps' 'examples' 'boards'                # 空
git grep -nwE 'netmgr_conn_(wifi|wired|cellular)_t' -- 'apps' 'examples' 'boards'  # 空
```

44 个 include `netmgr.h` 的文件里，26 个在 `src/` 外，而它们用到的是 `netmgr_init` / `netmgr_conn_get` / `netmgr_conn_set` / `netmgr_type_e` / `netmgr_status_e` / `NETCONN_*` 命令枚举 —— **没有一个碰 `netmgr_conn_base_t`**：

```bash
git grep -hoE '\b(netmgr_[a-z_]+|NETMGR_[A-Z_]+|NETCONN_[A-Z_]+|netconn_[a-z_]+)\b' -- 'apps/*.c' 'examples/*.c' \
  | sort | uniq -c | sort -rn
```

**但树外暴露面不是零，而且恰好落在这个字段上。** 扩展指南 §2.7 的驱动骨架教的就是在链路驱动的静态初始化器里写这一行，树内三个驱动都是这个形状：

```
src/tuya_cloud_service/netmgr/netconn_wifi.c:61        .card_type = TAL_NET_PROVIDER_DEFAULT,
src/tuya_cloud_service/netmgr/netconn_wired.c:35        .card_type = TAL_NET_PROVIDER_DEFAULT,
src/tuya_cloud_service/netmgr/netconn_cellular.c:45     .card_type = TAL_NET_PROVIDER_DEFAULT,
```

任何照指南写过树外链路驱动的板子，代码里都有这一行。**所以：整个废弃窗口的长度由这一个字段决定，不由那些能编过的东西决定。** 见 §4.7。

### 3.6 明确不做的两件事

1. **不把 `tal_net_provider_id_t` 变成真枚举。** 今天 `typedef uint8_t` 不是懒惰，是一条被写下来的设计约束：`netmgr.h:98-101` 和 `netconn_registry.h:269-271` 都说明了字段用 `uint8_t` 是**为了让控制面头文件不必 include 数据面头文件**。改成 `enum` 就得让 `netmgr.h` include `tal_network_register.h`，那会把 `1767f10b refactor(netmgr): stop the control-plane header depending on the data plane` 撤销掉。想要类型安全是另一件事，且要先解决那个依赖方向。
2. **不重命名文件。** 见 §6。

---

## 4. 落地顺序

四步，加一步准备。每步一句话：**S1 让新名字存在，S2 让树内改用新名字，S3 让旧名字变成一个警告，S4 让旧名字消失。** 前三步对树外调用方是完全兼容的（一个例外，见 S1），只有 S4 会打破它们，所以只有 S4 需要一次版本公告。

### 4.0 S0：准备，不改代码

- 把 §2 每一条命令重跑一遍，确认基线没漂。数字变了就先更新本文，再动手。
- 在**动手之前**先把两个验证 target 各编一次，留下基线日志。一次「本来就编不过」被当成「重命名弄坏了」，会让整个 S2 无法二分。

**Gate：两个 target 在 `0dff53e0` 上编过。**

### 4.1 S1：引入新名字，旧名字变别名（一个 commit，只改 `tal_network_register.h`）

```c
/* --- 新名字 --- */
typedef uint8_t tal_net_provider_id_t;
#define TAL_NET_PROVIDER_POSIX    (0)
#define TAL_NET_PROVIDER_TKL      (1)
#define TAL_NET_PROVIDER_AT_MODEM (2)
#define TAL_NET_PROVIDER_MAX      (3)

typedef struct {
    char                  name[16];
    tal_net_provider_id_t type;   /* 不改名，见 §3.3 */
    TUYA_IP_ADDR_T        ipaddr;
    TAL_NETWORK_OPS_T     ops;
} tal_net_provider_t;

OPERATE_RET        tal_net_provider_init(void);
TAL_NETWORK_OPS_T *tal_net_provider_ops(void);
TUYA_IP_ADDR_T     tal_net_route_src_ip(void);

/* --- 旧名字：别名。数值不变。 --- */
typedef tal_net_provider_id_t TAL_NETWORK_CARD_TYPE_E;
typedef tal_net_provider_t    TAL_NETWORK_CARD_T;

#define TAL_NET_TYPE_POSIX    TAL_NET_PROVIDER_POSIX
#define TAL_NET_TYPE_PLATFORM TAL_NET_PROVIDER_TKL
#define TAL_NET_TYPE_AT_MODEM TAL_NET_PROVIDER_AT_MODEM
#define TAL_NET_TYPE_MAX      TAL_NET_PROVIDER_MAX

#define tal_network_card_init          tal_net_provider_init
#define tal_network_get_active_ops     tal_net_provider_ops
#define tal_network_card_get_active_ip tal_net_route_src_ip
```

`tal_net_provider_t` 的成员保留 `type` 这个名字，不跟着改成 `.id`：结构体成员名没有别名机制，改了会让 `tal_platform.c` / `tal_posix.c` 的初始化器编不过，S1 就得碰调用方——见 §3.3、§3.5。

三个零调用方 wrapper（`tal_network_card_set_active`、`_get_active_type`、`_set_active_ip`）**不动**：它们不改名，S4 直接删。

函数别名用 `#define` 而不是转发 wrapper：wrapper 会在热路径上多一次调用（`tal_net_provider_ops()` 每个 socket 原语都要走一遍），`#define` 是零成本。代价是**符号名在 `.o` 里变了** —— 一个拿旧头文件编出来的 `.o` 去链新的库会缺符号。TuyaOpen 是源码分发、整树重编，所以这个代价实际是零；但它是 S1 唯一一处不是纯增量的地方，**必须写进 release note**，不能靠「反正大家都重编」默认掉。

**Gate：S1 单独编两个 target，且不改任何调用方。** 这就是 S1 存在的全部理由 —— 它把「新名字定义得对不对」和「调用方改得对不对」分成两次可以独立回滚的事件。如果 S1 单独编不过，问题一定在这一个头文件里。

### 4.2 S2：机械重命名（按目录分 commit）

13 个文件、96 行——这两个数字跟着 §2.2 的 PAT 修正一起变了，见该节。**禁止全树 `sed`**（§2.5）。范围限定：

```bash
# 只在这两个目录里替换，不要用 grep -rl 的结果喂 sed
git grep -lE "$PAT" -- 'src/tal_network' 'src/tuya_cloud_service/netmgr'
```

建议的 commit 切分，每一个都能独立编过（因为 S1 的别名还在）：

| commit | 范围 | 文件 / 行 |
| --- | --- | --- |
| S2a | `src/tal_network/` | 6 / 64 |
| S2b | `src/tuya_cloud_service/netmgr/`，**除** `.card_type` | 5 / 16 |
| S2c | **只**改 `netmgr_conn_base_t.card_type` → `.provider` | 7 / 16 |
| S2d | `docs/netmgr/extension_guide.md`、`docs/netmgr/release_notes.md` | 2 / 46 |
| S2e | 两个 `.config` 的头注释 | 2 / 2 |

S2c 单独一个 commit，因为它是**唯一一个没有别名兜底的改动**（§3.5）—— review 它的人应该只看它，回滚它的人应该只回滚它。

两条操作注意：

- **clang-format 会动到没改的行。** `.clang-format` 里 `AlignConsecutiveMacros`、`AlignConsecutiveAssignments`、`AlignConsecutiveDeclarations` 都是 `true`，`ColumnLimit` 是 120。把 `TAL_NETWORK_CARD_TYPE_E`（23 字符）换成 `tal_net_provider_id_t`（21 字符）会改变对齐列，于是**相邻的、这次没碰的行**也会被重排。按 M0 的先例把格式化拆成独立 commit（`00cb9215 style(netmgr): clang-format the lines M0 touched`），并且只格式化动过的行。
- **S2e 的两个 `.config` 是被构建改写的文件。** `tos.py build` 会重写 config，所以在提交 S2e 之前必须 `git diff` 确认改动只有注释那几行。这一点跟重命名无关，但它是这两个文件上最容易踩的坑。

**Gate（每个 commit）：两个 target 都能编过 + `python tools/check_format.py` 通过。** 树里没有 netmgr 的单元测试（`git ls-files tests/` 只有 `tests/export/`），所以构建是唯一的机械 gate —— 这也正是 §4.7 要说的事。

### 4.3 S3：别名标记废弃（一个 commit）

```c
typedef tal_net_provider_id_t TAL_NETWORK_CARD_TYPE_E
    __attribute__((deprecated("renamed to tal_net_provider_id_t")));
```

- **typedef 和函数可以被编译器强制。** 树内所有工具链都是 GCC 系（host gcc、T5AI/T3/T2 的 armgcc、ESP32 的 xtensa-gcc、LN882H 的 armgcc），`__attribute__((deprecated))` 全部支持。
- **宏不行。** `TAL_NET_TYPE_*` 是 `#define`，没有可移植的办法给一个宏加废弃警告。**不要用 `#pragma GCC poison`**：它会连注释里的提及一起报错，而这些常量在 `netconn_registry.h`、`netconn_wifi.c` 的注释里被引用了 5 处以上（那些注释是在解释「死掉的 4G 分支」，属于要保留的设计记录）。宏只能靠文档加一个可 grep 的标记注释。**这就是为什么值的别名必须比 typedef 的别名活得更久，也是 S4 不能只看编译器警告的原因。**
- 同时在 `tools/` 下放一个可执行的替换脚本，让树外用户能**跑**，而不是只能读一张表。见 §4.4 第 4 条。

**Gate：两个 target 在 `-Wdeprecated-declarations` 下只报别名相关的告警，且树内零命中旧名字：**

```bash
git grep -nwE 'TAL_NETWORK_CARD_T|TAL_NETWORK_CARD_TYPE_E|tal_network_card_init|tal_network_get_active_ops|tal_network_card_get_active_ip|TAL_NET_TYPE_(POSIX|PLATFORM|AT_MODEM|MAX)' \
  -- ':!src/tal_network/include/tal_network_register.h'
# 期望：空
```

**这是 S2 完成度唯一的机械证明。** 编译成功不能证明 S2 改全了（别名还在，漏改的地方照样编过）；上面这条 grep 能。

### 4.4 S4：删除别名（一个独立 PR，在一次带 release note 的 tag 之后）

内容：
1. 删掉 S1 加的所有别名。
2. 删掉三个零调用方 wrapper（§5.2）。
3. 给 `release_notes.md` 加一节，附**完整的旧名→新名映射表**（就是 §3.3）。
4. **删 wrapper 的同时，扫一遍点名它们的注释。** 三个函数删掉之后，函数不在了，但树里还有一批注释在**点名它们**——那些注释描述的会是不存在的函数。先把清单跑出来：
   ```bash
   git grep -nE 'tal_network_card_(set_active|get_active_type|set_active_ip)' -- src docs
   ```
   HEAD 上命中的、真正需要处理的注释（不含三个函数自己的声明/定义，那两处随 wrapper 一起删）：
   - `src/tal_network/src/tal_network_register.c:204`
   - `src/tuya_cloud_service/netmgr/include/netconn_registry.h:139,142,144`
   - `src/tuya_cloud_service/netmgr/netconn_wifi.c:34,397`
   - `docs/netmgr/extension_guide.md:62,411,412,413,793,800,807`（本次不改，留给 S4）

   这些注释记录的是一个被绕开的设计错误——控制面向数据面问了一个控制面问题，而且那个条件是恒真式（`netconn_registry.h:144`、`83b5f005`、`407bccd2`）——**内容要保留**，但不能再点名已经不存在的函数。

**Gate：**
- 两个 target 编过；
- 全树（含 `docs/` 与 `.config` 注释）旧名字零命中：
  ```bash
  git grep -nwE 'TAL_NETWORK_CARD|tal_network_card|TAL_NET_TYPE_' ; echo "expect: empty"
  ```
  这条 grep 到 S4 之前**不能**包含三个 wrapper 的名字，正是因为它们要到 S4 才删（S3 的完整性 gate，§4.3，特意把 `tal_network_card_set_active` 等排除在检查范围外）；S4 之后这条 grep 要**加上**它们，和上面第 4 条的清单一起验证为空。
- **而且这个 gate 不足以决定 S4 什么时候做**，见下。

### 4.5 编译证明不了什么，以及这对废弃窗口意味着什么

两个 target 合起来覆盖两个 socket 后端，这对这次重命名恰好是对的形状 —— 被改名的东西正好按 `TAL_NET_PROVIDER_DEFAULT` 那个 `#if` 分岔，两个 target 各走一条分支，各有一个**有内容**的 provider 文件（`tal_posix.c` / `tal_platform.c`）：

| config | 平台 | provider | 有内容的 provider 文件 |
| --- | --- | --- | --- |
| `apps/tuya_cloud/switch_demo/config/Ubuntu.config` | LINUX / host | posix (0) | `tal_posix.c` |
| `apps/tuya_cloud/switch_demo/config/TUYA_T5AI_BOARD_CELLULAR.config` | T5AI / `TUYA_T5AI_BOARD` | tkl (1) | `tal_platform.c` |

**实测：两个 target 都编了 `tal_posix.c` 和 `tal_platform.c`，不是各编一个。**

```bash
grep -oE 'tal_(posix|platform)\.c\.o' ~/.cache/claude/logs/s0-ubuntu.log | sort -u
# tal_platform.c.o
# tal_posix.c.o
grep -oE 'tal_(posix|platform)\.c\.o' ~/.cache/claude/logs/s0-t5ai.log | sort -u
# tal_platform.c.o
# tal_posix.c.o
```

`src/tal_network/CMakeLists.txt` 用 `aux_source_directory` 全收目录下的源文件，两个文件各自用同一个 `ENABLE_LIBLWIP` / `OPERATING_SYSTEM` 判断把**自己的整个函数体**条件编译掉（`tal_posix.c` 是 `#if defined(NET_USING_POSIX)`，`tal_platform.c` 是 `#if defined(NET_USING_TKL)`），所以两个都进编译单元，只有一个有内容。结论不变——被 `#if` 关掉的那一半不过语法检查，所以两个 target 仍然都必须编——但上一版「各编一个 card 定义文件」的措辞是不准的，改成上表这样。

（两个 target 的完整覆盖差异见 [`release_notes.md`](release_notes.md) §8，不在这里重复。）

**但它们能证明的只有「树内完整」这一件事。** 三个具体的盲区：

1. **树外 app 数不出来。** 树内 44 个 `netmgr.h` include 者里 26 个在 `src/` 外，那 26 个全是 Tuya 自己的 app 和 example。一个客户产品的 app 目录不在这个 repo 里，`git grep` 永远看不到它。
2. **`platform/*/` 连 git 都看不到**（§2.4）。这次侥幸是 0 命中，但那个 0 是靠磁盘 grep 得到的，不是靠 `git grep`。下一个重命名不一定这么幸运。
3. **`.card_type` 是树外真正会撞上的那一项**（§3.5）：扩展指南教的驱动骨架里就有这一行，而结构体成员名没有别名机制。

所以：

- **废弃窗口的长度由 `.card_type` 决定，不由「能编过」决定。** 它至少要跨一个带 release note 的 tag。S3 到 S4 之间不是「等 CI 绿」，是「等一次版本公告出去，并且给出去的时间足够长」。
- **S4 不能由树内状态触发。** §4.4 那条 grep 是 S4 的**前提**，不是 S4 的**理由**。理由只能是时间加公告。
- **数值必须全程不变。** 这是整份计划里最重要的一条约束，理由不对称：别名消失是一次**响亮的编译错误**，树外调用方立刻知道；数值换意思是一次**静默的错误行为**，没人会知道。同理，**旧名字永远不许被复用去指别的东西** —— 那是这份计划唯一可能造成的不可发现的破坏。
- **给脚本，不只给 changelog。** S3 要在 `tools/` 下留一个可执行的替换脚本，范围限定在调用方自己的目录，并且**在脚本里带上 §2.5 的碰撞名单**（`card_type` 是个太常见的词）。一份只能人读的映射表，等于把 §2.5 那个坑原样交给每一个树外用户去踩。

---

## 5. 树里已有的别名与死符号（与重命名分开处理）

这一节是**独立清单**，不是重命名的一部分。放在这里是因为盘点重命名影响面时顺手把它们数出来了，而这些结论今天只存在于源码注释里。

### 5.1 兼容别名 / 转发宏

| 符号 | 位置 | 引用数 | 建议 |
| --- | --- | --- | --- |
| `CELLULAR_STAT_E` → `TAL_CELLULAR_STAT_E` | `netconn_cellular.h:27` | 2（自己的 `#define` + `netconn_cellular.c:57` 一处使用） | **删。** 一个只用了一次的转发宏，把一个 TAL 层类型伪装成 netmgr 层类型。改法：`netconn_cellular.c:57` 直接用 `TAL_CELLULAR_STAT_E`，删掉 `#define`。两行。**但不要放进重命名 PR。** |
| `TAL_NETWORK_CARD_DEFAULT` → `tal_network_card_posix` / `_platform` | `tal_network_register.c:53/56`，用在 `:64` | 3（1 个文件，文件内私有） | **留，跟着重命名改成 `TAL_NET_PROVIDER_DEFAULT_OBJ`。** 它存在的理由和 `TAL_NET_PROVIDER_DEFAULT` 一样：让 `ENABLE_LIBLWIP`/`OPERATING_SYSTEM` 那个判断只出现在一个地方 |
| `NETMGR_POLICY_DEFAULT_MIN_DWELL_MS` → `NETMGR_POLICY_MIN_DWELL_MS` | `netmgr_policy.h:668-671` | 2 处定义 + `:709` 一处使用 | **留。** 它不是重命名别名，是 Kconfig 项缺失时的兜底（`#if defined(...)` / `#else 0`） |
| `TAL_NETWORK_CARD_TYPE_E` = `typedef uint8_t` | `tal_network_register.h:68` | 见 §2.1 | **留这个形状**，只改名字。它买不到类型安全，但那是故意的（§3.6） |

命令：

```bash
git grep -nE '^#define[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*$' \
  -- 'src/tuya_cloud_service/netmgr' 'src/tal_network'
git grep -nE '^typedef[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]+[A-Za-z_][A-Za-z0-9_]*;' \
  -- 'src/tuya_cloud_service/netmgr' 'src/tal_network'
git grep -nw CELLULAR_STAT_E -- ':!docs'
```

### 5.2 零调用方的函数

「引用」列是**除声明与定义之外**的引用数（注释单独标出）。命令：

```bash
git grep -nw '<symbol>' -- '*.c' '*.h'
```

| 函数 | 声明 / 定义 | 调用方 | 建议 |
| --- | --- | --- | --- |
| `tuya_lan_enable()` | `tuya_lan.c:1470` | **0** | **留。** 不重数 —— [`known_gaps.md`](known_gaps.md) §4 已经确认过零调用方（`:233`、`:240`），而且它正是那一节缺口的**修法所需**（`ap_netcfg_stop()` 应该调它）。删掉它等于删掉那个修法。**不属于本 PR。** |
| `tal_network_card_set_active()` | `tal_network_register.h:106` / `.c:164` | **0**（另有 2 处注释：`.c:190`、`netconn_registry.h:144`，后者明确写了「which has zero callers in the tree」） | **S4 删。** |
| `tal_network_card_get_active_type()` | `tal_network_register.h:108` / `.c:182` | **0**（另有 4 处注释：`netconn_registry.h:139/142`、`netconn_wifi.c:34/397`，都在解释那个死掉的 4G 分支） | **S4 删。** 删函数，**保留那些注释** —— 它们记录的是一个被绕开的设计错误，不是这个函数 |
| `tal_network_card_set_active_ip()` | `tal_network_register.h:123` / `.c:188` | **0**，连注释都没有 | **S4 删。** 这三个里最干净的一个 |
| `netmgr_policy_select_cb_set()` | `netmgr_policy.h:880` / `netmgr.c:2818` | **0** | **留。** 树内零调用方是**设计意图**：它是板级/产品级的排序钩子，扩展指南 §4.1、release notes §4.1 和 §9.8 都在教人用它 |
| `netconn_registry_set_table()` | `netconn_registry.h:328` / `netconn_table.c:274` | **0**（`netmgr.c:3020` 一处注释） | **留。** 同上，板级链路表覆盖入口（扩展指南 §2.8、release notes §4.3） |
| `netmgr_probe_backend_set()` | `netmgr_probe.h:436` / `netmgr.c:3017` | **0**（`netmgr.c:2149` 一处注释） | **留。** 同上，探测后端注册入口（扩展指南 §5.6、release notes §4.2） |
| `netmgr_probe_report_simple()` | `netmgr_probe.h:343` / `netmgr.c:3006` | **0**，无注释引用 | **留一个版本再看。** 这四个里唯一没有文档给它派活的：它是 `netmgr_probe_report()` 的便利包装。如果探测层长期只有一个被动后端，它就是纯多余，可以跟着 S4 一起清 |

**一个容易误判的形状，写在这里免得下一个人重踩。** 下面四个用 `NAME(` 搜会得到 0，但它们**不是死的** —— 它们是在驱动的静态初始化器里被取地址的：

```
netconn_wifi.c:63       .close = netconn_wifi_close,
netconn_wired.c:36      .open  = netconn_wired_open,
netconn_wired.c:37      .close = netconn_wired_close,
netconn_cellular.c:47   .open  = netconn_cellular_open,
```

数「有没有调用方」必须用 `git grep -nw`（不带括号），不能用 `git grep 'NAME('`。

### 5.3 只写不读的字段

`TAL_NETWORK_CARD_T` 四个字段里有三个是只写的：

```bash
git grep -nE '\.(name|type|ipaddr)[[:space:]]*=' -- 'src/tal_network'
# tal_platform.c:46,47,48   tal_posix.c:1130,1131,1132
git grep -nE '(->|\.)(name|ipaddr|type)\b' -- 'src/tal_network/src/tal_network_register.c'
# 只有一处注释，没有任何读取
```

| 字段 | 写 | 读 | 建议 |
| --- | --- | --- | --- |
| `.name` | `tal_platform.c:46`（`"tkl"`）、`tal_posix.c:1130`（`"posix"`） | **0** | **删，但要排在 `PLATFORM`→`TKL` 之后。** 这个字段是 §3.4 第 1 条证据；先删掉它，就把那条证据从树里删掉了 |
| `.type` | `tal_platform.c:47`、`tal_posix.c:1131` | **0** | **删。** 它和 `providers[]` 的下标是同一个信息，重复存了一份，而且没有任何东西校验两者一致 |
| `.ipaddr` | 两处都是 `= 0` | **0** | **删。** 已被 `tal_net_route_t.src_ip` 取代，`tal_network_register.c:30-34` 的注释写明了为什么（多条链路可以共用一个 provider，所以源地址是**激活链路**的属性而不是 provider 的属性） |
| `.ops` | | 热路径 | 留 |

三项都**不进重命名 PR**：删字段是改数据结构，重命名不是。

---

## 6. 这个 PR 里不许有什么

**一个既重命名又修 bug 的 PR 是不可 review 的。** 重命名的 review 方式是「逐个替换看对不对，然后看那条 grep 是不是空」；修 bug 的 review 方式是「这个改动是不是真的修了那个问题」。混在一起，两种 review 都做不了：读的人无法判断某一处改动是重命名的机械后果还是一个有意的行为变更，而这正是重命名唯一的风险来源。

### 6.1 来自 [`known_gaps.md`](known_gaps.md) 的，全部排除

十二项全部在 netmgr 之下（TAL/TKL、LAN、netcfg、`tuya_iot`、`tal_cli`），一项都不许进来。

| 缺口 | 为什么排除 |
| --- | --- |
| §1 porting 模板 `tkl_wired_set_status_cb()` 无保护【活的 bug】 | 改 `tools/porting/`，与本 PR 零交集 |
| §2 `tuya_lan_init()` 三条失败路径返回 `OPRT_OK`【活的 bug】 | 改 LAN 服务。它是活的 bug，所以它该有自己的 PR 和自己的验证，而不是搭一次重命名的车 |
| §3 LAN 与 AI monitor 共用 socket loop 无引用计数【活的 bug】 | 同上，而且它是 §4 的前置 |
| §4 AP 配网关掉 LAN 没人开回来【活的 bug】 | 同上。**注意**：它的修法要用到 `tuya_lan_enable()`，所以 §5.2 里那个零调用方函数不许删 |
| §5 `ENABLE_BLUETOOTH=y` + `ENABLE_WIFI=n` 编不过【活的 bug】 | 这是一个 CMake 门控问题。它会诱人顺手修，因为 `Ubuntu.config` 的注释正在解释它 —— 但改它会改变构建矩阵，那会让 S2 的 gate 不再是同一个 gate |
| §6 `tkl_net_getsockname()` 各平台语义不一致【一半活的】 | 改 `platform/*/`，而 `platform/*/` 被 `.gitignore` 排除（§2.4），本 PR 一个字节都不该碰那里 |
| §7 没有 TAL/TKL 入口撤回 wired 回调【已绕开】 | 改 TKL 接口 × 全平台，是那份文档里影响面最大的一项 |
| §8 `tal_cellular` 没有 deinit / connect-disconnect【已绕开】 | 同上 |
| §9 `tuya_mqtt_stop()` 赋值顺序【已绕开】 | 改 MQTT 服务 |
| §10 `tuya_iot_destroy()` 硬编码三种链路类型【潜在】 | 它在 `tuya_iot.c`，而且改法涉及遍历注册表 —— 是一次行为变更 |
| §11 `tal_cli` 的 `argv` 不清零【潜在】 | 改 `tal_cli` |
| §12 `NETCONN_CAP_METERED` 声明了但内置排序不看它 | 模块内唯一还开着的缺口，改它是改选路语义 |

### 6.2 重命名顺手会想做、但必须分开的

| 诱惑 | 为什么不行 |
| --- | --- |
| 删掉 `TAL_NET_TYPE_AT_MODEM` 并把 `MAX` 从 3 改成 2 | **这是行为变更。** `tal_net_route_set()` 用 `route->provider >= TAL_NET_TYPE_MAX` 做校验（`tal_network_register.c:140`），把 `MAX` 改小会让 provider = 2 从「合法但没实现」变成 `OPRT_INVALID_PARM`。S2 只重命名它，删除另开 |
| 删掉三个零调用方 wrapper（§5.2） | 删公开 API ≠ 重命名。它们排在 **S4**，和别名一起走同一个废弃窗口 |
| 删掉 `TAL_NETWORK_CARD_T` 的 `.name` / `.type` / `.ipaddr`（§5.3） | 改公开结构体布局。而且 `.name`（`"tkl"`）是 §3.4 的证据之一，删它要排在值重命名之后 |
| 删掉 `CELLULAR_STAT_E`（§5.1） | 两行的改动，但它属于 `tal_cellular` 的命名而不是 provider 的命名。单独一个 commit，单独 review |
| 顺手把 `NETMGR_LINK_UP_SWITH` 的拼写修掉 | **这是另一次公开重命名，不是这一次。** 它命中 5 个文件 11 行（`git grep -cw NETMGR_LINK_UP_SWITH -- ':!docs'`），其中 `examples/multimedia/audio_player/music/src/tuya_app_main.c:106` 在 `src/` 外，而且 `netmgr_event.h:306-312` 有一整段注释在解释为什么它至今没改。它需要自己的一份 §4 那样的顺序 |
| 把 `tal_network_card_manager` 改成 `static` | 它今天是非 `static` 的全局，但只在 `tal_network_register.c` 内被引用（9 行，全在同一个文件）。加 `static` 是**链接属性变更**，不是重命名。改名放 S2，加 `static` 另开——那个 commit 同时把它改名成 `s_provider_registry` |
| 把 `tal_network_register.[ch]` 改名成 `tal_net_provider.[ch]`、`tal_platform.c` 改名成 `tal_tkl.c` | **文件重命名会打破树外的 `#include "tal_network_register.h"`**，而且它跟符号重命名的废弃机制完全不同（头文件可以留一个只有 `#include` 的转发壳，源文件不需要转发）。按 `173b54ec refactor(tal_wifi_ulp): rename netmgr.[ch] to ulp_apiq.[ch]` 的先例，文件重命名自己一个 commit，而且要排在 S4 之后 |
| 把 `tal_net_provider_id_t` 改成真 `enum` | 会让 `netmgr.h` 依赖数据面头文件，撤销 `1767f10b`（§3.6） |
| 顺手把 `netmgr_get_active_ops()` 那些不加锁的读者「修好」 | 扩展指南 §3.3 用一整节在说不要这么做（热路径 + 优先级反转）。重命名一个函数不是重新审视它的锁的时机 |
| 全文件 clang-format | 只格式化动过的行，先例是 `00cb9215`（§4.2） |
| 更正那四处「44 files」 | 它们**是对的**（§2.3）。不要「顺手修」一个正确的数字 |

---

## 7. 一页纸清单

执行顺序，逐条：

1. **S0** 重跑 §2 的每条命令，确认数字没漂；把两个 target 各编一次留基线。
2. **S1** 一个 commit，只改 `tal_network_register.h`：加新名字，旧名字变别名，**数值不动**。gate：两个 target 编过，且**没改任何调用方**。
3. **S2a** `src/tal_network/`（6 文件 64 行）。**S2b** `src/tuya_cloud_service/netmgr/` 除 `.card_type`（5 文件 16 行）。**S2c** 只改 `.card_type` → `.provider`（7 文件 16 行，唯一没有别名兜底的一步，单独 commit）。**S2d** 两份文档（46 行）。**S2e** 两个 `.config` 的头注释（2 行，提交前 `git diff` 确认没被构建改写）。格式化单独 commit，只格式化动过的行。gate：每个 commit 之后两个 target 编过 + `tools/check_format.py` 过。

   **实际执行比这个切分多出了三处，都是落地时当场发现当场修，不是重新规划：** S2a 之后多了一个补对齐的 style commit（`64cdd072`——`TAL_NETWORK_CARD_TYPE_E` 换成更短的 `tal_net_provider_id_t` 挪动了 clang-format 的对齐列，S2a 自己漏排了一处）；S2b 之后多了两个 commit：`d807df8a` 补上 §2.2 那条旧 PAT 因为不认 `TAL_NET_TYPE_*` 通配写法而漏掉的 4 行，`83b5f005` 修正一句被 S1 自己的别名弄假的断言（`netconn_registry.h` 原来说 `TAL_NET_TYPE_AT_MODEM` "全树只有它自己的 `#define` 提到过"，S1 落地的废弃别名让这句话在四个 commit 前就已经不成立了）；S2c 之后多了一个补对齐的 style commit（`60c8d27f`）。实际序列（`git log --oneline 3b55d419..1935f2e3`，从旧到新）：
   ```
   501cf7ce refactor(tal_network): introduce the provider names, keep the card names as aliases   -- S1
   413fe17d refactor(tal_network): move the data plane onto the provider names                    -- S2a
   64cdd072 style(tal_network): restore the alignment S2a shifted by one column                    -- 对齐修复
   8b151bbf refactor(netmgr): move the control plane onto the provider names                       -- S2b
   d807df8a refactor(tal_network): finish the four mentions the S2 pattern could not match          -- PAT 缺陷修复
   83b5f005 fix(netmgr): correct a claim S1's alias made false                                     -- 断言修复
   73c133e2 refactor(netmgr): rename netmgr_conn_base_t.card_type to .provider                     -- S2c
   60c8d27f style(netmgr): re-align the wired initializer S2c narrowed                              -- 对齐修复
   59403a1b docs(netmgr): move the two netmgr guides onto the provider names                        -- S2d
   91dd44f3 docs(switch_demo): move the two matrix configs' header comments onto the provider names -- S2e
   407bccd2 fix(netmgr): stop the AT_MODEM note from naming the alias that will outlive it          -- 另一处断言修复
   785f2f32 refactor(tal_network): mark the compatibility names deprecated and ship the rewrite script -- S3
   1935f2e3 fix(tools): make the rewrite script follow the tree instead of the plan                 -- 修 S3 自己的脚本
   ```
   13 个 commit，不是计划设想的 6 个（S1、S2a-e、S3）。`407bccd2` 修的是 `83b5f005` 自己落地时新引入的问题——同一类「修正的措辞里又带出一个旧名字」，和 §4.4 第 4 条那种「点名 wrapper 的注释」是同一类坑。下一个执行这份计划的人应该预期这种形状：机械重命名的每一步都在验证前一步的完整性检查有没有漏洞，漏洞暴露出来就地修一个 commit，而不是留到下一轮或者悄悄改前一个 commit。
4. **S3** 给 typedef 和函数别名加 `__attribute__((deprecated))`；宏别名不加（做不到），只加文档和可 grep 的标记；在 `tools/` 下放替换脚本，脚本里带上 §2.5 的碰撞名单。gate：`git grep` 旧名字，除 `tal_network_register.h` 外零命中 —— **这是 S2 改全了的唯一机械证明。**
5. **发一次带 release note 的 tag**，附 §3.3 的完整映射表。这一步不是可选的，它是 S4 唯一合法的触发条件。
6. **S4** 独立 PR：删掉全部别名，删掉三个零调用方 wrapper。gate：全树旧名字零命中 + 两个 target 编过。
7. **S4 之后**才轮到：删 `TAL_NET_TYPE_AT_MODEM` 并调 `MAX`、删三个只写不读的字段、`tal_net_provider_registry` 加 `static` 并同时改名成 `s_provider_registry`（§3.3、§6.2）、文件重命名。每项一个 commit。
8. **全程不做**：§6 那两张表里的任何一项。

三条贯穿全程的约束，如果只记三句话就记这三句：

- **数值不许动。** 别名消失是响亮的编译错误，数值换意思是静默的错误行为。
- **旧名字不许被复用去指别的东西。** 这是本计划唯一可能造成不可发现破坏的操作。
- **`git grep` 是完整性证明，构建不是。** 构建证明「新名字定义得对」；只有那条 grep 证明「树内改全了」；而**两者都不能证明树外**，所以废弃窗口靠时间和公告，不靠 CI 变绿。
