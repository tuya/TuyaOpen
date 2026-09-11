# 显示 fb 池统一零拷贝(fb_manage 透明 vram)

## 背景:实测依据(非推测)

S31_Korvo_1(800×480 RGB565 SUB3 @18MHz,PSRAM@200MHz)lvgl_demo 15fps+撕裂,
根因 = 整帧双拷贝:port 层 `__disp_fill_display_framebuffer` 拷贝① + esp_lcd `draw_bitmap`
对陌生地址的内部拷贝②。已做三组实验:

1. **fill_color 5Hz**:no-bounce 直扫 PSRAM + `num_fbs=2` + 零拷切帧 → 稳定无花屏。
   硬件风险(S31 带宽扛不扛得住直扫)排除。
2. **零拷 DIRECT probe**(lv_port_disp_direct_probe.c):LVGL 直渲 panel 双 fb,24fps,
   有撕裂 → 拷贝不是问题了,差的只是 VSYNC 等待。
3. **原厂对齐目标**(同板 esp-bsp 官方 benchmark):all scenes avg 16fps / 静态 26-30,
   无撕裂(BUFFER_NUMS=2 + AVOID_TEAR + DIRECT + BOUNCE)。

tkl_rgb 路线已否决:ESP32 LCD_CAM 无 base-addr 寄存器(地址在 GDMA 描述符链里),
tkl_rgb 的"任意地址零拷"契约做不到;改共享 tkl_rgb.h/tdd_display_rgb.c 还会伤 T5。
**在 tdd 层做,不动 tkl。**

## 设计定稿

核心:**零拷贝对所有消费者透明**(LVGL/摄像头预览/u8g2/任意 app),一条主路,池自动分发。

| 层 | 职责 |
|---|---|
| tdd (esp_rgb) | 事实来源。`bounce_buffer_size==0` → 直扫零拷贝(`num_fbs=2`+实现 `get_frame_buffer`);`>0` → bounce 字节数(现状,DNESP32S3 不动)。删 40 行默认(树内无受益者:仅两板注册 RGB,DNESP32S3 显式传 480\*10\*2) |
| tdl | 新增 `tdl_disp_get_vram(disp_hdl, max, fbs, cnt)`,转调 tdd 回调;未 open/未实现 → `OPRT_NOT_SUPPORTED` |
| fb_manage | `init` 增加 `disp_hdl` 参数;首次 `add` 时 probe 一次。槽分发规则:**probe ≥2 块 + fmt/尺寸==整屏 + 还有剩 → vram 壳;否则该槽 malloc(混补,块数永远给足)**。来源记在 `fb->type` |
| flush | 完全无感知。`draw_bitmap` 按地址自动分流:vram 地址零拷切换,heap 地址内部拷贝 |
| 消费者 | `add` 签名零改动;严格场景(DIRECT 类)add 后断言 `fb->type == DISP_FB_TP_VRAM`,不满足报错不降级 |
| free | 对所有类型照常调。分流在实现内:`tdl_disp_free_frame_buff` 遇 VRAM 只 `tal_free` 壳,显存归 panel 生命周期 |

### 关键决策记录(讨论定稿,勿回退)

- **不加"优先 vram"参数/接口**:应用无从判断;能 vram 则 vram 已是池的默认行为,参数恒真无意义。
- **混补**:申请数 > vram 数时,超出的槽 malloc,不整体退回(问 3 给 0 本末倒置)、不截断
  (块数缩水把硬件后果推回应用)。
- **probe ≥2 块才启用**(单块 vram = 边扫边画必撕裂);尺寸不匹配整屏(partial 小 buf)天然走 malloc。
- **vram fb 的 free_cb 延迟到 `on_frame_buf_complete`**(驱动扫完那块)。一个挂点两用:
  池回收 + 防撕裂。
- **type 字段现成**:`TDL_DISP_FRAME_BUFF_T.type` 已有,枚举加一个值即可,不动结构体。

## 改动清单

### A. 类型 `tdl_display_type.h`
1. `DISP_FB_RAM_TP_E` 加 `DISP_FB_TP_VRAM`(注释:驱动内部 fb,零拷贝;free 只释放壳)。

### B. tdd 通用 `tdl_display_driver.h`
2. `TDD_DISP_INTFS_T` 加可选回调:
   `OPERATE_RET (*get_frame_buffer)(TDD_DISP_DEV_HANDLE_T device, uint8_t max, void **fbs, uint8_t *cnt);`
   仅 esp_rgb 实现,其余 NULL。

### C. tdd esp_rgb `tdd_disp_esp_rgb.c/.h`
3. bounce 语义翻转:`bounce_buffer_size==0` → 不 bounce(直扫),`num_fbs=2`;`>0` → bounce
   (现状路径,`num_fbs=1`,不交 fb)。删 `DEFAULT_BOUNCE_BUF_LINES` 40 行默认。
   头文件注释同步:`0 = direct scan (zero-copy capable) / >0 = bounce size in bytes`。
4. S31 板 `esp32s31_korvo_1.c`:`.bounce_buffer_size = 0` 注释改"direct scan zero-copy"。
5. `__esp_rgb_get_frame_buffer`:调 `esp_lcd_rgb_panel_get_frame_buffer(panel, 2, ...)`,
   填 fbs/cnt;panel 未开/单 fb → `OPRT_NOT_SUPPORTED`。intfs 注册加上。
6. flush 的 vram 分支:`draw_bitmap` 命中零拷切换后,**阻塞等 `on_frame_buf_complete`**
   (信号量,超时约 2 帧周期),扫完的前一块若是我们提交过且未归还的 pool fb → 触发其
   `free_cb`,然后返回。heap fb 路径现状不变(free_cb 即时)。
