# e-Paper Example

## Overview

This repository contains example projects and drivers for several common e-Paper (e-ink) displays (for example, 1.54inch and 4.26inch). The examples use a local driver library and demonstrate how to configure, build, and flash the examples in the Tuya development environment.

## Sample Pin Mapping

The following is a sample mapping. Refer to your display and board datasheets for exact wiring:

- SCLK: `P02`
- DIN:  `P04`
- CS:   `P03`
- DC:   `P07`
- RST:  `P08`
- BUSY: `P06`
- PWR:  `P28`

If your hardware uses different pins, update the wiring accordingly and modify the SPI/pin configuration in `lib/Config/DEV_Config.c`.

## Quick Start

1. Enter the Tuya build environment.
1. Change to the folder for the display you are using (for example `4.26inch_e-Paper`):

```powershell
cd 4.26inch_e-Paper
```

1. Configure the project (the examples were tested with the `T5AI Board`):

```powershell
tos.py config choice
```

1. Build the project:

```powershell
tos.py build
```

1. After building, flash the firmware to your device:

```powershell
tos.py flash
```

Tip: If you encounter build or flash errors, first verify the SPI and pin configuration in `lib/Config/DEV_Config.c` matches your target board.

## Adding or Porting a Display Example

To add a new display example or port a driver:

1. Replace or add the example program under `examples/`, for example `EPD_xxxx_test.c`.
2. Replace or add the display driver files under `lib/e-Paper/` (for example `EPD_xxxx.c` and `EPD_xxxx.h`).
3. Ensure the example's test function is named `EPD_test`; the example `main` calls this function to run the demo.

## Folder Structure

- `examples/`: example programs and test code.
- `lib/`: drivers, fonts and GUI helper code.

For additional help, check READMEs in each subfolder or contact the repository maintainer.

## Screen Links

- [Waveshare 4.26inch e-Paper](https://www.waveshare.com/4.26inch-e-paper.htm)
- [Waveshare 4.26inch e-Paper HAT](https://www.waveshare.com/4.26inch-e-paper-hat.htm)
- [Waveshare 1.54inch e-Paper](https://www.waveshare.com/1.54inch-e-paper.htm)
- [Waveshare 1.54inch e-Paper Module](https://www.waveshare.com/1.54inch-e-paper-module.htm)

