# L511C-Y7PM TuyaOpen bridge overlay

This versioned overlay adapts TuyaOpen to the official `TuyaOpen-LYNQ` branch
`l511c-y7pm`, pinned at `350c04d12d8b8f50463134c2c2e966bf43d60f3b`.

From the TuyaOpenSDK root, initialize the environment and apply the bridge once:

```sh
. ./export.sh
.venv/bin/python tools/porting/l511c_y7pm/apply.py
```

If `platform/L511C_Y7PM` is absent, the script shallow-clones the branch and,
if its tip has advanced, fetches and checks out the pinned commit detached.
An existing directory that is not a Git checkout is left untouched; an
existing checkout at another commit is rejected. The script checks the vendor
patch applicability and overlay target hashes before writing, and can be
rerun safely. The tracked patch and overlay are the persistence source; the
vendor checkout itself is ignored.

Select the board's default configuration, then build from the switch demo:

```sh
cd apps/tuya_cloud/switch_demo
tos.py config choice -d -c L511C_Y7PM
tos.py build
```

The TuyaOpen build wrapper invokes the vendor `sdk/PLAT/GccBuild_L511C_Y7PM.sh`
flow. That script directly runs `make` for the bootloader and
`tuyaos_adapter` targets, then packages the result; it does not call
`build_c_pm.sh`. Vendor feature flags, including `AUDIO_ENABLE=true`, follow
the official L511C-Y7PM target settings required by its prebuilt ABI and memory
layout. This does not enable the TuyaOpen board's CAT1 player option. No L511G
target or prebuilt library is used.

The initial bridge includes app startup, cellular state/event API, filesystem,
UART, and system base support. Cellular uses the vendor MDS registration
callback; `app.c` calls `main()` from `app_init()`, and the overlay `main()`
starts `tuya_app_main()`. This is a compile-time integration; hardware behavior
still requires board validation. No physical GPIO switch, LED, or unverified
peripheral is enabled.

The `.bin` file emitted in the project's `dist/` directory is a TuyaOpen naming
alias containing the vendor `.binpkg` payload. For actual flashing, use the
vendor-generated `platform/L511C_Y7PM/sdk/PLAT/L511C_Y7PM_download/L511C_Y7PM.binpkg`.
