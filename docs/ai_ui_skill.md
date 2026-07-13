---
name: ai_ui_skill
description: AI UI component architecture, patterns, conventions and implementation guide for the wechat-style embedded LVGL UI system
type: reference
originSessionId: fa30025d-8321-410e-b472-b1c0ce3fe12e
---
# AI UI Component Skill Reference

## 1. Architecture Overview

```
ai_ui/
  include/
    ai_ui_manage.h          # 核心接口定义（所有 INTFS 结构体、枚举、API 声明）
    ai_ui_page.h             # 页面管理器（栈式导航）
    ai_ui_camera.h           # Camera 管理层
    ai_ui_image_album.h      # Album 管理层
  src/
    ai_ui_manage.c           # 核心调度：消息队列 + 回调分发 + action 队列
    ai_ui_page.c             # 页面栈管理（open/close）
    ai_ui_camera.c           # Camera 管理逻辑
    ai_ui_image_album.c      # Album 管理逻辑
    ai_ui_stream_text.c      # 流式文本显示
    ai_ui_icon_font.c        # 字体管理
    wechat/                  # WeChat 风格 UI 实现
      ai_ui_chat_wechat.c     # 主入口：LVGL 初始化、screen、container、status bar
      ai_ui_wechat_chat.c     # Chat 子页面：消息、流式、链接、图片查看、附件栏
      ai_ui_wechat_camera.c   # Camera 子页面：预览画布、快门、缩略图、关闭
      ai_ui_wechat_album.c    # Album 子页面：单图查看、全部缩略图、选择模式
      ai_ui_wechat_common.h   # wechat 内部共享声明
    chatbot/                 # Chatbot 风格 UI（独立变体）
    oled/                    # OLED 风格 UI（独立变体）
```

## 2. 四层接口体系

管理层通过四个独立的 INTFS 结构体委托显示回调，各 UI 变体独立注册：

| 结构体 | 职责 | 注册函数 |
|--------|------|----------|
| `AI_UI_INTFS_T` | 全局：init、emotion、status、notification、wifi、chat_mode | `ai_ui_register()` |
| `AI_UI_CHAT_INTFS_T` | Chat：open/close、user_msg、ai_msg、stream、image、link、attach | `ai_ui_chat_register()` |
| `AI_UI_CAMERA_INTFS_T` | Camera：open、yuv_flush、thumbnail_jpeg、close | `ai_ui_camera_register()` |
| `AI_UI_ALBUM_INTFS_T` | Album：open、image、all_thumb_list、select_thumb_list、close | `ai_ui_image_album_register()` |

**Wechat 注册模式**（ai_ui_chat_wechat.c）：
```c
// 主入口注册全局接口
ai_ui_register(&intfs);
// 各子页面独立注册
ai_ui_wechat_chat_register();    // → ai_ui_chat_register()
ai_ui_wechat_camera_register();  // → ai_ui_camera_register()
ai_ui_wechat_album_register();   // → ai_ui_image_album_register()
```

## 3. 消息调度机制

`ai_ui_manage.c` 维护两个队列：

- **UI 消息队列** (`sg_ui_queue_hdl`)：外部调用 `ai_ui_disp_msg()` / `ai_ui_disp_msg_sync()` 投递，`__ai_chat_ui_task` 线程消费，通过 `__ui_disp_msg_handle()` 分发到对应的 INTFS 回调
- **Action 队列** (`sg_action_queue_hdl`)：UI 层调用 `ai_ui_notify_action()` 投递用户操作事件（拍照、翻页、删除等），`__ai_ui_action_task` 线程消费

同步版本 `ai_ui_disp_msg_sync()` 通过二值信号量阻塞调用者直到 UI 线程处理完毕。**不能在 UI 线程（ai_ui task）中调用，否则死锁。**

## 4. 页面管理器

`ai_ui_page.h/.c` 提供栈式页面导航：

```c
typedef enum {
    AI_UI_PAGE_CHAT,
    AI_UI_PAGE_CAMERA,
    AI_UI_PAGE_ALBUM_VIEW,
    AI_UI_PAGE_ALBUM_ALL,
    AI_UI_PAGE_ALBUM_SELECT,
} AI_UI_PAGE_E;

ai_ui_page_open(AI_UI_PAGE_CAMERA, NULL);  // push + 调 open 回调
ai_ui_page_close();                         // pop + 调 close 回调
```

页面的 open/close 回调在 `ai_ui_manage.c` 的 `__page_register_all()` 中注册。

## 5. LVGL 层级结构（Wechat）

