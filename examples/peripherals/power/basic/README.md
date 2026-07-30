# Power — Basic Usage

Minimal, board-agnostic demo of the `power` component (`tdl_power`): read battery voltage/percentage, query the board-declared battery landmarks, subscribe to charge-state events, and switch power domains by **semantic role**.

The driver only provides mechanism (facts + switches); the app owns policy. This example shows the typical app flow and a simple low-battery policy built on the board-declared `critical` threshold. Roles a board does not have are skipped gracefully, so the same code runs on every battery board.

For the power API, see `src/peripherals/power/tdl_power/include/tdl_power_manage.h`.

## What it does

1. `tdl_power_find(POWER_NAME)` — get the board's power device.
2. `tdl_power_get_info()` — read battery landmarks (full / empty / low / critical mV).
3. `tdl_power_charger_on_event()` — subscribe to charge-state changes (falls back to polling if unsupported).
4. `tdl_power_domain_set(mask, FALSE/TRUE)` — enter/leave a low-power state by cutting/restoring heavy rails (camera / SD / cellular / audio / display) via one role mask.
5. Loop: poll voltage/percent/charge-state and apply an example low-battery policy.

## Supported boards

Battery boards that register a `power` device. Configs provided:

| Board | Notes |
|---|---|
| TUYA_T5AI_POCKET | AXP2101 PMIC + SoC GPIO (multi-contributor; has CELLULAR domain) |
| ZECTRIX_T5AI_NOTE_4 | SoC ADC battery + GPIO charger |

## Build & run

```shell
cd examples/peripherals/power/basic
tos.py config choice          # pick the board (or copy a config/*.config to app_default.config)
tos.py build
tos.py flash
tos.py monitor
```

Expected log:

```
[ty N] battery landmarks: full=4200mV empty=2800mV low=3300mV critical=3100mV
[ty N] >> entering low-power: cut camera / SD / cellular / audio / display
[ty N] >> leaving low-power: restore those rails
[ty N] battery: 3980 mV, 78%, discharging | SD domain: on
[ty N] [event] charge state changed -> charging
```

## See also

- `../selftest` — comprehensive on-target hardware self-test (PASS/FAIL + on-screen results).
