/**
 * @file ai_ui_chat_guided_wechat.c
 * @brief Guided device experience layered on the complete WeChat UI.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tuya_iot.h"
#include "netmgr.h"
#include "lvgl.h"
#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "guided_wechat_core.h"
#include "ai_ui_chat_guided_wechat.h"
#include "guided_wechat_locale.h"
#include "guided_wechat_state.h"

extern void ai_chat_ui_refresh_locale(void);

#define GUIDED_GREEN          0x07C160
#define GUIDED_BG             0xF4F5F7
#define GUIDED_TEXT           0x202124
#define GUIDED_MUTED          0x71757A
#define GUIDED_BORDER         0xE2E5E9
#define GUIDED_DANGER         0xD94248
#define GUIDED_HEADER_HEIGHT  44
#define GUIDED_STEPPER_HEIGHT 64
#define GUIDED_STEPPER_CIRCLE 28
#define GUIDED_STEPPER_LINE   4
#define GUIDED_STEP_COUNT     3
#define GUIDED_BUTTON_HEIGHT  44
#define GUIDED_RADIUS         14
#define GUIDED_NETWORK_GUIDE_URL_ZH \
    "https://tuyaopen.ai/zh/docs/quick-start/device-network-configuration#add-device"
#define GUIDED_NETWORK_GUIDE_URL_EN \
    "https://tuyaopen.ai/docs/quick-start/device-network-configuration"
#define GUIDED_AUTH_GUIDE_URL_ZH "https://tuyaopen.ai/zh/pricing"
#define GUIDED_AUTH_GUIDE_URL_EN "https://tuyaopen.ai/pricing"

typedef enum {
    GUIDED_PAGE_LANGUAGE_FIRST = 0,
    GUIDED_PAGE_AUTH_MISSING,
    GUIDED_PAGE_AUTH_READY,
    GUIDED_PAGE_NETWORK,
    GUIDED_PAGE_MENU,
    GUIDED_PAGE_LANGUAGE,
    GUIDED_PAGE_ABOUT,
    GUIDED_PAGE_RESET_CONFIRM,
} guided_page_t;

typedef enum {
    GUIDED_ACTION_NONE = 0,
    GUIDED_ACTION_LANG_ZH,
    GUIDED_ACTION_LANG_EN,
    GUIDED_ACTION_NEXT,
    GUIDED_ACTION_MENU,
    GUIDED_ACTION_NETWORK,
    GUIDED_ACTION_LANGUAGE,
    GUIDED_ACTION_ABOUT,
    GUIDED_ACTION_RESET,
    GUIDED_ACTION_RESET_CONFIRM,
    GUIDED_ACTION_STEP_LANGUAGE,
    GUIDED_ACTION_STEP_AUTH,
    GUIDED_ACTION_STEP_NETWORK,
    GUIDED_ACTION_BACK,
    GUIDED_ACTION_REFRESH,
    GUIDED_ACTION_PAIRING,
} guided_action_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *menu_button;
    lv_obj_t *status_label;
    guided_page_t page;
    guided_page_t back_page;
    guided_wechat_device_state_t state;
    lv_timer_t *refresh_timer;
    lv_obj_t *back_button;
    bool reset_pending;
    guided_page_t reset_parent_page;
    guided_page_t reset_parent_back_page;
} guided_ui_t;

static guided_ui_t sg_ui = {0};

static void __show_page(guided_page_t page, guided_page_t back_page);
static void __action_cb(lv_event_t *event);

static bool __setup_complete(void)
{
    guided_wechat_state_read(&sg_ui.state);
    return sg_ui.state.link_up && sg_ui.state.activated && sg_ui.state.cloud_online;
}

static uint8_t __guide_step(guided_page_t page)
{
    switch (page) {
    case GUIDED_PAGE_LANGUAGE_FIRST:
        return 1;
    case GUIDED_PAGE_AUTH_MISSING:
    case GUIDED_PAGE_AUTH_READY:
        return 2;
    case GUIDED_PAGE_NETWORK:
        return (sg_ui.back_page == GUIDED_PAGE_AUTH_READY ||
                sg_ui.back_page == GUIDED_PAGE_AUTH_MISSING)
                   ? 3
                   : 0;
    default:
        return 0;
    }
}

static bool __enter_chat(void)
{
    if (!__setup_complete()) {
        __show_page(sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_AUTH_MISSING
                        : GUIDED_PAGE_NETWORK,
                    sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_LANGUAGE_FIRST
                        : GUIDED_PAGE_AUTH_READY);
        return false;
    }

    lv_obj_add_flag(sg_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.menu_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_ui.menu_button);
    return true;
}

static const char *__tr(const char *zh, const char *en)
{
    return guided_wechat_tr(zh, en);
}

static const char *__network_guide_url(void)
{
    return guided_wechat_locale_get() == GUIDED_WECHAT_LANG_EN
               ? GUIDED_NETWORK_GUIDE_URL_EN
               : GUIDED_NETWORK_GUIDE_URL_ZH;
}

static const char *__auth_guide_url(void)
{
    return guided_wechat_locale_get() == GUIDED_WECHAT_LANG_EN
               ? GUIDED_AUTH_GUIDE_URL_EN
               : GUIDED_AUTH_GUIDE_URL_ZH;
}

static void __set_text_font(lv_obj_t *obj)
{
    lv_obj_set_style_text_font(obj, ai_ui_get_text_font(), 0);
}

static lv_obj_t *__make_label(lv_obj_t *parent, const char *text, lv_color_t color,
                              lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    __set_text_font(label);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text != NULL ? text : "");
    return label;
}

static lv_obj_t *__make_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(GUIDED_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, GUIDED_RADIUS, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void __make_guide_qr(lv_obj_t *card, const char *url)
{
    lv_obj_t *qr_wrap;
    lv_obj_t *qr;

    __make_label(card, __tr("扫码看教程", "Scan for guide"),
                 lv_color_hex(GUIDED_MUTED), LV_TEXT_ALIGN_CENTER);
    qr_wrap = lv_obj_create(card);
    lv_obj_set_width(qr_wrap, LV_PCT(100));
    lv_obj_set_height(qr_wrap, 184);
    lv_obj_set_style_bg_color(qr_wrap, lv_color_white(), 0);
    lv_obj_set_style_border_color(qr_wrap, lv_color_hex(GUIDED_BORDER), 0);
    lv_obj_set_style_border_width(qr_wrap, 1, 0);
    lv_obj_set_style_radius(qr_wrap, 8, 0);
    lv_obj_set_style_pad_all(qr_wrap, 10, 0);
    lv_obj_clear_flag(qr_wrap, LV_OBJ_FLAG_SCROLLABLE);
    qr = lv_qrcode_create(qr_wrap);
    lv_qrcode_set_size(qr, 156);
    lv_qrcode_set_dark_color(qr, lv_color_hex(GUIDED_TEXT));
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, url, strlen(url));
    lv_obj_center(qr);
}

static void __make_stepper(uint8_t current_step)
{
    static const char *step_numbers[] = {"1", "2", "3"};
    const char *step_names[] = {
        __tr("语言", "Language"),
        __tr("授权", "Auth"),
        __tr("联网", "Network"),
    };
    lv_obj_t *stepper;
    int i;

    stepper = lv_obj_create(sg_ui.overlay);
    lv_obj_set_size(stepper, LV_PCT(100), GUIDED_STEPPER_HEIGHT);
    lv_obj_align(stepper, LV_ALIGN_TOP_MID, 0, GUIDED_HEADER_HEIGHT);
    lv_obj_set_style_pad_all(stepper, 0, 0);
    lv_obj_set_style_radius(stepper, 0, 0);
    lv_obj_set_style_border_width(stepper, 0, 0);
    lv_obj_set_style_bg_color(stepper, lv_color_white(), 0);
    lv_obj_clear_flag(stepper, LV_OBJ_FLAG_SCROLLABLE);

    for (i = 0; i < GUIDED_STEP_COUNT - 1; i++) {
        lv_obj_t *line = lv_obj_create(stepper);
        lv_obj_set_size(line, LV_HOR_RES / 3 - GUIDED_STEPPER_CIRCLE, GUIDED_STEPPER_LINE);
        lv_obj_align(line, LV_ALIGN_TOP_MID,
                     i == 0 ? -(LV_HOR_RES / 6) : (LV_HOR_RES / 6), 19);
        lv_obj_set_style_pad_all(line, 0, 0);
        lv_obj_set_style_radius(line, GUIDED_STEPPER_LINE / 2, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_bg_color(line,
                                  current_step > (uint8_t)(i + 1)
                                      ? lv_color_hex(GUIDED_GREEN)
                                      : lv_color_hex(0xDDE7E1),
                                  0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    }

    for (i = 0; i < GUIDED_STEP_COUNT; i++) {
        lv_obj_t *circle = lv_obj_create(stepper);
        lv_obj_t *number = lv_label_create(circle);
        bool completed = current_step > (uint8_t)i;

        lv_obj_set_size(circle, GUIDED_STEPPER_CIRCLE, GUIDED_STEPPER_CIRCLE);
        lv_obj_align(circle, LV_ALIGN_TOP_MID, (i - 1) * (LV_HOR_RES / 3), 7);
        lv_obj_set_style_pad_all(circle, 0, 0);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(circle, completed ? 0 : 1, 0);
        lv_obj_set_style_border_color(circle, lv_color_hex(0xB8C8BE), 0);
        lv_obj_set_style_bg_color(circle,
                                  completed ? lv_color_hex(GUIDED_GREEN)
                                            : lv_color_hex(0xF3F6F4),
                                  0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

        __set_text_font(number);
        lv_obj_set_style_text_color(number,
                                     completed ? lv_color_white() : lv_color_hex(GUIDED_MUTED),
                                     0);
        lv_label_set_text(number, step_numbers[i]);
        lv_obj_center(number);
        lv_obj_set_ext_click_area(circle, 8);
        lv_obj_add_event_cb(circle, __action_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)(GUIDED_ACTION_STEP_LANGUAGE + i));

        lv_obj_t *name = lv_label_create(stepper);
        lv_obj_set_width(name, LV_HOR_RES / 3);
        lv_obj_set_height(name, LV_SIZE_CONTENT);
        __set_text_font(name);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(name,
                                    completed ? lv_color_hex(GUIDED_GREEN)
                                              : lv_color_hex(GUIDED_MUTED),
                                    0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_label_set_text(name, step_names[i]);
        lv_obj_align(name, LV_ALIGN_TOP_MID, (i - 1) * (LV_HOR_RES / 3), 39);
    }
}

static void __action_cb(lv_event_t *event)
{
    guided_action_t action = (guided_action_t)(uintptr_t)lv_event_get_user_data(event);

    switch (action) {
    case GUIDED_ACTION_LANG_ZH:
        guided_wechat_locale_set(GUIDED_WECHAT_LANG_ZH);
        ai_chat_ui_refresh_locale();
        guided_wechat_state_read(&sg_ui.state);
        if (sg_ui.page == GUIDED_PAGE_LANGUAGE_FIRST) {
            if (sg_ui.state.activated) {
                __enter_chat();
            } else {
                __show_page(sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                                ? GUIDED_PAGE_AUTH_MISSING
                                : GUIDED_PAGE_AUTH_READY,
                            GUIDED_PAGE_LANGUAGE_FIRST);
            }
        } else {
            __show_page(GUIDED_PAGE_MENU, GUIDED_PAGE_MENU);
        }
        break;
    case GUIDED_ACTION_LANG_EN:
        guided_wechat_locale_set(GUIDED_WECHAT_LANG_EN);
        ai_chat_ui_refresh_locale();
        guided_wechat_state_read(&sg_ui.state);
        if (sg_ui.page == GUIDED_PAGE_LANGUAGE_FIRST) {
            if (sg_ui.state.activated) {
                __enter_chat();
            } else {
                __show_page(sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                                ? GUIDED_PAGE_AUTH_MISSING
                                : GUIDED_PAGE_AUTH_READY,
                            GUIDED_PAGE_LANGUAGE_FIRST);
            }
        } else {
            __show_page(GUIDED_PAGE_MENU, GUIDED_PAGE_MENU);
        }
        break;
    case GUIDED_ACTION_NEXT:
        if (sg_ui.page == GUIDED_PAGE_AUTH_READY) {
            __show_page(GUIDED_PAGE_NETWORK, GUIDED_PAGE_AUTH_READY);
        }
        break;
    case GUIDED_ACTION_MENU:
        __show_page(GUIDED_PAGE_MENU, GUIDED_PAGE_MENU);
        break;
    case GUIDED_ACTION_NETWORK:
        /* Device settings opens the network details page. The guided stepper
         * is reserved for the first-time setup flow entered from auth. */
        __show_page(GUIDED_PAGE_NETWORK, GUIDED_PAGE_MENU);
        break;
    case GUIDED_ACTION_LANGUAGE:
        __show_page(GUIDED_PAGE_LANGUAGE, GUIDED_PAGE_MENU);
        break;
    case GUIDED_ACTION_ABOUT:
        __show_page(GUIDED_PAGE_ABOUT, GUIDED_PAGE_MENU);
        break;
    case GUIDED_ACTION_RESET:
        sg_ui.reset_parent_page = sg_ui.page;
        sg_ui.reset_parent_back_page = sg_ui.back_page;
        __show_page(GUIDED_PAGE_RESET_CONFIRM, sg_ui.page);
        break;
    case GUIDED_ACTION_RESET_CONFIRM:
        if (sg_ui.reset_pending) {
            break;
        }
        if (tuya_iot_client_get() == NULL) {
            lv_label_set_text(sg_ui.status_label,
                              __tr("设备服务尚未就绪，暂不能解绑。",
                                   "Device service is not ready; unbind is unavailable."));
            break;
        }
        {
            OPERATE_RET net_rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_RESET, NULL);
            if (net_rt != OPRT_OK) {
                PR_WARN("network config reset failed: %d", net_rt);
            }

            /* tuya_iot_reset() schedules the SDK reset even when its remote
             * unbind request returns an error. The local reset path is still
             * required so the next boot can enter phone provisioning. */
            guided_wechat_reprovision_mark();
            OPERATE_RET reset_rt = tuya_iot_reset(tuya_iot_client_get());
            if (reset_rt != OPRT_OK) {
                PR_WARN("native Tuya IoT reset returned: %d; local reset continues", reset_rt);
            }
            sg_ui.reset_pending = true;
            lv_label_set_text(sg_ui.status_label,
                              __tr("正在清理并重启。", "Clearing settings and restarting."));
            if (sg_ui.back_button != NULL) {
                lv_obj_add_state(sg_ui.back_button, LV_STATE_DISABLED);
            }
        }
        break;
    case GUIDED_ACTION_PAIRING:
        guided_wechat_state_read(&sg_ui.state);
        if (sg_ui.state.activated) {
            sg_ui.reset_parent_page = sg_ui.page;
            sg_ui.reset_parent_back_page = sg_ui.back_page;
            __show_page(GUIDED_PAGE_RESET_CONFIRM, sg_ui.page);
        } else if (sg_ui.state.pairing_state == GUIDED_WECHAT_PAIRING_IDLE) {
            guided_wechat_pairing_request();
            __show_page(GUIDED_PAGE_NETWORK, sg_ui.back_page);
        }
        break;
    case GUIDED_ACTION_STEP_LANGUAGE:
        __show_page(GUIDED_PAGE_LANGUAGE_FIRST, GUIDED_PAGE_LANGUAGE_FIRST);
        break;
    case GUIDED_ACTION_STEP_AUTH:
        guided_wechat_state_read(&sg_ui.state);
        __show_page(sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_AUTH_MISSING
                        : GUIDED_PAGE_AUTH_READY,
                    GUIDED_PAGE_LANGUAGE_FIRST);
        break;
    case GUIDED_ACTION_STEP_NETWORK:
        guided_wechat_state_read(&sg_ui.state);
        __show_page(GUIDED_PAGE_NETWORK,
                    sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_AUTH_MISSING
                        : GUIDED_PAGE_AUTH_READY);
        break;
    case GUIDED_ACTION_BACK:
        if (sg_ui.reset_pending) {
            break;
        }
        if (sg_ui.page == GUIDED_PAGE_RESET_CONFIRM) {
            __show_page(sg_ui.reset_parent_page, sg_ui.reset_parent_back_page);
            break;
        }
        if (sg_ui.back_page == GUIDED_PAGE_MENU && sg_ui.page == GUIDED_PAGE_MENU) {
            __enter_chat();
        } else if (sg_ui.page == GUIDED_PAGE_NETWORK &&
                   (sg_ui.back_page == GUIDED_PAGE_AUTH_READY ||
                    sg_ui.back_page == GUIDED_PAGE_AUTH_MISSING)) {
            __show_page(sg_ui.back_page, GUIDED_PAGE_LANGUAGE_FIRST);
        } else {
            __show_page(sg_ui.back_page, GUIDED_PAGE_MENU);
        }
        break;
    case GUIDED_ACTION_REFRESH:
        guided_wechat_state_read(&sg_ui.state);
        if (sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE) {
            __show_page(GUIDED_PAGE_AUTH_MISSING, GUIDED_PAGE_LANGUAGE_FIRST);
        } else if (__setup_complete()) {
            if (sg_ui.page == GUIDED_PAGE_NETWORK &&
                sg_ui.back_page == GUIDED_PAGE_AUTH_READY) {
                __enter_chat();
            } else {
                __show_page(GUIDED_PAGE_NETWORK, sg_ui.back_page);
            }
        } else if (sg_ui.page == GUIDED_PAGE_NETWORK) {
            __show_page(GUIDED_PAGE_NETWORK, sg_ui.back_page);
        } else {
            __show_page(GUIDED_PAGE_AUTH_READY, GUIDED_PAGE_LANGUAGE_FIRST);
        }
        break;
    default:
        break;
    }
}

