# JieLi boards

Each board directory carries the hardware facts the platform stages into the
vendor SDK. `Kconfig` selects the board and describes it; `audio_config.h`
carries the audio data, which is a vendor C initialisation table rather than a
set of scalar facts.

## Board facts

They are ordinary Kconfig symbols, so the platform reads them from the
evaluated config the same way it reads every other setting and `tos.py config
menu` shows them. A board added here needs no platform change.

| Symbol | Meaning |
| :--- | :--- |
| `JIELI_BOARD_USB_ROLE` | `host` or `device`. The platform maps this to the vendor's `TCFG_HUSB_DEVICE` / `TCFG_FUSB_DEVICE`. |
| `JIELI_BOARD_FLASH_MIB` / `JIELI_BOARD_SDRAM_MIB` | On-chip flash and external SDRAM, in MiB. |
| `JIELI_BOARD_EXTERNAL_DDR` | Whether the board populates the external DDR. The vendor SDK disables it with `CONFIG_NO_SDRAM_ENABLE` unless this says otherwise. |
| `JIELI_BOARD_LOG_UART_PORT` | SDK UART index used for the log. |
| `JIELI_BOARD_LOG_UART_TX_PIN` | Empty unless the staged board file has to be rewritten. A chip that only sets the baudrate leaves the vendor header's pin alone and leaves this empty. |
| `JIELI_BOARD_HAS_SERVICE_UART` | Whether the board carries the second UART the TAL CLI is routed to. When set, `JIELI_BOARD_SERVICE_UART_{PORT,TX_PIN,RX_PIN}` apply. |
| `JIELI_BOARD_HAS_DISPLAY` | Whether the board has a panel. When set, `JIELI_BOARD_DISPLAY_{PANEL,RESET_IO,BACKLIGHT_IO}` apply. |

None of these has a prompt: they are facts about the board, not choices for
whoever builds it. Every board declares the same names, so the platform reads
one set whichever is selected; Kconfig resolves the value from the node that
the board choice makes visible.

Values that would name a vendor macro stay semantic - `usb_role`, the panel -
and the platform owns the mapping. Pin names are the SDK's own vocabulary and
are written as the vendor headers spell them.

## `audio_config.h`

Boards with audio declare it there rather than in Kconfig, because the values
are a vendor C initialisation table. `AUDIO_PROFILES` in the platform names the
chip's audio profile; the board supplies the data.
