# Picture View Action Buttons — Design Spec

**Date:** 2026-04-16  
**Branch:** dev_picture  
**Scope:** `ai_ui_wechat_chat` — add print and attach-to-chat buttons on the full-screen picture view.

---

## 1. Problem

`__ui_disp_image` currently shows a full-screen JPEG view with no actions. The image name is available at call time (the image always comes from the album via `__image_link_view_cb`) but is discarded because the `disp_image` callback signature only carries `(uint8_t *jpeg, uint32_t len)`.

Users want two actions on this view:
- **Print** the current image on the thermal printer.
- **Add to attachment bar** so the image can be sent to the AI in a follow-up message.

Both existing action paths (`AI_UI_ACT_PRINT_IMG`, `AI_UI_ACT_ADD_IMG_ATTACH`) already accept an album filename — no new action types are needed.

---

## 2. Solution Overview

1. Widen the `disp_image` callback signature to `AI_UI_IMG_T *` so the album filename travels with the image.
2. Store the filename inside `AI_UI_WECHAT_CHAT_T` while the picture view is open.
3. Overlay a right-side vertical floating action bar (two icon buttons) on the picture canvas.

---

## 3. Interface Change

### `AI_UI_CHAT_INTFS_T` (`ai_ui_manage.h`)

```c
// Before
void (*disp_image)(uint8_t *jpeg, uint32_t len);

// After
void (*disp_image)(AI_UI_IMG_T *img);
```

### `__image_link_view_cb` (`ai_ui_manage.c`)

```c
// Before
sg_chat_intfs.disp_image(img.data, img.len);

// After
sg_chat_intfs.disp_image(&img);
// img.data is freed by the caller immediately after this call returns —
// the callee must not hold img.data beyond the function scope.
// img.name points to the link widget's own copy; callee must strncpy it.
```

---

## 4. State Changes in `AI_UI_WECHAT_CHAT_T`

Add one field:

```c
char cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];
```

`__ui_disp_image(AI_UI_IMG_T *img)`:
- If `img->name != NULL`: `strncpy(sg_chat.cur_img_name, img->name, ALBUM_FILENAME_MAX_LEN)`
- If `img->name == NULL`: `sg_chat.cur_img_name[0] = '\0'`

`__return_chat_content_event_cb`: clear `cur_img_name` on return to chat.

---

## 5. UI Layout

### Button container

Created once in `ai_ui_wechat_chat_init`, parented to `sg_chat.picture`.

```
sg_chat.picture  (full screen, hidden by default)
└── sg_chat.picture_canvas   (clickable, tap → return to chat)
└── sg_chat.picture_action_bar   (NEW — vertical pill, right-center)
    ├── print_btn       (top icon)
    ├── divider
    └── attach_btn      (bottom icon)
```

### Dimensions and position

| Property | Value |
|---|---|
| Container size | 36 × 80 px |
| Align | `LV_ALIGN_RIGHT_MID`, offset (-10, 0) |
| Background | `rgba(0,0,0,0.55)`, radius = 18 |
| Each icon button | 28 × 28 px, centered in its half |
| Icon font | `ai_ui_get_icon_font()` |
| Print icon | `FONT_AWESOME_PRINT` (`\xef\x80\xae`) — add to `font_awesome_symbols.h` |
| Attach icon | `FONT_AWESOME_IMAGE` (`\xef\x80\xbe`) |
| Icon color | white |

The container is not separately hidden/shown — it follows `sg_chat.picture` visibility automatically as a child.

### Disabled state

If `cur_img_name[0] == '\0'` (no name available), both buttons are styled grey and ignore click events (`lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE)`). This is a safety guard; in practice the only current call site always provides a name.

---

## 6. Button Callbacks

### Print button (`__picture_print_btn_cb`)

```c
static void __picture_print_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_chat.cur_img_name[0] == '\0') return;
    ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
}
```

Reuses the existing `AI_UI_ACT_PRINT_IMG` path in `app_ui_action.c` unchanged.

### Attach button (`__picture_attach_btn_cb`)

```c
static void __picture_attach_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_chat.cur_img_name[0] == '\0') return;
    ai_ui_notify_action(AI_UI_ACT_ADD_IMG_ATTACH,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
}
```

Reuses the existing `AI_UI_ACT_ADD_IMG_ATTACH` path in `app_ui_action.c` unchanged, which:
1. Calls `ai_ui_disp_msg_sync(AI_UI_DISP_ADD_CHAT_ATTACH_IMG, ...)` → shows thumbnail in attach bar
2. Calls `ai_picture_input_add_from_album(filename, NULL)` → registers image for AI input

---

## 7. Files Changed

| File | Change |
|---|---|
| `ai_ui_manage.h` | `disp_image` signature in `AI_UI_CHAT_INTFS_T` |
| `ai_ui_manage.c` | `__image_link_view_cb`: pass `&img` instead of `img.data, img.len` |
| `font_awesome_symbols.h` | Add `FONT_AWESOME_PRINT` (`"\xef\x80\xae"`) |
| `ai_ui_wechat_chat.c` | New field in struct, updated `__ui_disp_image`, new button create+callbacks, updated `__return_chat_content_event_cb` to clear name |

**No changes to:** `app_ui_action.c`, `ai_picture_input.c`, `ai_ui_wechat_album.c`.

---

## 8. Out of Scope

- Print result feedback overlay in the picture view (print result already goes to the album overlay; chat view ignores it for now).
- Images without an album backing (currently impossible via the existing call path).
