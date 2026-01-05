# e-Paper Touch Example 

Overview

This example demonstrates how to configure, build, flash, and port touch-enabled e-Paper (e-ink) displays in the Tuya development environment using the local driver library (for example, the Waveshare 2.13inch Touch e-Paper HAT).

Recommended Wiring (example)

Use the mapping below as a reference. Always check your display and board datasheets for exact wiring.

- DIN: `P16`
- SCLK: `P14`
- CS: `P18`
- DC: `P19`
- RST: `P47`
- BUSY: `P46`
- I2C_INT: `P44`
- I2C_CLK: `P42`
- I2C_DIN: `P43`
- I2C_RST: `P40`

If your hardware differs, update the pin mapping and edit the SPI/pin settings in `lib/Config/DEV_Config.c`.

Quick Start

1. Enter the Tuya build environment.
2. Change to the example folder for your display (e.g. `2.13inch_e-Paper_Touch`):

```powershell
cd 2.13inch_e-Paper_Touch
```

3. Configure the project (examples tested with the `T5AI Board`):

```powershell
tos.py config choice
```

4. Build the project:

```powershell
tos.py build
```

5. Flash the firmware to your device:

```powershell
tos.py flash
```

Tip: If you see build or flash errors, first verify `lib/Config/DEV_Config.c` contains the correct SPI and pin settings for your board.

Porting or Adding a New Display

To add a new example or port a third-party driver:

1. Add or replace the example program under `examples/` (for example `EPD_xxxx_test.c`).
2. Add or replace the display driver under `lib/e-Paper/` (`EPD_xxxx.c` / `EPD_xxxx.h`).
3. Add or modify the touch driver under `lib/e-Driver/`.
4. Ensure the example's test function is named `EPD_test`; the example `main` calls this function to run the demo.

Repository Layout

- `examples/` — example programs and test code.
- `lib/` — drivers, fonts, and GUI helper code.

Need help? Check each subfolder's README or contact the repository maintainer.

Example Screen Links

- [Waveshare 2.13inch Touch e-Paper HAT](https://www.waveshare.com/2.13inch-touch-e-paper-hat.htm)

Hardware Recommendation

- If you are using the Waveshare module above, we recommend connecting it via the board's 40-pin IO header for more stable power and signal routing.
