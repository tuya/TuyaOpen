/**
 * @file test_power_waveshare.c
 * @brief Host unit tests for the power component, based on the WAVESHARE board's
 *        configuration (SoC backend: ADC divider battery + single-line GPIO charger,
 *        no power domains, 11-point LiPo curve). Runs the REAL tdl_power.c and
 *        tdd_power_soc.c against mocked tkl_adc / tkl_gpio. No hardware needed.
 *
 * Build & run: ./run.sh
 */
#include <stdio.h>
#include <string.h>
#include "tuya_cloud_types.h"

/* ---------- mock state ---------- */
static int32_t           g_adc_seq[32];             // ADC raw values the mock returns (cycled)
static int               g_adc_len  = 0;
static int               g_adc_idx  = 0;
static int               g_adc_fail = 0;            // 1 => every ADC read fails
static TUYA_GPIO_LEVEL_E g_chrg_lvl = TUYA_GPIO_LEVEL_HIGH; // CHRG pin level the mock reports
static uint32_t          g_fake_mv  = 0;            // exact voltage for the curve-only backend

/* ---------- real component sources under test ---------- */
#include "../tdl_power/src/tdl_power.c"
#include "../tdd_power/src/tdd_power_soc.c"

/* ---------- tkl mocks (prototypes come from the stub headers included above) ---------- */
OPERATE_RET tkl_adc_read_single_channel(TUYA_ADC_NUM_E n, uint8_t ch, int32_t *data)
{
    (void)n;
    (void)ch;
    if (g_adc_fail || 0 == g_adc_len) {
        return OPRT_COM_ERROR;
    }
    *data = g_adc_seq[g_adc_idx % g_adc_len];
    g_adc_idx++;
    return OPRT_OK;
}
OPERATE_RET tkl_adc_init(TUYA_ADC_NUM_E n, TUYA_ADC_BASE_CFG_T *c) { (void)n; (void)c; return OPRT_OK; }
OPERATE_RET tkl_adc_deinit(TUYA_ADC_NUM_E n) { (void)n; return OPRT_OK; }

OPERATE_RET tkl_gpio_read(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E *level) { (void)pin; *level = g_chrg_lvl; return OPRT_OK; }
OPERATE_RET tkl_gpio_init(TUYA_GPIO_NUM_E pin, const TUYA_GPIO_BASE_CFG_T *cfg) { (void)pin; (void)cfg; return OPRT_OK; }
OPERATE_RET tkl_gpio_write(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level) { (void)pin; (void)level; return OPRT_OK; }
OPERATE_RET tkl_gpio_deinit(TUYA_GPIO_NUM_E pin) { (void)pin; return OPRT_OK; }
OPERATE_RET tkl_gpio_irq_init(TUYA_GPIO_NUM_E pin, const TUYA_GPIO_IRQ_T *cfg) { (void)pin; (void)cfg; return OPRT_OK; }
OPERATE_RET tkl_gpio_irq_enable(TUYA_GPIO_NUM_E pin) { (void)pin; return OPRT_OK; }
OPERATE_RET tkl_gpio_irq_disable(TUYA_GPIO_NUM_E pin) { (void)pin; return OPRT_OK; }

/* ---------- a curve-only backend: returns an exact voltage, no gauge (for A2) ---------- */
static OPERATE_RET fake_batt_voltage(TDD_POWER_DEV_HANDLE_T ctx, uint32_t *mv) { (void)ctx; *mv = g_fake_mv; return OPRT_OK; }

/* ---------- WAVESHARE data ---------- */
static const TDL_POWER_OCV_PT_T WS_CURVE[] = {
    {2800, 0},  {3100, 10}, {3280, 20}, {3440, 30}, {3570, 40}, {3680, 50},
    {3780, 60}, {3880, 70}, {3980, 80}, {4090, 90}, {4200, 100},
};

/* ---------- tiny assert harness ---------- */
static int g_pass = 0, g_fail = 0;
#define CHECK(cond, name)                                                                                              \
    do {                                                                                                              \
        if (cond) {                                                                                                   \
            g_pass++;                                                                                                 \
            printf("  PASS  %s\n", name);                                                                             \
        } else {                                                                                                      \
            g_fail++;                                                                                                 \
            printf("  FAIL  %s\n", name);                                                                             \
        }                                                                                                             \
    } while (0)

static void set_adc(const int32_t *seq, int len)
{
    memcpy(g_adc_seq, seq, len * sizeof(int32_t));
    g_adc_len  = len;
    g_adc_idx  = 0;
    g_adc_fail = 0;
}
static uint32_t near(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= 2; } // float tolerance

