# Power — On-Target Self-Test

Board-agnostic hardware self-test for the `power` component (`tdl_power`). Flash it to a battery board: it auto-checks (PASS/FAIL) everything it can and, for the charger, guides you through a plug/unplug step. Results and prompts are shown **both** on the UART log and, when the board has a display, **on screen** — handy for single-USB-port boards where unplugging to test charging also drops the serial log.

## What it checks

1. **find** — `tdl_power_find(POWER_NAME)` returns a handle.
2. **info** — battery landmarks are sane (`v_full > v_empty`).
3. **battery** — voltage plausible (2–5 V) and percentage in 0–100 (skipped if the board has no battery).
4. **charger** — `charger_get_state()` returns a valid state.
5. **domain roundtrip** — for every role the board has, toggle it and read back, then restore; an unmapped role must return `NOT_SUPPORTED`.
6. **charge event (interactive)** — a 20 s window; plug/unplug the charger and it should fire a charge event.
7. **live loop** — keeps printing/refreshing voltage, percent and charge state.

On-screen it shows a compact summary (centered so it stays inside a round display):

```
POWER SELF-TEST
3.98V   78%
[ CHARGING ]
> PLUG/UNPLUG CHARGER (12s)
evt 2    P5  F0
```

## Supported boards

| Board | Battery | Charger | Domains | Screen |
|---|---|---|---|---|
| WAVESHARE_T5AI_TOUCH_AMOLED_1_75 | ADC | GPIO (CHRG only) | none | 466×466 AMOLED |
| TUYA_T5AI_POCKET | AXP2101 | AXP | incl. CELLULAR | LCD |
| TUYA_T5AI_EINK_NFC | ADC | GPIO | — | e-paper |
| ZECTRIX_T5AI_NOTE_4 | ADC | GPIO | DISPLAY | SSD2683 e-paper |

Boards without a display fall back to UART-only automatically (the on-screen code is gated on `DISPLAY_NAME`). The screen config adds `CONFIG_ENABLE_LIBLVGL=y`.

## Build & run

```shell
cd examples/peripherals/power/selftest
tos.py config choice          # pick the board (or copy a config/*.config to app_default.config)
tos.py build
tos.py flash
tos.py monitor
```

Then watch the screen (or UART): during the 20 s window, plug/unplug USB and confirm `[ CHARGING ] <-> [ DISCHARGE ]` flips and `evt` increments.

## Notes

- **Charge state while charging reads a bit high** — the charger raises the terminal voltage; for true SOC, read at rest (charger unplugged, settled a minute).
- **Battery calibration** — if the reported voltage disagrees with a meter, it's the SoC ADC (the divider is a physical constant). Calibrate **at rest** with two points and set `cal_low`/`cal_span` in the board's battery config; don't fudge `divider_ratio`.

## See also

- `../basic` — minimal API-usage demo.