static lv_obj_t *__make_button(lv_obj_t *parent, const char *text, guided_action_t action,
                               bool secondary, bool danger)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;

    lv_obj_set_width(button, LV_PCT(100));
    lv_obj_set_height(button, GUIDED_BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, secondary ? 1 : 0, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(danger ? GUIDED_DANGER : GUIDED_BORDER), 0);
    lv_obj_set_style_bg_color(button,
                              secondary ? lv_color_white()
                                        : lv_color_hex(danger ? GUIDED_DANGER : GUIDED_GREEN),
                              0);
    lv_obj_set_ext_click_area(button, 8);
    label = lv_label_create(button);
    __set_text_font(label);
    lv_obj_set_style_text_color(label,
                                secondary ? lv_color_hex(danger ? GUIDED_DANGER : GUIDED_TEXT)
                                          : lv_color_white(),
                                0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, __action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    return button;
}

static lv_obj_t *__begin_page(const char *title, bool allow_back)
{
    lv_obj_t *header;
    lv_obj_t *body;
    lv_obj_t *title_label;
    uint8_t guide_step = __guide_step(sg_ui.page);
    lv_coord_t body_top = GUIDED_HEADER_HEIGHT;

    lv_obj_clean(sg_ui.overlay);
    sg_ui.status_label = NULL;
    sg_ui.back_button = NULL;
    if (sg_ui.page != GUIDED_PAGE_RESET_CONFIRM) {
        sg_ui.reset_pending = false;
    }

    header = lv_obj_create(sg_ui.overlay);
    lv_obj_set_size(header, LV_PCT(100), GUIDED_HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(GUIDED_GREEN), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    if (allow_back) {
        lv_obj_t *back = lv_btn_create(header);
        lv_obj_t *back_label;
        lv_obj_set_size(back, 44, 40);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(back, 0, 0);
        lv_obj_set_style_border_width(back, 0, 0);
        lv_obj_set_ext_click_area(back, 8);
        back_label = lv_label_create(back);
        __set_text_font(back_label);
        lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
        lv_label_set_text(back_label, "<");
        lv_obj_center(back_label);
        sg_ui.back_button = back;
        lv_obj_add_event_cb(back, __action_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)GUIDED_ACTION_BACK);
    }

    title_label = lv_label_create(header);
    lv_obj_set_width(title_label, LV_PCT(68));
    __set_text_font(title_label);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(title_label, title);
    lv_obj_center(title_label);

    {
        lv_obj_t *wifi_icon = lv_label_create(header);
        const char *wifi_symbol = sg_ui.state.cloud_online || sg_ui.state.link_up
                                      ? FONT_AWESOME_WIFI
                                      : (sg_ui.state.ssid[0] != '\0'
                                             ? FONT_AWESOME_WIFI_WEAK
                                             : FONT_AWESOME_WIFI_OFF);
        lv_obj_set_style_text_font(wifi_icon, ai_ui_get_icon_font(), 0);
        lv_obj_set_style_text_color(wifi_icon, lv_color_white(), 0);
        lv_label_set_text(wifi_icon, wifi_symbol);
        lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    if (guide_step > 0) {
        __make_stepper(guide_step);
        body_top += GUIDED_STEPPER_HEIGHT;
    }

    body = lv_obj_create(sg_ui.overlay);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, LV_VER_RES - body_top);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(body, lv_color_hex(GUIDED_BG), 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_pad_all(body, 14, 0);
    lv_obj_set_style_pad_row(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    return body;
}

static void __make_badge(lv_obj_t *parent, const char *text, bool ok)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_t *label;

    lv_obj_set_width(badge, LV_PCT(100));
    lv_obj_set_height(badge, 42);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(ok ? 0xE8F7ED : 0xFFF2E5), 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    label = lv_label_create(badge);
    __set_text_font(label);
    lv_obj_set_style_text_color(label, lv_color_hex(ok ? 0x16813A : 0xA85B00), 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

static void __make_info_row(lv_obj_t *parent, const char *name, const char *value)
{
    char line[150];
    snprintf(line, sizeof(line), "%s\n%s", name, value != NULL && value[0] != '\0' ? value : "-");
    __make_label(parent, line, lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_LEFT);
}

static void __make_list_row(lv_obj_t *parent, const char *name, const char *value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *name_label = lv_label_create(row);
    lv_obj_t *value_label = lv_label_create(row);

    lv_obj_set_width(row, LV_PCT(100));
    /* Keep normal rows compact, but let long values grow with wrapped text. */
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 44, 0);
    lv_obj_set_style_pad_left(row, 0, 0);
    lv_obj_set_style_pad_right(row, 0, 0);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_set_style_pad_bottom(row, 8, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_border_color(row, lv_color_hex(GUIDED_BORDER), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    __set_text_font(name_label);
    lv_obj_set_width(name_label, LV_PCT(40));
    lv_obj_set_height(name_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(name_label, lv_color_hex(GUIDED_MUTED), 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(name_label, name != NULL ? name : "");
    lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 14, 0);

    __set_text_font(value_label);
    lv_obj_set_width(value_label, LV_PCT(52));
    lv_obj_set_height(value_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(value_label, lv_color_hex(GUIDED_TEXT), 0);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(value_label, value != NULL && value[0] != '\0' ? value : "-");
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -14, 0);
}

static void __render_language_first(void)
{
    lv_obj_t *body = __begin_page("TuyaOpen", false);
    lv_obj_t *card = __make_card(body);
    __make_label(card, "欢迎使用 TuyaOpen AI\nWelcome to TuyaOpen AI",
                 lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_CENTER);
    __make_label(card, "请选择设备语言\nChoose device language",
                 lv_color_hex(GUIDED_MUTED), LV_TEXT_ALIGN_CENTER);
    __make_button(card, "中文", GUIDED_ACTION_LANG_ZH,
                  guided_wechat_locale_get() != GUIDED_WECHAT_LANG_ZH, false);
    __make_button(card, "English", GUIDED_ACTION_LANG_EN,
                  guided_wechat_locale_get() != GUIDED_WECHAT_LANG_EN, false);
}

static void __render_auth_missing(void)
{
    lv_obj_t *body = __begin_page(__tr("设备授权", "Device authorization"), true);
    lv_obj_t *card = __make_card(body);
    __make_badge(card, __tr("设备未授权", "Device not authorized"), false);
    __make_label(card,
                 __tr("请先完成设备授权，才能使用 AI 云服务。",
                      "Authorize the device to use AI cloud services."),
                 lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_LEFT);
    __make_guide_qr(card, __auth_guide_url());
    __make_button(card, __tr("重新检测", "Check again"), GUIDED_ACTION_REFRESH, false, false);
}

static void __render_auth_ready(void)
{
    lv_obj_t *body = __begin_page(__tr("设备授权", "Device authorization"), true);
    lv_obj_t *card = __make_card(body);
    const char *source = sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_STORAGE
                             ? __tr("设备存储 / 模组授权", "Device storage / module")
                             : __tr("固件配置", "Firmware configuration");
    __make_badge(card, __tr("已授权", "Authorized"), true);
    __make_info_row(card, __tr("授权来源", "Authorization source"), source);
    __make_info_row(card, "UUID", sg_ui.state.uuid_masked);
    __make_info_row(card, "AuthKey", sg_ui.state.authkey_masked);
    __make_button(card, __tr("开始联网", "Connect now"), GUIDED_ACTION_NEXT,
                  false, false);
}

static const char *__network_stage(void)
{
    if (sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE) {
        return __tr("等待设备授权", "Waiting for device authorization");
    }
    if (sg_ui.state.link_up && sg_ui.state.cloud_online) {
        return __tr("已联网", "Connected");
    }
    if (sg_ui.state.pairing_state == GUIDED_WECHAT_PAIRING_WAITING_PHONE &&
        !sg_ui.state.link_up) {
        return __tr("等待手机配网", "Waiting for phone setup");
    }
    if (sg_ui.state.pairing_state == GUIDED_WECHAT_PAIRING_STARTING) {
        return __tr("正在准备", "Preparing");
    }
    if (sg_ui.state.link_up ||
        sg_ui.state.pairing_state == GUIDED_WECHAT_PAIRING_CONNECTING) {
        return __tr("正在联网", "Connecting");
    }
    return __tr("未联网", "Not connected");
}

static void __render_network(void)
{
    char current_network[100];
    lv_obj_t *body = __begin_page(__tr("设备联网", "Device network"), true);
    lv_obj_t *card = __make_card(body);

    __make_badge(card, __network_stage(),
                 sg_ui.state.link_up && sg_ui.state.cloud_online);
    snprintf(current_network, sizeof(current_network), "%s: %s",
             __tr("当前网络", "Current network"),
             sg_ui.state.link_up && sg_ui.state.ssid[0] != '\0'
                 ? sg_ui.state.ssid
                 : __tr("未连接", "Not connected"));
    __make_label(card, current_network, lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_CENTER);

    if (!sg_ui.state.cloud_online) {
        __make_guide_qr(card, __network_guide_url());
    }

    if (sg_ui.state.activated) {
        __make_button(card, __tr("重新配网", "Reconfigure"),
                      GUIDED_ACTION_PAIRING, false, false);
    } else if (sg_ui.state.pairing_state == GUIDED_WECHAT_PAIRING_IDLE) {
        __make_button(card, __tr("开始配网", "Start pairing"),
                      GUIDED_ACTION_PAIRING, false, false);
    }
}

static void __render_menu(void)
{
    lv_obj_t *body = __begin_page(__tr("设备设置", "Device settings"), true);
    lv_obj_t *card = __make_card(body);
    __make_button(card, __tr("设备联网", "Device network"),
                  GUIDED_ACTION_NETWORK, true, false);
    __make_button(card, __tr("选择语言", "Language"), GUIDED_ACTION_LANGUAGE, true, false);
    __make_button(card, __tr("关于设备", "About device"), GUIDED_ACTION_ABOUT, true, false);
}

static void __render_language(void)
{
    lv_obj_t *body = __begin_page(__tr("语言", "Language"), true);
    lv_obj_t *card = __make_card(body);
    __make_label(card, __tr("选择界面语言", "Choose interface language"),
                 lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_CENTER);
    __make_button(card, "中文", GUIDED_ACTION_LANG_ZH,
                  guided_wechat_locale_get() != GUIDED_WECHAT_LANG_ZH, false);
    __make_button(card, "English", GUIDED_ACTION_LANG_EN,
                  guided_wechat_locale_get() != GUIDED_WECHAT_LANG_EN, false);
}

static void __render_about(void)
{
    lv_obj_t *body = __begin_page(__tr("关于设备", "About device"), true);
    lv_obj_t *card = __make_card(body);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    __make_list_row(card, __tr("固件版本", "Firmware version"), sg_ui.state.version);
    __make_list_row(card, "TuyaOpen SDK", sg_ui.state.sdk_version);
    __make_list_row(card, "Product ID", sg_ui.state.product_id);
    __make_list_row(card, "Device ID", sg_ui.state.device_id);
    __make_list_row(card, "UUID", sg_ui.state.uuid_masked);
    __make_list_row(card, "AuthKey", sg_ui.state.authkey_masked);
    __make_list_row(card, __tr("Wi-Fi", "Wi-Fi"),
                    sg_ui.state.ssid[0] != '\0' ? sg_ui.state.ssid : "-");
    __make_list_row(card, __tr("云端状态", "Cloud state"),
                    sg_ui.state.cloud_online ? __tr("在线", "Online") : __tr("离线", "Offline"));
}

static void __render_reset_confirm(void)
{
    lv_obj_t *body = __begin_page(__tr("重新配网", "Re-provision"), true);
    lv_obj_t *card = __make_card(body);
    __make_badge(card, __tr("确认重新配网", "Confirm reconfiguration"), false);
    __make_label(card,
                 __tr("当前网络和设备绑定信息将被清除，设备重启后可以重新配网。",
                      "The current network and device binding will be cleared. The device will restart for setup."),
                 lv_color_hex(GUIDED_TEXT), LV_TEXT_ALIGN_LEFT);
    sg_ui.status_label = __make_label(card, "", lv_color_hex(GUIDED_DANGER), LV_TEXT_ALIGN_CENTER);
    __make_button(card, __tr("确认重配", "Confirm"),
                  GUIDED_ACTION_RESET_CONFIRM, false, true);
}

static void __show_page(guided_page_t page, guided_page_t back_page)
{
    if (page == GUIDED_PAGE_MENU && !__setup_complete()) {
        page = GUIDED_PAGE_NETWORK;
        back_page = sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_AUTH_MISSING
                        : GUIDED_PAGE_AUTH_READY;
    }
    sg_ui.page = page;
    sg_ui.back_page = back_page;
    guided_wechat_state_read(&sg_ui.state);
    lv_obj_clear_flag(sg_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.menu_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_ui.overlay);

    switch (page) {
    case GUIDED_PAGE_LANGUAGE_FIRST:
        __render_language_first();
        break;
    case GUIDED_PAGE_AUTH_MISSING:
        __render_auth_missing();
        break;
    case GUIDED_PAGE_AUTH_READY:
        __render_auth_ready();
        break;
    case GUIDED_PAGE_NETWORK:
        __render_network();
        break;
    case GUIDED_PAGE_MENU:
        __render_menu();
        break;
    case GUIDED_PAGE_LANGUAGE:
        __render_language();
        break;
    case GUIDED_PAGE_ABOUT:
        __render_about();
        break;
    case GUIDED_PAGE_RESET_CONFIRM:
        __render_reset_confirm();
        break;
    default:
        break;
    }
}

static void __menu_button_cb(lv_event_t *event)
{
    (void)event;
    if (__setup_complete()) {
        __show_page(GUIDED_PAGE_MENU, GUIDED_PAGE_MENU);
    } else {
        __show_page(GUIDED_PAGE_NETWORK, GUIDED_PAGE_AUTH_READY);
    }
}

void guided_wechat_menu_btn_set_hidden(bool hidden)
{
    if (NULL == sg_ui.menu_button) {
        return;
    }

    lv_vendor_disp_lock();
    if (hidden) {
        lv_obj_add_flag(sg_ui.menu_button, LV_OBJ_FLAG_HIDDEN);
    } else if (lv_obj_has_flag(sg_ui.overlay, LV_OBJ_FLAG_HIDDEN)) {
        /* Overlay hidden means the chat surface is active; guided pages keep
         * the menu button hidden via __show_page(). */
        lv_obj_clear_flag(sg_ui.menu_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(sg_ui.menu_button);
    }
    lv_vendor_disp_unlock();
}

static void __refresh_cb(lv_timer_t *timer)
{
    guided_wechat_device_state_t next;
    (void)timer;

    guided_wechat_state_read(&next);
    if (guided_wechat_state_equal(&next, &sg_ui.state)) {
        return;
    }
    sg_ui.state = next;

    if (sg_ui.page == GUIDED_PAGE_AUTH_MISSING &&
        sg_ui.state.auth_source != GUIDED_WECHAT_AUTH_NONE) {
        __show_page(GUIDED_PAGE_AUTH_READY, GUIDED_PAGE_LANGUAGE_FIRST);
    } else if (sg_ui.page == GUIDED_PAGE_NETWORK && __setup_complete()) {
        if (sg_ui.back_page == GUIDED_PAGE_AUTH_READY) {
            __enter_chat();
        } else {
            __show_page(GUIDED_PAGE_NETWORK, sg_ui.back_page);
        }
    } else if (sg_ui.page == GUIDED_PAGE_AUTH_READY &&
               !sg_ui.state.activated &&
               sg_ui.state.pairing_state != GUIDED_WECHAT_PAIRING_IDLE) {
        /* A bind event means the phone-provisioning flow has started. Keep
         * the guided stepper visible even when the previous page was the
         * startup authorization page. */
        __show_page(GUIDED_PAGE_NETWORK, GUIDED_PAGE_AUTH_READY);
    } else if (!(lv_obj_has_flag(sg_ui.overlay, LV_OBJ_FLAG_HIDDEN)) &&
               (sg_ui.page == GUIDED_PAGE_NETWORK || sg_ui.page == GUIDED_PAGE_MENU ||
                sg_ui.page == GUIDED_PAGE_ABOUT)) {
        __show_page(sg_ui.page, sg_ui.back_page);
    }
}

static OPERATE_RET __guided_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    bool language_selected;
    bool reprovision_pending;
    lv_obj_t *menu_icon;

    TUYA_CALL_ERR_RETURN(guided_wechat_core_init());
    language_selected = guided_wechat_locale_load();
    reprovision_pending = guided_wechat_reprovision_pending();
    if (reprovision_pending) {
        guided_wechat_reprovision_clear();
    }
    guided_wechat_state_read(&sg_ui.state);

    lv_vendor_disp_lock();
    sg_ui.overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sg_ui.overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(sg_ui.overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.overlay, 0, 0);
    lv_obj_set_style_border_width(sg_ui.overlay, 0, 0);
    lv_obj_set_style_radius(sg_ui.overlay, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.overlay, lv_color_hex(GUIDED_BG), 0);
    lv_obj_clear_flag(sg_ui.overlay, LV_OBJ_FLAG_SCROLLABLE);

    sg_ui.menu_button = lv_btn_create(lv_scr_act());
    lv_obj_set_size(sg_ui.menu_button, 42, 42);
    lv_obj_align(sg_ui.menu_button, LV_ALIGN_BOTTOM_LEFT, 18, -18);
    lv_obj_set_style_radius(sg_ui.menu_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sg_ui.menu_button, lv_color_white(), 0);
    lv_obj_set_style_border_color(sg_ui.menu_button, lv_color_hex(GUIDED_BORDER), 0);
    lv_obj_set_style_border_width(sg_ui.menu_button, 1, 0);
    lv_obj_set_style_shadow_width(sg_ui.menu_button, 8, 0);
    lv_obj_set_style_shadow_color(sg_ui.menu_button, lv_color_hex(0xB8BCC2), 0);
    lv_obj_set_ext_click_area(sg_ui.menu_button, 8);
    menu_icon = lv_label_create(sg_ui.menu_button);
    lv_obj_set_style_text_font(menu_icon, ai_ui_get_icon_font(), 0);
    lv_obj_set_style_text_color(menu_icon, lv_color_hex(GUIDED_TEXT), 0);
    lv_label_set_text(menu_icon, FONT_AWESOME_GEAR);
    lv_obj_center(menu_icon);
    lv_obj_add_event_cb(sg_ui.menu_button, __menu_button_cb, LV_EVENT_CLICKED, NULL);

    if (reprovision_pending) {
        __show_page(GUIDED_PAGE_NETWORK,
                    sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE
                        ? GUIDED_PAGE_AUTH_MISSING
                        : GUIDED_PAGE_AUTH_READY);
    } else if (!language_selected) {
        __show_page(GUIDED_PAGE_LANGUAGE_FIRST, GUIDED_PAGE_LANGUAGE_FIRST);
    } else if (sg_ui.state.auth_source == GUIDED_WECHAT_AUTH_NONE) {
        __show_page(GUIDED_PAGE_AUTH_MISSING, GUIDED_PAGE_LANGUAGE_FIRST);
    } else if (!sg_ui.state.activated) {
        __show_page(GUIDED_PAGE_AUTH_READY, GUIDED_PAGE_LANGUAGE_FIRST);
    } else {
        __enter_chat();
    }

    sg_ui.refresh_timer = lv_timer_create(__refresh_cb, 1000, NULL);
    lv_vendor_disp_unlock();
    return OPRT_OK;
}

OPERATE_RET ai_ui_chat_guided_wechat_register(void)
{
    return guided_wechat_register_with_init(__guided_ui_init);
}

#endif /* ENABLE_AI_CHAT_GUI_GUIDED_WECHAT */