int main(void)
{
    /* ---- register the WAVESHARE SoC power device (real backend) ---- */
    static const TDD_POWER_ADC_BATTERY_T ws_batt = {
        .adc_num = TUYA_ADC_NUM_0, .adc_ch = 15, .divider_ratio = 4.922f, .samples = 16,
    };
    static const TDD_POWER_GPIO_CHARGER_T ws_chg = {
        .chrg_pin = TUYA_GPIO_NUM_30, .chrg_active = TUYA_GPIO_LEVEL_LOW,
        .stdby_pin = TDD_POWER_PIN_NONE, .stdby_active = TUYA_GPIO_LEVEL_HIGH,
    };
    TDD_POWER_SOC_CFG_T soc = {0};
    soc.battery = &ws_batt;
    soc.charger = &ws_chg;
    soc.info.battery = (TDL_POWER_BATTERY_INFO_T){.v_full_mv = 4200, .v_empty_mv = 2800, .v_low_mv = 3300,
                                                  .v_critical_mv = 3100, .curve = WS_CURVE, .curve_cnt = 11};
    CHECK(OPRT_OK == tdd_power_soc_register("power", &soc), "register WAVESHARE SoC power device");
    TDL_POWER_HANDLE h = tdl_power_find("power");
    CHECK(NULL != h, "find(\"power\") returns a handle");
    CHECK(NULL == tdl_power_find("nope"), "find(unknown) returns NULL");

    uint32_t        mv  = 0;
    uint8_t         pct = 0;
    TDL_CHG_STATE_E st;
    BOOL_T          on;
    OPERATE_RET     rt;

    printf("[A1] battery voltage (peak of 16 samples, 2-point cal, x4.922 divider)\n");
    { int32_t s[] = {2113};            set_adc(s, 1); tdl_power_battery_get_voltage(h, &mv); CHECK(near(mv, 4200), "A1-1 raw 2113 -> ~4200mV"); }
    { int32_t s[] = {1767};            set_adc(s, 1); tdl_power_battery_get_voltage(h, &mv); CHECK(near(mv, 3499), "A1-2 raw 1767 -> ~3499mV"); }
    { int32_t s[] = {1521};            set_adc(s, 1); tdl_power_battery_get_voltage(h, &mv); CHECK(near(mv, 3000), "A1-3 raw 1521 -> ~3000mV"); }
    { int32_t s[] = {1767,1700,1650,1600}; set_adc(s, 4); tdl_power_battery_get_voltage(h, &mv); CHECK(near(mv, 3499), "A1-4 sagging seq -> peak 1767 (~3499mV, not the mean)"); }
    { g_adc_fail = 1; rt = tdl_power_battery_get_voltage(h, &mv); CHECK(OPRT_COM_ERROR == rt, "A1-5 all ADC reads fail -> OPRT_COM_ERROR"); }

    printf("[A2] battery percent from 11-point curve (exact mV via curve-only backend)\n");
    {
        TDL_POWER_INTFS_T mi = {0};
        mi.battery_get_voltage = fake_batt_voltage;
        TDL_POWER_INFO_T minfo = {0};
        minfo.battery = (TDL_POWER_BATTERY_INFO_T){.v_full_mv = 4200, .v_empty_mv = 2800, .curve = WS_CURVE, .curve_cnt = 11};
        tdl_power_register("pt", &mi, &minfo, (TDD_POWER_DEV_HANDLE_T)1);
        TDL_POWER_HANDLE hp = tdl_power_find("pt");
        g_fake_mv = 4200; tdl_power_battery_get_percent(hp, &pct); CHECK(100 == pct, "A2-1 4200mV -> 100%");
        g_fake_mv = 3680; tdl_power_battery_get_percent(hp, &pct); CHECK(50 == pct, "A2-2 3680mV -> 50%");
        g_fake_mv = 3730; tdl_power_battery_get_percent(hp, &pct); CHECK(55 == pct, "A2-3 3730mV -> 55% (interpolated)");
        g_fake_mv = 2800; tdl_power_battery_get_percent(hp, &pct); CHECK(0 == pct, "A2-4 2800mV -> 0%");
        g_fake_mv = 2500; tdl_power_battery_get_percent(hp, &pct); CHECK(0 == pct, "A2-5 2500mV -> 0% (clamped)");
    }

    printf("[A3] charger state (single line P30, low=charging, no STDBY -> never FULL)\n");
    g_chrg_lvl = TUYA_GPIO_LEVEL_LOW;  tdl_power_charger_get_state(h, &st); CHECK(TDL_CHG_CHARGING == st,   "A3-1 P30 low -> CHARGING");
    g_chrg_lvl = TUYA_GPIO_LEVEL_HIGH; tdl_power_charger_get_state(h, &st); CHECK(TDL_CHG_DISCHARGE == st,  "A3-2 P30 high -> DISCHARGE");
    g_chrg_lvl = TUYA_GPIO_LEVEL_LOW;  tdl_power_charger_get_state(h, &st); CHECK(TDL_CHG_FULL != st,       "A3-3 never reports FULL (no STDBY line)");

    printf("[A4] power domains (WAVESHARE has none)\n");
    rt = tdl_power_domain_set(h, TDL_PWR_DOMAIN_SD, FALSE); CHECK(OPRT_OK == rt,            "A4-1 domain_set(no domains) -> OK no-op");
    rt = tdl_power_domain_get(h, TDL_PWR_DOMAIN_SD, &on);   CHECK(OPRT_NOT_SUPPORTED == rt, "A4-2 domain_get(no domains) -> NOT_SUPPORTED");

    printf("[A5] board-declared battery landmarks (get_info)\n");
    {
        TDL_POWER_INFO_T info = {0};
        rt = tdl_power_get_info(h, &info);
        CHECK(OPRT_OK == rt && 4200 == info.battery.v_full_mv && 2800 == info.battery.v_empty_mv &&
                  3300 == info.battery.v_low_mv && 3100 == info.battery.v_critical_mv && 11 == info.battery.curve_cnt,
              "A5-1 get_info returns WAVESHARE landmarks (4200/2800/3300/3100, 11-pt curve)");
    }

    printf("\n==== %d passed, %d failed ====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
