/**
 * @file power_selftest.c
 * @brief On-target self-test for the power component. Flash to a battery board: it
 *        auto-checks (PASS/FAIL) what it can and, for the charger, guides you to
 *        plug/unplug. Results and prompts are shown BOTH on the UART log and, when
 *        the board has a display, on screen.
 *
 *        Board-agnostic: probes capabilities, iterates every power-domain role and
 *        skips the ones this board does not have. Build the matching config.
 *
 * @version 0.2
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_power_manage.h"
#include "board_com_api.h"

#if defined(DISPLAY_NAME)
#include "lvgl.h"
#include "lv_vendor.h"
#endif

/* live state mirrored to the screen */
static volatile uint32_t g_mv    = 0;
static volatile uint8_t  g_pct   = 0;
static volatile int      g_chg   = 0; // TDL_CHG_STATE_E
static int               g_pass  = 0, g_fail = 0;
static char              g_phase[48] = "starting";
static volatile int      g_evt_cnt = 0; // charge events seen (updated from TDL worker)

static const char *__chg_str(int st)
{
    return (TDL_CHG_CHARGING == st) ? "CHARGING" : (TDL_CHG_FULL == st) ? "FULL" : "DISCHARGE";
}

/* ---------------- screen (best-effort; only if the board has a display) ---------------- */
#if defined(DISPLAY_NAME)
static lv_obj_t *g_label = NULL;

static void __screen_update(void)
{
    char buf[192];

    if (NULL == g_label) {
        return;
    }
    // Cast the u32/u8 values to unsigned: uint32_t is unsigned int on some SoCs (T5)
    // and unsigned long on others (ESP32), so %u alone is not portable.
    snprintf(buf, sizeof(buf), "POWER SELF-TEST\n%u.%02uV   %u%%\n[ %s ]\n%s\nevt %d    P%d  F%d",
             (unsigned)(g_mv / 1000), (unsigned)((g_mv % 1000) / 10), (unsigned)g_pct, __chg_str(g_chg), g_phase,
             g_evt_cnt, g_pass, g_fail);

    lv_vendor_disp_lock();
    lv_label_set_text(g_label, buf);
    lv_vendor_disp_unlock();
}

static void __screen_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);
    g_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(g_label, lv_color_black(), LV_PART_MAIN);
#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(g_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    // Center everything: on the round 466x466 AMOLED the corners are physically
    // clipped, so top-left text would land off the visible circle.
    lv_obj_set_width(g_label, lv_pct(90));
    lv_label_set_long_mode(g_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_label, LV_ALIGN_CENTER, 0, 0);
    lv_vendor_start(5, 1024 * 8);
}
#else
static void __screen_update(void) {}
static void __screen_init(void) {}
#endif

static void ui_phase(const char *p)
{
    strncpy(g_phase, p, sizeof(g_phase) - 1);
    g_phase[sizeof(g_phase) - 1] = 0;
    PR_NOTICE(">> %s", p);
    __screen_update();
}

#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        if (cond) {                                                                                                   \
            g_pass++;                                                                                                 \
            PR_NOTICE("  [PASS] " __VA_ARGS__);                                                                       \
        } else {                                                                                                      \
            g_fail++;                                                                                                 \
            PR_NOTICE("  [FAIL] " __VA_ARGS__);                                                                       \
        }                                                                                                             \
    } while (0)

static void __on_charge_evt(TDL_CHG_STATE_E st, void *arg)
{
    (void)arg;
    g_evt_cnt++;
    g_chg = st;
    PR_NOTICE("  [event #%d] charge -> %s", g_evt_cnt, __chg_str(st));
    __screen_update();
}

static const struct {
    uint32_t    role;
    const char *name;
} ROLES[] = {
    {TDL_PWR_DOMAIN_DISPLAY, "DISPLAY"},      {TDL_PWR_DOMAIN_SD, "SD"},
    {TDL_PWR_DOMAIN_AUDIO, "AUDIO"},          {TDL_PWR_DOMAIN_CAMERA, "CAMERA"},
    {TDL_PWR_DOMAIN_CAMERA_AVDD, "CAM_AVDD"}, {TDL_PWR_DOMAIN_CAMERA_DVDD, "CAM_DVDD"},
    {TDL_PWR_DOMAIN_RTC, "RTC"},              {TDL_PWR_DOMAIN_JOYSTICK, "JOYSTICK"},
    {TDL_PWR_DOMAIN_CELLULAR, "CELLULAR"},
};
#define ROLE_CNT (sizeof(ROLES) / sizeof(ROLES[0]))

static void __read_live(TDL_POWER_HANDLE h)
{
    uint32_t        mv = 0;
    uint8_t         pct = 0;
    TDL_CHG_STATE_E st = TDL_CHG_DISCHARGE;
    if (OPRT_OK == tdl_power_battery_get_voltage(h, &mv)) {
        g_mv = mv;
    }
    if (OPRT_OK == tdl_power_battery_get_percent(h, &pct)) {
        g_pct = pct;
    }
    if (OPRT_OK == tdl_power_charger_get_state(h, &st)) {
        g_chg = st;
    }
}

