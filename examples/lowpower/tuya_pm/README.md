# tuya_pm example (fixed-level power measurement)

Pins the device power-level manager (`tuya_pm`) at **one fixed level** and holds it,
so the actual current can be measured on a power analyzer. There is no idle decay:
the level is set once via `tuya_pm_request()` and stays put.

Peripherals are registered as real `tdl_power` rails (audio / display / SD), so the
held level reflects true power: at DORMANT / DEEPSLEEP those rails are gated off.

## Choosing the level

Edit `PM_DEMO_HOLD_LEVEL` at the top of `src/example_tuya_pm.c`:

| Level | What it holds |
|---|---|
| `TUYA_PM_ACTIVE`    | full speed, no WiFi PS, rails on |
| `TUYA_PM_STANDBY`   | WiFi PS dtim1, CPU sleep, rails on |
| `TUYA_PM_DORMANT`   | WiFi PS dtim10, CPU sleep, audio/display/SD rails off |
| `TUYA_PM_DEEPSLEEP` | CPU powered down; wakes (reboots) on P43. Real power-off auto-compiles where the platform has TKL wakeup (`ENABLE_WAKEUP`) |

Then reflash and measure.

## WiFi (required for DORMANT)

DORMANT is an online DTIM keep-alive level, so the example **gates it on WiFi**: until
the device associates to an AP it is pinned at STANDBY (via a `link` hold-lock), and it
only descends into DORMANT once WiFi is up. If the AP drops, it falls back to STANDBY.

Fill in `PM_DEMO_WIFI_SSID` / `PM_DEMO_WIFI_PWD` at the top of the source to join a
network (plain STA connect, no Tuya pairing/cloud needed). Left empty (or if the AP is
never reached), the gate stays held and the device sits at STANDBY — it will **not**
enter DORMANT offline. Other measured levels (ACTIVE / STANDBY / DEEPSLEEP) are not
gated.

## Notes

- With no AP, a `PM_DEMO_HOLD_LEVEL = DORMANT` build stays at STANDBY by design; measure
  another level, or connect to an AP, to exercise DORMANT.
- `board_register_hardware()` provides the `power` device the rails bind to.
- Battery/charge policy is left off here to keep the measurement clean; enable it in the
  policy (`.battery`) if you want charging to pin ACTIVE.

## Build & run

```shell
cd examples/lowpower/tuya_pm
tos.py config choice          # pick the board (or copy a config/*.config to app_default.config)
tos.py build
```

Provided config: `ZECTRIX_T5AI_NOTE_4` (with `CONFIG_ENABLE_TUYA_PM=y`, which on a
WiFi platform also pulls in the WiFi ULP deep-sleep backend; deep sleep auto-compiles
where TKL wakeup exists).
