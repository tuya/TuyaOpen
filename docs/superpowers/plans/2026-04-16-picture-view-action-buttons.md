# Picture View Action Buttons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a print button (top) and an attach-to-chat button (bottom) as a right-side vertical floating pill on the full-screen picture view in `ai_ui_wechat_chat`.

**Architecture:** Widen `disp_image` callback signature from `(uint8_t*, uint32_t)` to `(AI_UI_IMG_T*)` so the album filename travels with the image. Store the filename in `AI_UI_WECHAT_CHAT_T.cur_img_name`. Both buttons fire existing actions (`AI_UI_ACT_PRINT_IMG` / `AI_UI_ACT_ADD_IMG_ATTACH`) with the filename — no new action types needed.

**Tech Stack:** C, LVGL, TuyaOpen embedded framework. No unit test framework — verification is build + visual inspection.

---

## File Map

| File | Change |
|---|---|
| `apps/tuya.ai/ai_components/ai_ui/font/font_awesome_symbols.h` | Add `FONT_AWESOME_PRINT` |
| `apps/tuya.ai/ai_components/ai_ui/include/ai_ui_manage.h` | Change `disp_image` field in `AI_UI_CHAT_INTFS_T` |
| `apps/tuya.ai/ai_components/ai_ui/src/ai_ui_manage.c` | Update `__image_link_view_cb` to pass `&img` |
| `apps/tuya.ai/ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c` | New struct field, updated `__ui_disp_image`, new button widgets + callbacks, clear name on return |

---

### Task 1: Add `FONT_AWESOME_PRINT` symbol

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/font/font_awesome_symbols.h`

- [ ] **Step 1: Add the define after `FONT_AWESOME_DOWNLOAD`**

Open `font_awesome_symbols.h`. After the line:
```c
#define FONT_AWESOME_DOWNLOAD          "\xef\x80\x99"
```
Add:
```c
#define FONT_AWESOME_PRINT             "\xef\x80\xae"
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/font/font_awesome_symbols.h
git commit -m "feat(ai_ui): add FONT_AWESOME_PRINT symbol"
```

---

### Task 2: Widen `disp_image` signature in the chat interface

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/include/ai_ui_manage.h:151`
- Modify: `apps/tuya.ai/ai_components/ai_ui/src/ai_ui_manage.c:128-129`

- [ ] **Step 1: Update the function pointer in `AI_UI_CHAT_INTFS_T`**

In `ai_ui_manage.h`, find `AI_UI_CHAT_INTFS_T` (around line 142). Change:
```c
    void (*disp_image)(uint8_t *jpeg, uint32_t len);
```
to:
```c
    void (*disp_image)(AI_UI_IMG_T *img);
```

- [ ] **Step 2: Update the call site in `__image_link_view_cb`**

In `ai_ui_manage.c`, find `__image_link_view_cb`. Change:
```c
    if (sg_chat_intfs.disp_image && img.data) {
        sg_chat_intfs.disp_image(img.data, img.len);
    }
    if (img.data) {
        image_album_free_file_data(img.data);
    }
```
to:
```c
    if (sg_chat_intfs.disp_image && img.data) {
        sg_chat_intfs.disp_image(&img);
    }
    if (img.data) {
        image_album_free_file_data(img.data);
    }
```

Note: `img.data` is freed by `__image_link_view_cb` immediately after `disp_image` returns. The callee must not hold `img.data` — it only needs to decode it and copy the filename.

- [ ] **Step 3: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/include/ai_ui_manage.h \
        apps/tuya.ai/ai_components/ai_ui/src/ai_ui_manage.c
git commit -m "feat(ai_ui): widen disp_image callback to AI_UI_IMG_T*"
```

---

### Task 3: Update `__ui_disp_image` to accept `AI_UI_IMG_T*` and store filename

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c`

- [ ] **Step 1: Add `cur_img_name` to `AI_UI_WECHAT_CHAT_T`**