7. 注册 `on_frame_buf_complete` 回调;内部 fb 指针→提交记账(防每帧重复触发;首帧无前块不触发)。
   ⚠ 实现时先打点确认 S31 IDF6.2 该回调语义(仅切换时 vs 每帧)。

### D. tdl `tdl_display_manage.c/.h`
8. `tdl_disp_get_vram(disp_hdl, max, fbs, cnt)`:未 open / 回调 NULL → `OPRT_NOT_SUPPORTED`。
9. `tdl_disp_free_frame_buff` 加 VRAM 分支:`tal_free` 壳(fb_manage 用
   `tal_malloc(sizeof(TDL_DISP_FRAME_BUFF_T))` 建壳,frame 指向内部 fb)。

### E. fb_manage `tdl_display_fb_manage.c/.h`
10. `tdl_disp_fb_manage_init(&handle, disp_hdl)` 加第二参;存入管理结构。**7 处调用者全改**
    (v8/v9 full_frame port、u8g2、your_chat_bot、lvgl_camera、camera output_display)。
    NULL = 纯堆池(行为同今天)。
11. 结构体加:`disp_hdl`、probe 结果(内部 fb 指针数组 + 剩余数 + 已 probe 标志)。
12. `add`:首次触发 probe(此时设备必已 open);分发规则见上表。vram 壳:
    `tal_malloc` 壳 + 填 `frame`/`type=DISP_FB_TP_VRAM`/`free_cb`/`free_arg`,fmt/宽高照填。
    malloc 槽现状不动。`release`:对所有槽照常调 free(分流在 tdl_disp_free_frame_buff)。

### F. LVGL port 三模式(Kconfig choice 定稿)

Kconfig `choice LIBLVGL_FLUSH_MODE` 三选一:`PARTIAL` / `FULL`(默认)/ `DIRECT`;
`DUAL_DISP_BUFF` 改为 `depends on ENABLE_LVGL_FULL_FLUSH`。全树 config 遍历过:
11 个源配置全是 `PARTIAL_FLUSH=y`(choice 后行为不变),其余全落 FULL(同原语义),
仅 S31 `lvgl_demo` config 切成 DIRECT。

**三模式三文件**(v8/v9 同构,公共 `lv_port_disp.c` 零模式分支):
`lv_port_disp_partial.c` / `lv_port_disp_full_frame.c` / `lv_port_disp_direct.c`,
各自实现 `lv_port_flush_init` + `lv_port_disp_set_buffers`(buffer 策略归属模式文件)+ flush 四件套。

13. **direct 模式不走池**(定稿,用户指定):`lv_port_disp_direct.c`(v8/v9 各一)
    调确定的 `tdl_disp_get_vram(dev_hdl, 2, ...)` 直取驱动显存——拿到的必是 vram。
    <2 块 / rotation≠0 / is_swap → init 报错,**不降级**。壳结构 `direct_fb[2]`
    (`tkl_system_malloc`,`type=DISP_FB_VRAM`,free 只释放壳)。
14. v9:`lv_display_set_buffers(fb0->frame, fb1->frame, full_len, RENDER_MODE_DIRECT)`
    (px_map=整缓冲基址,已从 lv_refr.c 确认);v8:`lv_disp_draw_buf_init` 双整帧 +
    `direct_mode=1 + full_refresh=1`。均不再自分配 buf_2_1/2_2。
15. `flush_execute` DIRECT:`flush_is_last` → 按 px_map 地址匹配壳 →
    `tdl_disp_dev_flush`(tdd 内部阻塞等扫完,等价原厂 flush_wait_cb)→ `flush_ready`。
    **无任何 memcpy**。DMA2D 钩子做空函数(没拷贝了)。

### G. 还原 probe 脚手架
15. `tdd_disp_esp_rgb.c`:删 `ZEROCOPY_PROBE` 全部块/字段/导出函数/强制 num_fbs=2。
16. 删 `src/liblvgl/v9/port/lv_port_disp_direct_probe.c`。
17. `example_lvgl.c`:删 `ZEROCOPY_PROBE_PORT` 分支,还原 demo 调用(widgets/benchmark 按用户意愿)。
18. `fill_color` `example_display.c`:`tal_system_sleep(200)` 还原 1000。
19. `lvgl_demo` config:调试期改动按需还原(`ENABLE_LVGL_TP` 原本开、PARTIAL_FLUSH 原本关)。

## 验证

1. lvgl_demo benchmark:fps ≥ 24(超原厂 avg 16)且**无撕裂**;flush 单帧耗时 ≈1 帧周期
   (阻塞等待生效)。
2. fill_color:新池路径下 5Hz 切色依旧稳定。
3. 断言路径:无 vram 板/单 fb → `fb->type == PSRAM` → DIRECT 断言报错;heap 路径无行为变化。
4. 回归:DNESP32S3(显式 bounce)逻辑不变;u8g2/chatbot/camera 例程 init 签名改后编译过;
   混补边界(申请 3、vram 2 → 2 vram + 1 heap)走查。
5. camera output_display 例程天然受益(fmt/尺寸匹配才吃 vram)——S31 上相机→屏变零拷,
   注意双 DMA(相机写+LCD 读)同一 PSRAM 的 cache 一致性,首验先打点。

## 风险

- `on_frame_buf_complete` 语义待打点确认(每帧回调 vs 切换时);记账防重。
- DIRECT 分支不再需要 DMA2D memcpy 加速(没有拷贝了);heap 路径 DMA2D 照旧。
- `num_fbs` 1→2 多占 768KB PSRAM(S31 16MB 无压力)。
- 相机 DMA 写 vram fb 的 cache 一致性未实测,列入首验项。