```
lv_scr_act()
  └── screen (LV_HOR_RES x LV_VER_RES, bg=0xF0F0F0)
        ├── container (LV_HOR_RES x LV_VER_RES)
        │     ├── status_bar (LV_HOR_RES x 40, green)
        │     │     ├── mode_label (LEFT)
        │     │     ├── status_label (CENTER)
        │     │     ├── emotion_label (LEFT of status)
        │     │     ├── notification_label (CENTER, hidden)
        │     │     └── network_label (RIGHT)
        │     ├── chat.content (滚动消息区)
        │     ├── chat.picture (图片查看, hidden)
        │     └── chat.attach_bar (附件缩略图, hidden)
        ├── camera.page (全屏, hidden)    ← parent=screen，覆盖 status bar
        │     ├── preview_canvas (lv_canvas)
        │     └── ctrl_bar (底部控制栏, 可隐藏)
        └── album pages (全屏, hidden)    ← parent=screen，覆盖 status bar
              ├── view_page
              ├── all_page
              └── select_page
```

**关键决策**：
- chat 子页面 parent = `container`（与 status bar 共存）
- camera/album 子页面 parent = `screen`（全屏覆盖，包括状态栏）

## 6. 图像处理模式

### 6.1 JPEG 缩略图（推荐方式）

使用 `tal_image_jpeg_scale_rgb565()` 一步完成解码+缩放：

```c
TAL_IMAGE_JPEG_SCALE_IN_T scale_in = {0};
scale_in.method     = TAL_IMAGE_SCALE_MTH_NEAREST;
scale_in.mode       = TAL_IMAGE_SCALE_MODE_SIZE;
scale_in.data       = jpeg_data;
scale_in.size       = jpeg_len;
scale_in.out_width  = THUMB_SIZE;
scale_in.out_height = THUMB_SIZE;

TAL_IMAGE_SCALE_OUT_T scale_out = {0};
tal_image_jpeg_scale_rgb565(&scale_in, &scale_out);

// scale_out.buf 由 API 内部分配，需用 tal_image_scale_buf_free() 释放
// 如需长期持有（canvas buffer），先 memcpy 到自己的 buffer
uint8_t *thumb_buf = Malloc(THUMB_SIZE * THUMB_SIZE * 2);
memcpy(thumb_buf, scale_out.buf, THUMB_SIZE * THUMB_SIZE * 2);
tal_image_scale_buf_free(&scale_out);
```

### 6.2 JPEG 原图显示

直接解码不缩放：

```c
TAL_IMAGE_JPEG_INFO_T info = {0};
tal_image_jpeg_get_info(data, len, &info);

uint32_t buf_size = (uint32_t)info.width * info.height * 2;
uint8_t *rgb565_buf = Malloc(buf_size);

TAL_IMAGE_JPEG_OUTPUT_T out = {0};
out.out_buf = rgb565_buf; out.out_buf_size = buf_size;
out.out_width = info.width; out.out_height = info.height;
tal_image_jpeg_decode_rgb565(data, len, &out);

lv_canvas_set_buffer(canvas, rgb565_buf, info.width, info.height, LV_IMG_CF_TRUE_COLOR);
```

### 6.3 Camera YUV 预览（双缓冲）

```c
// 1. 在 disp lock 外分配新 buffer 并转换
uint8_t *rgb565_buf = CAMERA_UI_MALLOC(w * h * 2);
TAL_IMAGE_YUV422_TO_RGB_T conv = { .in_buf=yuv, .in_width=w, .in_height=h,
                                    .out_buf=rgb565_buf, .out_width=w, .out_height=h };
tal_image_convert_yuv422_to_rgb565(&conv);

// 2. Lock → 设置 canvas → 释放旧 buffer → Unlock
lv_vendor_disp_lock();
lv_canvas_set_buffer(canvas, rgb565_buf, w, h, LV_IMG_CF_TRUE_COLOR);
if (old_buf) CAMERA_UI_FREE(old_buf);
preview_buf = rgb565_buf;
lv_vendor_disp_unlock();
```

## 7. 内存管理约定

| 场景 | 分配 | 释放 |
|------|------|------|
| 通用 UI (chat) | `Malloc()` / `Free()` | `Free()` |
| Camera (可能用 PSRAM) | `CAMERA_UI_MALLOC` / `CAMERA_UI_FREE` | `CAMERA_UI_FREE` |
| Album | `tal_malloc()` / `tal_free()` | `tal_free()` |
| Scale API 输出 | API 内部分配 | `tal_image_scale_buf_free()` |

**Canvas buffer 生命周期**：canvas 引用外部 buffer，必须在 buffer 释放前更新或删除 canvas。存储的 buffer 指针（如 `attach_bufs[]`、`preview_buf`、`view_buf`、`thumb_buf`）需在对象删除或页面关闭时释放。

## 8. 常用 LVGL 模式

### 8.1 线程安全

所有 LVGL API 调用必须包裹在 `lv_vendor_disp_lock()` / `lv_vendor_disp_unlock()` 之间。耗时操作（图像解码、转换）放在 lock 外。

### 8.2 Event 回调 user_data 内存泄漏防护

当 LVGL 对象绑定了动态分配的 user_data 时，必须同时注册 `LV_EVENT_DELETE` 回调来释放：

