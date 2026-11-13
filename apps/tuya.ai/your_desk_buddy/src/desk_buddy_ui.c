/**
 * @file desk_buddy_ui.c
 * @brief Touch-driven hardware self-test UI for a round 360x360 display.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "desk_buddy_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "desk_buddy_test.h"
#include "lvgl.h"

LV_FONT_DECLARE(desk_buddy_font_18);

#define UI_SCREEN_SIZE          360
#define UI_FONT                 (&desk_buddy_font_18)

#define UI_BG_COLOR             0x090D13
#define UI_PANEL_COLOR          0x141C27
#define UI_PANEL_PRESSED_COLOR  0x1C2939
#define UI_TEXT_COLOR           0xF4F7FB
#define UI_MUTED_COLOR          0x8B9BB1
#define UI_READY_COLOR          0x7E8DA2
#define UI_RUNNING_COLOR        0x4DA3FF
#define UI_STOPPING_COLOR       0xF2C94C
#define UI_PASS_COLOR           0x37D67A
#define UI_FAIL_COLOR           0xFF6B6B
#define UI_STOPPED_COLOR        0xB5A8FF
#define UI_SKIP_COLOR           0xF2C94C

#define UI_CARD_X               76
#define UI_CARD_Y               96
#define UI_CARD_WIDTH           208
#define UI_CARD_HEIGHT          40
#define UI_CARD_GAP             6
#define UI_CARD_COMPACT_Y       90
#define UI_CARD_COMPACT_HEIGHT  34
#define UI_CARD_COMPACT_GAP     5

typedef enum {
    UI_CARD_READY = 0,
    UI_CARD_RUNNING,
    UI_CARD_STOPPING,
    UI_CARD_PASS,
    UI_CARD_FAIL,
    UI_CARD_SKIP,
    UI_CARD_STOPPED,
} UI_CARD_STATE_E;

typedef struct {
    DESK_BUDDY_TEST_ID_E id;
    const char *title;
} TEST_CARD_DESC_T;

static lv_obj_t *s_screen;
static lv_obj_t *s_home_layer;
static lv_obj_t *s_detail_layer;
static lv_obj_t *s_home_hint_label;
static lv_obj_t *s_cards[DESK_BUDDY_TEST_MAX];
static lv_obj_t *s_accents[DESK_BUDDY_TEST_MAX];
static lv_obj_t *s_status_labels[DESK_BUDDY_TEST_MAX];
static lv_obj_t *s_detail_title_label;
static lv_obj_t *s_detail_status_label;
static lv_obj_t *s_detail_result_label;
static lv_obj_t *s_start_button;
static lv_obj_t *s_stop_button;
static lv_timer_t *s_ui_timer;
static UI_CARD_STATE_E s_card_states[DESK_BUDDY_TEST_MAX];
static char s_card_details[DESK_BUDDY_TEST_MAX][96];
static DESK_BUDDY_TEST_ID_E s_detail_id = DESK_BUDDY_TEST_MAX;
static DESK_BUDDY_TEST_ID_E s_running_id = DESK_BUDDY_TEST_MAX;
static DESK_BUDDY_TEST_ID_E s_resource_open_id = DESK_BUDDY_TEST_MAX;
static bool s_stop_pending;
static uint32_t s_test_count;

static const TEST_CARD_DESC_T s_test_cards[] = {
#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
    {DESK_BUDDY_TEST_FRONT_CAMERA, "前置摄像头"},
#if defined(ENABLE_CAMERA_2) && (ENABLE_CAMERA_2 == 1)
    {DESK_BUDDY_TEST_REAR_CAMERA, "后置摄像头"},
#endif
#endif
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    {DESK_BUDDY_TEST_SD_CARD, "存储卡"},
#endif
#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
    {DESK_BUDDY_TEST_ENCODER, "编码器"},
#endif
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    {DESK_BUDDY_TEST_LED, "氛围灯"},
#endif
#if defined(ENABLE_MOTION) && (ENABLE_MOTION == 1)
    {DESK_BUDDY_TEST_MOTION, "加速度计"},
#endif
};

static const char *__state_text(UI_CARD_STATE_E state)
{
    switch (state) {
    case UI_CARD_RUNNING:
        return "运行中";
    case UI_CARD_STOPPING:
        return "关闭中";
    case UI_CARD_PASS:
        return "通过";
    case UI_CARD_FAIL:
        return "失败";
    case UI_CARD_SKIP:
        return "跳过";
    case UI_CARD_STOPPED:
        return "已停止";
    case UI_CARD_READY:
    default:
        return "待测试";
    }
}

static uint32_t __state_color(UI_CARD_STATE_E state)
{
    switch (state) {
    case UI_CARD_RUNNING:
        return UI_RUNNING_COLOR;
    case UI_CARD_STOPPING:
        return UI_STOPPING_COLOR;
    case UI_CARD_PASS:
        return UI_PASS_COLOR;
    case UI_CARD_FAIL:
        return UI_FAIL_COLOR;
    case UI_CARD_SKIP:
        return UI_SKIP_COLOR;
    case UI_CARD_STOPPED:
        return UI_STOPPED_COLOR;
    case UI_CARD_READY:
    default:
        return UI_READY_COLOR;
    }
}

static bool __is_valid_id(DESK_BUDDY_TEST_ID_E id)
{
    return id >= 0 && id < DESK_BUDDY_TEST_MAX;
}

static void __set_button_enabled(lv_obj_t *button, bool enabled)
{
    if (button == NULL) {
        return;
    }

    if (enabled) {
        lv_obj_remove_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
}

static void __update_detail_buttons(void)
{
    bool running = s_running_id == s_detail_id;
    bool resource_open = s_resource_open_id == s_detail_id;
    bool can_start = !running && !resource_open && s_running_id == DESK_BUDDY_TEST_MAX;
    bool can_stop = (running || resource_open) && !s_stop_pending;

    __set_button_enabled(s_start_button, can_start);
    __set_button_enabled(s_stop_button, can_stop);
}

static void __set_cards_enabled(bool enabled)
{
    for (uint32_t index = 0U; index < s_test_count; ++index) {
        lv_obj_t *card = s_cards[s_test_cards[index].id];
        if (card == NULL) {
            continue;
        }

        if (enabled) {
            lv_obj_remove_state(card, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(card, LV_STATE_DISABLED);
        }
    }
}

static void __refresh_home_hint(void)
{
    if (s_home_hint_label == NULL) {
        return;
    }

    if (s_test_count == 0U) {
        lv_label_set_text(s_home_hint_label, "当前没有启用的外设");
    } else if (s_running_id != DESK_BUDDY_TEST_MAX) {
        lv_label_set_text(s_home_hint_label, "正在关闭外设，请稍候");
        lv_obj_set_style_text_color(s_home_hint_label,
                                    lv_color_hex(UI_STOPPING_COLOR), 0);
    } else {
        lv_label_set_text(s_home_hint_label, "选择设备进入测试");
        lv_obj_set_style_text_color(s_home_hint_label,
                                    lv_color_hex(UI_MUTED_COLOR), 0);
    }
}

static void __set_card_state(DESK_BUDDY_TEST_ID_E id, UI_CARD_STATE_E state,
                             const char *detail)
{
    char detail_copy[sizeof(s_card_details[0])];
    uint32_t color;

    if (!__is_valid_id(id) || s_cards[id] == NULL ||
        s_status_labels[id] == NULL) {
        return;
    }

    snprintf(detail_copy, sizeof(detail_copy), "%s", detail != NULL ? detail : "");
    snprintf(s_card_details[id], sizeof(s_card_details[id]), "%s", detail_copy);
    s_card_states[id] = state;
    color = __state_color(state);

    lv_label_set_text(s_status_labels[id], __state_text(state));
    lv_obj_set_style_text_color(s_status_labels[id], lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(s_status_labels[id], lv_color_hex(color), 0);
    lv_obj_set_style_border_color(s_cards[id], lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(s_accents[id], lv_color_hex(color), 0);

    if (s_detail_id == id && s_detail_status_label != NULL &&
        s_detail_result_label != NULL) {
        lv_label_set_text(s_detail_status_label, __state_text(state));
        lv_obj_set_style_text_color(s_detail_status_label,
                                    lv_color_hex(color), 0);
        lv_label_set_text(s_detail_result_label,
                          detail_copy[0] != '\0' ? detail_copy : "-");
    }
}

static void __show_home(void)
{
    if (s_detail_layer != NULL) {
        lv_obj_add_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_home_layer != NULL) {
        lv_obj_clear_flag(s_home_layer, LV_OBJ_FLAG_HIDDEN);
    }

    __set_cards_enabled(s_running_id == DESK_BUDDY_TEST_MAX);
    __refresh_home_hint();
}

static void __show_detail(DESK_BUDDY_TEST_ID_E id)
{
    const char *title = NULL;

    if (!__is_valid_id(id)) {
        return;
    }

    for (uint32_t index = 0U; index < s_test_count; ++index) {
        if (s_test_cards[index].id == id) {
            title = s_test_cards[index].title;
            break;
        }
    }

    if (title == NULL) {
        return;
    }

    s_detail_id = id;
    lv_label_set_text(s_detail_title_label, title);
    lv_label_set_text(s_detail_status_label, __state_text(s_card_states[id]));
    lv_obj_set_style_text_color(s_detail_status_label,
                                lv_color_hex(__state_color(s_card_states[id])), 0);
    lv_label_set_text(s_detail_result_label,
                      s_card_details[id][0] != '\0' ? s_card_details[id] :
                                                     "点击“开始”运行测试");

    if (s_home_layer != NULL) {
        lv_obj_add_flag(s_home_layer, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_detail_layer != NULL) {
        lv_obj_clear_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
    }

    __update_detail_buttons();
}

static void __card_clicked_cb(lv_event_t *event)
{
    DESK_BUDDY_TEST_ID_E id =
        (DESK_BUDDY_TEST_ID_E)(uintptr_t)lv_event_get_user_data(event);

    if (s_running_id != DESK_BUDDY_TEST_MAX) {
        return;
    }

    __show_detail(id);
}

static void __start_clicked_cb(lv_event_t *event)
{
    OPERATE_RET rt;

    (void)event;

    if (s_detail_id == DESK_BUDDY_TEST_MAX ||
        s_running_id != DESK_BUDDY_TEST_MAX ||
        s_resource_open_id == s_detail_id) {
        return;
    }

    __set_card_state(s_detail_id, UI_CARD_RUNNING, "正在打开外设...");
    rt = desk_buddy_test_request(s_detail_id);
    if (rt != OPRT_OK) {
        __set_card_state(s_detail_id, UI_CARD_FAIL, "测试请求失败");
        __update_detail_buttons();
        return;
    }

    s_running_id = s_detail_id;
    s_stop_pending = false;
    __set_card_state(s_detail_id, UI_CARD_RUNNING, "测试运行中...");
    __update_detail_buttons();
}

static void __stop_clicked_cb(lv_event_t *event)
{
    OPERATE_RET rt;
    bool running;
    bool resource_open;

    (void)event;

    if (s_detail_id == DESK_BUDDY_TEST_MAX || s_stop_pending) {
        return;
    }

    running = s_running_id == s_detail_id;
    resource_open = s_resource_open_id == s_detail_id;
    if (!running && !resource_open) {
        return;
    }

    if (running) {
        s_stop_pending = true;
        rt = desk_buddy_test_stop(s_detail_id);
        if (rt != OPRT_OK) {
            s_stop_pending = false;
            __set_card_state(s_detail_id, UI_CARD_RUNNING,
                             "关闭请求失败，请重试");
        } else {
            __set_card_state(s_detail_id, UI_CARD_STOPPING,
                             "正在停止并关闭外设...");
        }
        __update_detail_buttons();
        return;
    }

    rt = desk_buddy_test_stop(s_detail_id);
    if (rt == OPRT_OK) {
        s_resource_open_id = DESK_BUDDY_TEST_MAX;
        __set_card_state(s_detail_id, UI_CARD_STOPPED, "外设已关闭，可以再次开始");
    } else {
        __set_card_state(s_detail_id, UI_CARD_FAIL, "外设关闭失败，请重试");
    }
    __update_detail_buttons();
}

static void __back_clicked_cb(lv_event_t *event)
{
    OPERATE_RET rt;

    (void)event;

    if (s_detail_id != DESK_BUDDY_TEST_MAX &&
        s_running_id == s_detail_id && !s_stop_pending) {
        s_stop_pending = true;
        rt = desk_buddy_test_stop(s_detail_id);
        if (rt == OPRT_OK) {
            __set_card_state(s_detail_id, UI_CARD_STOPPING,
                             "正在停止并关闭外设...");
        } else {
            s_stop_pending = false;
            __set_card_state(s_detail_id, UI_CARD_RUNNING,
                             "关闭请求失败，请稍后重试");
        }
    } else if (s_detail_id != DESK_BUDDY_TEST_MAX &&
               s_resource_open_id == s_detail_id) {
        rt = desk_buddy_test_stop(s_detail_id);
        if (rt == OPRT_OK) {
            s_resource_open_id = DESK_BUDDY_TEST_MAX;
            __set_card_state(s_detail_id, UI_CARD_STOPPED, "外设已关闭");
        } else {
            __set_card_state(s_detail_id, UI_CARD_FAIL, "外设关闭失败，请重试");
        }
    }

    __update_detail_buttons();
    __show_home();
}

static void __style_panel(lv_obj_t *panel)
{
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(UI_READY_COLOR), 0);
    lv_obj_set_style_border_opa(panel, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_shadow_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

static void __create_card(DESK_BUDDY_TEST_ID_E id, const char *title,
                          uint32_t index)
{
    bool compact = s_test_count > 5U;
    int32_t start_y = compact ? UI_CARD_COMPACT_Y : UI_CARD_Y;
    int32_t height = compact ? UI_CARD_COMPACT_HEIGHT : UI_CARD_HEIGHT;
    int32_t gap = compact ? UI_CARD_COMPACT_GAP : UI_CARD_GAP;
    int32_t y = start_y + (int32_t)index * (height + gap);
    lv_obj_t *card = lv_obj_create(s_home_layer);
    lv_obj_t *accent;
    lv_obj_t *title_label;
    lv_obj_t *status;

    __style_panel(card);
    lv_obj_set_pos(card, UI_CARD_X, y);
    lv_obj_set_size(card, UI_CARD_WIDTH, height);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_PANEL_PRESSED_COLOR),
                              LV_STATE_PRESSED);
    lv_obj_set_style_opa(card, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, __card_clicked_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)id);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(accent, 4, 20);
    lv_obj_set_style_radius(accent, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(accent, lv_color_hex(UI_READY_COLOR), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE);

    title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 13, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(title_label, UI_FONT, 0);

    status = lv_label_create(card);
    lv_label_set_text(status, "待测试");
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(UI_READY_COLOR), 0);
    lv_obj_set_style_text_font(status, UI_FONT, 0);
    lv_obj_set_style_bg_color(status, lv_color_hex(UI_READY_COLOR), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_20, 0);
    lv_obj_set_style_radius(status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(status, 7, 0);
    lv_obj_set_style_pad_ver(status, 2, 0);

    s_cards[id] = card;
    s_accents[id] = accent;
    s_status_labels[id] = status;
    s_card_states[id] = UI_CARD_READY;
    snprintf(s_card_details[id], sizeof(s_card_details[id]),
             "点击“开始”运行测试");
}

static void __create_home_layer(void)
{
    lv_obj_t *title;
    lv_obj_t *subtitle;

    s_home_layer = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_home_layer);
    lv_obj_set_pos(s_home_layer, 0, 0);
    lv_obj_set_size(s_home_layer, UI_SCREEN_SIZE, UI_SCREEN_SIZE);
    lv_obj_clear_flag(s_home_layer, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(s_home_layer);
    lv_label_set_text(title, "DESK BUDDY");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_letter_space(title, 1, 0);

    subtitle = lv_label_create(s_home_layer);
    lv_label_set_text(subtitle, "桌面伙伴 · 外设自检");
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(UI_RUNNING_COLOR), 0);
    lv_obj_set_style_text_font(subtitle, UI_FONT, 0);

    s_home_hint_label = lv_label_create(s_home_layer);
    lv_label_set_text(s_home_hint_label, "选择设备进入测试");
    lv_obj_align(s_home_hint_label, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_text_color(s_home_hint_label,
                                lv_color_hex(UI_MUTED_COLOR), 0);
    lv_obj_set_style_text_font(s_home_hint_label, UI_FONT, 0);

    for (uint32_t index = 0U; index < s_test_count; ++index) {
        __create_card(s_test_cards[index].id, s_test_cards[index].title, index);
    }
}

static void __style_button(lv_obj_t *button, uint32_t color)
{
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 7, 0);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
    lv_obj_set_style_shadow_offset_y(button, 3, 0);
    lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *__create_button(lv_obj_t *parent, const char *text,
                                 int32_t x, int32_t y, int32_t width,
                                 int32_t height, uint32_t color)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label;

    __style_button(button, color);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);

    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);

    return button;
}

static void __create_detail_layer(void)
{
    lv_obj_t *status_panel;
    lv_obj_t *status_caption;
    lv_obj_t *back_button;

    s_detail_layer = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_detail_layer);
    lv_obj_set_pos(s_detail_layer, 0, 0);
    lv_obj_set_size(s_detail_layer, UI_SCREEN_SIZE, UI_SCREEN_SIZE);
    lv_obj_clear_flag(s_detail_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_detail_title_label = lv_label_create(s_detail_layer);
    lv_label_set_text(s_detail_title_label, "设备");
    lv_obj_align(s_detail_title_label, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_text_color(s_detail_title_label,
                                lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(s_detail_title_label, UI_FONT, 0);

    status_panel = lv_obj_create(s_detail_layer);
    __style_panel(status_panel);
    lv_obj_set_pos(status_panel, 55, 78);
    lv_obj_set_size(status_panel, 250, 110);

    status_caption = lv_label_create(status_panel);
    lv_label_set_text(status_caption, "状态");
    lv_obj_set_pos(status_caption, 18, 11);
    lv_obj_set_style_text_color(status_caption, lv_color_hex(UI_MUTED_COLOR), 0);
    lv_obj_set_style_text_font(status_caption, UI_FONT, 0);

    s_detail_status_label = lv_label_create(status_panel);
    lv_label_set_text(s_detail_status_label, "待测试");
    lv_obj_set_pos(s_detail_status_label, 18, 33);
    lv_obj_set_style_text_color(s_detail_status_label,
                                lv_color_hex(UI_READY_COLOR), 0);
    lv_obj_set_style_text_font(s_detail_status_label, UI_FONT, 0);
    lv_obj_set_style_text_letter_space(s_detail_status_label, 1, 0);

    s_detail_result_label = lv_label_create(status_panel);
    lv_label_set_text(s_detail_result_label, "点击“开始”运行测试");
    lv_obj_set_pos(s_detail_result_label, 18, 62);
    lv_obj_set_size(s_detail_result_label, 214, 38);
    lv_label_set_long_mode(s_detail_result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_detail_result_label,
                                lv_color_hex(UI_MUTED_COLOR), 0);
    lv_obj_set_style_text_font(s_detail_result_label, UI_FONT, 0);

    s_start_button = __create_button(s_detail_layer, "开始", 55, 204, 118, 46,
                                     UI_RUNNING_COLOR);
    lv_obj_add_event_cb(s_start_button, __start_clicked_cb, LV_EVENT_CLICKED,
                        NULL);

    s_stop_button = __create_button(s_detail_layer, "停止并关闭", 187, 204, 118,
                                    46, UI_FAIL_COLOR);
    lv_obj_add_event_cb(s_stop_button, __stop_clicked_cb, LV_EVENT_CLICKED,
                        NULL);

    back_button = __create_button(s_detail_layer, "返回列表", 105, 268, 150, 40,
                                  UI_PANEL_PRESSED_COLOR);
    lv_obj_add_event_cb(back_button, __back_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
}

static void __ui_timer_cb(lv_timer_t *timer)
{
    DESK_BUDDY_TEST_RESULT_T result;

    (void)timer;

    while (desk_buddy_test_poll_result(&result)) {
        bool ui_canceled = s_stop_pending && s_running_id == result.id;
        UI_CARD_STATE_E state;

        if (s_running_id == result.id) {
            s_running_id = DESK_BUDDY_TEST_MAX;
        }

        if (ui_canceled && result.status == DESK_BUDDY_TEST_STATUS_PASS) {
            state = UI_CARD_STOPPED;
        } else {
            switch (result.status) {
            case DESK_BUDDY_TEST_STATUS_PASS:
                state = UI_CARD_PASS;
                break;
            case DESK_BUDDY_TEST_STATUS_SKIPPED:
                state = UI_CARD_SKIP;
                break;
            case DESK_BUDDY_TEST_STATUS_STOPPED:
                state = UI_CARD_STOPPED;
                break;
            case DESK_BUDDY_TEST_STATUS_FAIL:
            default:
                state = UI_CARD_FAIL;
                break;
            }
        }

        if (state == UI_CARD_PASS && result.id == DESK_BUDDY_TEST_ENCODER) {
            s_resource_open_id = result.id;
        } else if (s_resource_open_id == result.id) {
            s_resource_open_id = DESK_BUDDY_TEST_MAX;
        }

        if (s_running_id == DESK_BUDDY_TEST_MAX) {
            s_stop_pending = false;
        }

        __set_card_state(result.id, state, result.detail);
        __set_cards_enabled(true);
        __refresh_home_hint();
        __update_detail_buttons();
    }

#if defined(ENABLE_ENCODER) && (ENABLE_ENCODER == 1)
    if (s_detail_layer != NULL &&
        !lv_obj_has_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN) &&
        s_detail_id == DESK_BUDDY_TEST_ENCODER &&
        s_resource_open_id == DESK_BUDDY_TEST_ENCODER &&
        s_running_id == DESK_BUDDY_TEST_MAX) {
        int32_t position = 0;
        uint8_t pressed = 0U;

        if (desk_buddy_test_get_encoder_state(&position, &pressed) == OPRT_OK) {
            char detail[64];
            snprintf(detail, sizeof(detail), "位置 %d, 按键%s", position,
                     pressed != 0U ? "按下" : "松开");
            snprintf(s_card_details[DESK_BUDDY_TEST_ENCODER],
                     sizeof(s_card_details[DESK_BUDDY_TEST_ENCODER]), "%s", detail);
            lv_label_set_text(s_detail_result_label, detail);
        }
    }
#endif
}

void desk_buddy_ui_create(void)
{
    memset(s_cards, 0, sizeof(s_cards));
    memset(s_accents, 0, sizeof(s_accents));
    memset(s_status_labels, 0, sizeof(s_status_labels));
    memset(s_card_states, 0, sizeof(s_card_states));
    memset(s_card_details, 0, sizeof(s_card_details));

    s_screen = lv_screen_active();
    s_home_layer = NULL;
    s_detail_layer = NULL;
    s_home_hint_label = NULL;
    s_cards[0] = NULL;
    s_detail_title_label = NULL;
    s_detail_status_label = NULL;
    s_detail_result_label = NULL;
    s_start_button = NULL;
    s_stop_button = NULL;
    s_detail_id = DESK_BUDDY_TEST_MAX;
    s_running_id = DESK_BUDDY_TEST_MAX;
    s_resource_open_id = DESK_BUDDY_TEST_MAX;
    s_stop_pending = false;
    s_test_count = (uint32_t)(sizeof(s_test_cards) / sizeof(s_test_cards[0]));

    if (s_ui_timer != NULL) {
        lv_timer_delete(s_ui_timer);
        s_ui_timer = NULL;
    }

    lv_obj_clean(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    __create_home_layer();
    __create_detail_layer();
    __show_home();

    s_ui_timer = lv_timer_create(__ui_timer_cb, 100U, NULL);
}