Find the struct definition (around line 44). Add one field after `popup_menu`:
```c
    char      cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];
```
The struct block becomes:
```c
typedef struct {
    lv_style_t style_avatar;
    lv_style_t style_ai_bubble;
    lv_style_t style_user_bubble;
    lv_style_t style_link;

    lv_obj_t *content;
    lv_obj_t *picture;
    lv_obj_t *picture_canvas;

    lv_obj_t *stream_msg_cont;
    lv_obj_t *stream_bubble;
    lv_obj_t *stream_label;
    bool      is_streaming;

    lv_obj_t *attach_bar;
    uint8_t  *attach_bufs[MAX_ATTACH_NUM];
    uint8_t   attach_count;

    lv_obj_t *plus_btn;
    lv_obj_t *popup_menu;

    char      cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];
} AI_UI_WECHAT_CHAT_T;
```

You'll also need to add the include for `ALBUM_FILENAME_MAX_LEN`. At the top of the file, add after the existing includes:
```c
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "image_album.h"
#endif
```

If `ALBUM_FILENAME_MAX_LEN` is not already available without the album guard, define a local fallback just above the struct:
```c
#ifndef ALBUM_FILENAME_MAX_LEN
#define ALBUM_FILENAME_MAX_LEN  63
#endif
```

- [ ] **Step 2: Update `__ui_disp_image` signature and body**

Change the function signature from:
```c
static void __ui_disp_image(uint8_t *jpeg, uint32_t len)
```
to:
```c
static void __ui_disp_image(AI_UI_IMG_T *img)
```

At the top of the function body, replace the null/length check and variable extraction:
```c
    if (jpeg == NULL || len == 0) {
        return;
    }
```
with:
```c
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }
    uint8_t  *jpeg = img->data;
    uint32_t  len  = img->len;

    /* Store album filename so the action buttons can reference it */
    if (img->name != NULL) {
        strncpy(sg_chat.cur_img_name, img->name, ALBUM_FILENAME_MAX_LEN);
        sg_chat.cur_img_name[ALBUM_FILENAME_MAX_LEN] = '\0';
    } else {
        sg_chat.cur_img_name[0] = '\0';
    }
```

The rest of the function body is unchanged — `jpeg` and `len` locals are used identically.

- [ ] **Step 3: Update the registration line**

In `ai_ui_wechat_chat_register`, the assignment:
```c
    intfs.disp_image              = __ui_disp_image;
```
needs no text change — the function pointer type now matches after the signature update.

- [ ] **Step 4: Clear `cur_img_name` when returning to chat**

In `__return_chat_content_event_cb`:
```c
static void __return_chat_content_event_cb(lv_event_t *e)
{
    lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
    sg_chat.cur_img_name[0] = '\0';
}
```

- [ ] **Step 5: Verify the project builds**

```bash
cd /home/share/samba/tmp/TuyaOpen
cmake -S apps/tuya.ai/your_chat_bot -B apps/tuya.ai/your_chat_bot/.build 2>&1 | tail -20
cmake --build apps/tuya.ai/your_chat_bot/.build 2>&1 | tail -30
```

Expected: build succeeds (zero errors). Fix any type mismatch errors before continuing.

- [ ] **Step 6: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c
git commit -m "feat(ai_ui/chat): store album filename when displaying picture"
```

---

### Task 4: Add the floating action pill and button callbacks

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c`

- [ ] **Step 1: Add `picture_action_bar` to `AI_UI_WECHAT_CHAT_T`**

Add two more fields after `cur_img_name`:
```c
    lv_obj_t *picture_action_bar;
    lv_obj_t *picture_print_btn;
    lv_obj_t *picture_attach_btn;
```

- [ ] **Step 2: Add the two button callbacks before `__return_chat_content_event_cb`**

Insert the following two static functions just before `__return_chat_content_event_cb` (around line 206):

```c
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static void __picture_print_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_chat.cur_img_name[0] == '\0') {
        return;
    }
    ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
}
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static void __picture_attach_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_chat.cur_img_name[0] == '\0') {
        return;
    }
    ai_ui_notify_action(AI_UI_ACT_ADD_IMG_ATTACH,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
}
#endif
```

- [ ] **Step 3: Build the floating pill in `ai_ui_wechat_chat_init`**

After the block that creates `sg_chat.picture` (the lines ending with `lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);`), add:

```c
    /* Floating action pill — right-center of picture view */
    sg_chat.picture_action_bar = lv_obj_create(sg_chat.picture);
    lv_obj_set_size(sg_chat.picture_action_bar, 36, 80);
    lv_obj_align(sg_chat.picture_action_bar, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(sg_chat.picture_action_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.picture_action_bar, (lv_opa_t)(0.55f * 255), 0);
    lv_obj_set_style_radius(sg_chat.picture_action_bar, 18, 0);
    lv_obj_set_style_border_width(sg_chat.picture_action_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.picture_action_bar, 0, 0);
    lv_obj_clear_flag(sg_chat.picture_action_bar, LV_OBJ_FLAG_SCROLLABLE);

    const lv_font_t *icon_font = ai_ui_get_icon_font();

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    /* Print button — top half of pill */
    sg_chat.picture_print_btn = lv_obj_create(sg_chat.picture_action_bar);
    lv_obj_set_size(sg_chat.picture_print_btn, 36, 36);
    lv_obj_align(sg_chat.picture_print_btn, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(sg_chat.picture_print_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_chat.picture_print_btn, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.picture_print_btn, 0, 0);
    lv_obj_set_style_radius(sg_chat.picture_print_btn, 0, 0);
    lv_obj_clear_flag(sg_chat.picture_print_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.picture_print_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *print_icon = lv_label_create(sg_chat.picture_print_btn);
    lv_obj_set_style_text_font(print_icon, icon_font, 0);
    lv_obj_set_style_text_color(print_icon, lv_color_white(), 0);
    lv_label_set_text(print_icon, FONT_AWESOME_PRINT);
    lv_obj_center(print_icon);

    lv_obj_add_event_cb(sg_chat.picture_print_btn, __picture_print_btn_cb, LV_EVENT_CLICKED, NULL);
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    /* Attach button — bottom half of pill */
    sg_chat.picture_attach_btn = lv_obj_create(sg_chat.picture_action_bar);
    lv_obj_set_size(sg_chat.picture_attach_btn, 36, 36);
    lv_obj_align(sg_chat.picture_attach_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(sg_chat.picture_attach_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_chat.picture_attach_btn, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.picture_attach_btn, 0, 0);
    lv_obj_set_style_radius(sg_chat.picture_attach_btn, 0, 0);
    lv_obj_clear_flag(sg_chat.picture_attach_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.picture_attach_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *attach_icon = lv_label_create(sg_chat.picture_attach_btn);
    lv_obj_set_style_text_font(attach_icon, icon_font, 0);
    lv_obj_set_style_text_color(attach_icon, lv_color_white(), 0);
    lv_label_set_text(attach_icon, FONT_AWESOME_IMAGE);
    lv_obj_center(attach_icon);

    lv_obj_add_event_cb(sg_chat.picture_attach_btn, __picture_attach_btn_cb, LV_EVENT_CLICKED, NULL);
#endif
```

- [ ] **Step 4: Build and verify**

```bash
cmake --build apps/tuya.ai/your_chat_bot/.build 2>&1 | tail -30
```

Expected: zero errors. The pill will be visible on-screen whenever `sg_chat.picture` is shown (it's a child, so it shows/hides with its parent automatically — no extra hide/show calls needed).

- [ ] **Step 5: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c
git commit -m "feat(ai_ui/chat): add print+attach floating action buttons on picture view"
```

---

## Self-Review Checklist

- [x] `FONT_AWESOME_PRINT` defined before use in Task 4
- [x] `ALBUM_FILENAME_MAX_LEN` guarded with a fallback `#ifndef` — compile-safe even without album enabled
- [x] `img->data` lifetime: freed by caller after `disp_image` returns; callee only reads `jpeg`/`len` locals during decode, and copies `name` via `strncpy` — no dangling pointer
- [x] Both button callbacks guarded by the same `#if defined` as the features they trigger
- [x] `picture_action_bar` is a child of `sg_chat.picture` — automatically hidden/shown with the picture view, no separate visibility management needed
- [x] `__return_chat_content_event_cb` clears `cur_img_name` to prevent stale action on next open
- [x] `intfs.disp_image = __ui_disp_image` assignment unchanged in text — type-compatible after signature update
- [x] No new action types, no changes to `app_ui_action.c`