```c
UI_LINK_CB_DATA_T *link_data = Malloc(sizeof(UI_LINK_CB_DATA_T));
lv_obj_add_event_cb(label, __link_click_event_cb, LV_EVENT_CLICKED, link_data);
lv_obj_add_event_cb(label, __link_delete_event_cb, LV_EVENT_DELETE, link_data);
// __link_delete_event_cb 中 Free(data)
```

### 8.3 手势识别

**关键机制（LVGL v9）**：`indev_gesture()` 从 `act_obj`（手指下方对象）开始，沿父级链向上走，直到找到**没有** `LV_OBJ_FLAG_GESTURE_BUBBLE` 的对象，在那个对象上触发 `LV_EVENT_GESTURE`。

**所有通过 `lv_obj_create(parent)` 创建的子对象，`LV_OBJ_FLAG_GESTURE_BUBBLE` 默认是开启的。** 因此，如果不做处理，手势会一路冒泡到根屏幕（`lv_obj_create(NULL)`），注册在中间层级的回调永远收不到事件。

**正确用法**：在接收手势的对象上手动清除 `GESTURE_BUBBLE`，让手势在此停止：

```c
/* 清除 GESTURE_BUBBLE，让手势在 view_page 停止，不再往上冒泡 */
lv_obj_clear_flag(view_page, LV_OBJ_FLAG_GESTURE_BUBBLE);
lv_obj_add_flag(view_page, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(view_page, __gesture_cb, LV_EVENT_GESTURE, NULL);

/* 子对象保持默认的 GESTURE_BUBBLE，手势会从子对象冒泡到 view_page */
```

```c
static void __gesture_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_active();   /* LVGL v9 API */
    if (indev == NULL) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT)  { /* 左滑 → 下一张 */ }
    if (dir == LV_DIR_RIGHT) { /* 右滑 → 上一张 */ }
}
```

**lv_canvas 的特殊性**：`lv_canvas_create()` 内部会清除 `LV_OBJ_FLAG_CLICKABLE`，所以触摸会穿透 canvas 到其父对象（`view_page`）。父对象成为 `act_obj`，手势直接在父对象上触发，无需额外处理 canvas。

**常见踩坑**：
- `LV_OBJ_FLAG_SCROLLABLE` 影响的是滚动冲突（v8 的问题），不是手势冒泡（v9 的问题），清 SCROLLABLE 解决不了手势不触发
- `lv_indev_get_act()` 在 v9 中通过 `lv_api_map_v8.h` 映射到 `lv_indev_active()`，两者等价，但推荐直接用 `lv_indev_active()`

### 8.4 点击隐藏/显示 UI 叠加层

```c
static void __preview_click_cb(lv_event_t *e) {
    if (lv_obj_has_flag(ctrl_bar, LV_OBJ_FLAG_HIDDEN))
        lv_obj_clear_flag(ctrl_bar, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ctrl_bar, LV_OBJ_FLAG_HIDDEN);
}
```

## 9. 附件栏（Attach Bar）

- 最多 `MAX_ATTACH_NUM`(4) 张缩略图
- 每张图：container(48x48) + canvas + close button(16x16, 右上角)
- buffer 存储在 `attach_bufs[]` 数组，通过 `lv_obj_set_user_data(container, buf)` 关联
- 删除时 shift 数组、释放 buffer、删除 LVGL 对象；全部为空时隐藏 bar

## 10. CMakeLists.txt 条件编译

根据 Kconfig 选择只编译对应的 UI 变体：

```cmake
# 公共源文件
aux_source_directory(${COMP_MODULE_PATH}/src COMP_MODULE_SRCS)

# 按 Kconfig 选择 UI 变体
if (CONFIG_ENABLE_AI_CHAT_GUI_WECHAT STREQUAL "y")
    file(GLOB WECHAT_SRCS ${COMP_MODULE_PATH}/src/wechat/*.c)
    list(APPEND COMP_MODULE_SRCS ${WECHAT_SRCS})
elseif (CONFIG_ENABLE_AI_CHAT_GUI_CHATBOT STREQUAL "y")
    ...
elseif (CONFIG_ENABLE_AI_CHAT_GUI_OLED STREQUAL "y")
    ...
endif()
```

Include 路径也按条件添加（如 `src/wechat` 仅在 wechat 模式）。

## 11. 嵌入式平台注意事项

- **不用标准 `<string.h>`**：`tal_api.h` 提供 `memcpy`/`memset`/`strlen`/`strcmp`/`snprintf`
- **Clang LSP 误报**：`string.h file not found`、`memcpy undeclared` 等是嵌入式交叉编译环境下的 LSP 误诊，不是真正的编译错误
- **Tuya 宏**：`TUYA_CHECK_NULL_RETURN`、`TUYA_CALL_ERR_RETURN`、`PR_ERR`/`PR_DEBUG`/`PR_INFO`
- **内存**：`Malloc`/`Free`（大写开头）是 Tuya 封装，`tal_malloc`/`tal_free` 是 TAL 层封装