void user_main(void)
{
    OPERATE_RET      rt   = OPRT_OK;
    TDL_POWER_HANDLE h    = NULL;
    TDL_POWER_INFO_T info = {0};
    TDL_CHG_STATE_E  st   = TDL_CHG_DISCHARGE;
    BOOL_T           on   = FALSE;
    int              i;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();
    board_register_hardware();
    __screen_init();

    PR_NOTICE("=============== POWER COMPONENT SELF-TEST ===============");

    h = tdl_power_find(POWER_NAME);
    CHECK(NULL != h, "tdl_power_find(\"%s\") returns a handle", POWER_NAME);
    if (NULL == h) {
        ui_phase("NO POWER DEVICE");
        while (1) {
            tal_system_sleep(1000);
        }
    }
    __read_live(h);

    /* battery landmarks */
    ui_phase("battery info");
    rt = tdl_power_get_info(h, &info);
    if (OPRT_OK == rt) {
        PR_NOTICE("  landmarks: full=%u empty=%u low=%u critical=%u mV, curve=%u", (unsigned)info.battery.v_full_mv,
                  (unsigned)info.battery.v_empty_mv, (unsigned)info.battery.v_low_mv,
                  (unsigned)info.battery.v_critical_mv, (unsigned)info.battery.curve_cnt);
        CHECK(info.battery.v_full_mv > info.battery.v_empty_mv, "v_full > v_empty");
    }

    /* battery voltage / percent */
    ui_phase("battery reading");
    uint32_t mv  = 0;
    uint8_t  pct = 0;
    rt           = tdl_power_battery_get_voltage(h, &mv);
    if (OPRT_NOT_SUPPORTED == rt) {
        PR_NOTICE("  no battery on this board");
    } else {
        g_mv = mv;
        CHECK(OPRT_OK == rt && mv > 2000 && mv < 5000, "voltage plausible (2-5V): %u mV", (unsigned)mv);
        rt = tdl_power_battery_get_percent(h, &pct);
        g_pct = pct;
        CHECK(OPRT_OK == rt && pct <= 100, "percent 0-100: %u%%", (unsigned)pct);
    }

    /* charger state */
    ui_phase("charger state");
    rt = tdl_power_charger_get_state(h, &st);
    if (OPRT_NOT_SUPPORTED != rt) {
        g_chg = st;
        CHECK(OPRT_OK == rt && (TDL_CHG_DISCHARGE == st || TDL_CHG_CHARGING == st || TDL_CHG_FULL == st),
              "charge state valid: %s", __chg_str(st));
    }
    __screen_update();

    /* power-domain set/get roundtrip (real toggle, restored) */
    ui_phase("domain roundtrip");
    for (i = 0; i < (int)ROLE_CNT; i++) {
        BOOL_T on0, on1, on2;
        if (OPRT_NOT_SUPPORTED == tdl_power_domain_get(h, (TDL_POWER_DOMAIN_E)ROLES[i].role, &on0)) {
            continue; // role not on this board
        }
        tdl_power_domain_set(h, ROLES[i].role, on0 ? FALSE : TRUE);
        tal_system_sleep(20);
        tdl_power_domain_get(h, (TDL_POWER_DOMAIN_E)ROLES[i].role, &on1);
        tdl_power_domain_set(h, ROLES[i].role, on0);
        tal_system_sleep(20);
        tdl_power_domain_get(h, (TDL_POWER_DOMAIN_E)ROLES[i].role, &on2);
        CHECK((on1 != on0) && (on2 == on0), "domain %s roundtrip", ROLES[i].name);
    }
    CHECK(OPRT_NOT_SUPPORTED == tdl_power_domain_get(h, (TDL_POWER_DOMAIN_E)(1u << 20), &on),
          "unmapped role -> NOT_SUPPORTED");
    __screen_update();

    /* interactive charger event */
    rt = tdl_power_charger_on_event(h, __on_charge_evt, NULL);
    if (OPRT_NOT_SUPPORTED != rt) {
        int start = g_evt_cnt;
        for (i = 20; i > 0; i--) {
            char p[48];
            snprintf(p, sizeof(p), "PLUG/UNPLUG CHARGER (%ds)", i);
            ui_phase(p);
            __read_live(h);
            __screen_update();
            tal_system_sleep(1000);
        }
        CHECK(g_evt_cnt > start, "got a charge event (%d)", g_evt_cnt - start);
    }

    ui_phase("DONE");
    PR_NOTICE("=============== RESULT: %d passed, %d failed ===============", g_pass, g_fail);

    while (1) {
        __read_live(h);
        __screen_update();
        PR_NOTICE("[live] %u mV, %u%%, %s", (unsigned)g_mv, (unsigned)g_pct, __chg_str(g_chg));
        tal_system_sleep(5000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
    while (1) {
        tal_system_sleep(500);
    }
}
#else
static THREAD_HANDLE ty_app_thread = NULL;
static void          tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth   = 1024 * 6;
    thrd_param.priority     = THREAD_PRIO_1;
    thrd_param.thrdname     = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
