# JieLi boards

Each board directory carries the hardware facts the platform stages into the
vendor SDK. `Kconfig` selects the board; `jieli_board.json` describes it.

## `jieli_board.json`

`schema_version` is `1`. The platform reads this file while building the staging
tree and fails if a required field is missing, so a board added here needs no
platform change.

| Field | Meaning |
| :--- | :--- |
| `board` | Must equal the directory name. |
| `usb_device` | Vendor USB device macro: `TCFG_HUSB_DEVICE` for a host port, `TCFG_FUSB_DEVICE` for a full-speed device port. |
| `memory.flash_mib` / `memory.sdram_mib` | On-chip flash and external SDRAM, in MiB. |
| `memory.external_ddr` | `true` when the board actually populates the external DDR. The vendor SDK otherwise disables it via `CONFIG_NO_SDRAM_ENABLE`. |
| `log_uart.port` | SDK UART index used for the log. The platform asserts the board's documented port. |
| `log_uart.tx_pin` | Required when the chip's board file has to be rewritten, which is the wl82 path; omit it on a chip that only sets the baudrate, as wl83 does. A chip that rewrites the pin rejects a board that omits it, because it would have no pin to write. |
| `service_uart` | `null` when the board has no second UART. Otherwise `port`, `tx_pin` and `rx_pin` for the UART the TAL CLI is routed to. |
| `display` | `null` for a board with no panel. Otherwise `panel_macro`, `device_name`, `reset_io` and `backlight_io`. |

Fields the platform does not read are a mistake, not a harmless extra: they
suggest a fact is being honoured when it is not.

## `audio_config.h`

Boards with audio declare it there rather than in the JSON, because the values
are a vendor C initialisation table. `AUDIO_PROFILES` in the platform names the
chip's audio profile; the board supplies the data.
