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

## Flash layout

The vendor downloader (`isd_config_rule.c`) reserves one TuyaOpen span and
places the rest with `AUTO`, so the map has three regions on the 8 MiB part:

| Region | Range | Size | Contents |
| :--- | :--- | :--- | :--- |
| Application | `0x000000`–`0x400000` | 4 MiB | `app.bin` followed by the 32 KiB VM region the downloader places after it |
| TuyaOpen reserved | `0x400000`–`0x4D1000` | `0xD1000` | `KV_KEY` 4 KiB, `KV_DATA` 64 KiB, `UF` 512 KiB, `RCD` 192 KiB, `USER0` 64 KiB |
| Vendor `AUTO` | `0x4D1000`–`0x800000` | ~3.2 MiB | `BTIF`, `PROFILE`, `USER`, `FIX`/`LOG` as the downloader places them |

The application base is 4 MiB because the downloader rejects an image that does
not fit below the TuyaOpen span (`ERROR: The reserved(TUYAOPEN) config address
is less then the flash bin size`). The guided WeChat UI image is ~3.1 MiB, so
the region leaves ~0.86 MiB of headroom on top of the VM. The span addresses
come from `JIELI_*_START/SIZE` in
`platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_flash.c`; the platform
rewrites the reserved block in the staged `isd_config_rule.c` from those
values, so moving a partition only needs the C file changed.

`TUYAOPEN_OPT=1` makes the downloader leave the span untouched, which is what
keeps a reflash from erasing provisioning data. Moving the span loses that data
once: the KV the firmware reads no longer contains it (device has to be
provisioned again).

Where the size goes, measured on the guided UI image (`-Oz -flto` already):
the 18 px CJK text font's glyph bitmap is ~0.59 MiB, a second glyph table plus
two descriptor arrays add ~0.43 MiB, and the KWS model is ~0.13 MiB — about a
third of the image is font and model data. `CONFIG_FONT_TEXT_SIZE_*` and
`CONFIG_FONT_EMOJI_SIZE_32` are the knobs; the alternative is a font subset or a
UI resource pack in its own partition.

## `audio_config.h`

Boards with audio declare it there rather than in Kconfig, because the values
are a vendor C initialisation table. `AUDIO_PROFILES` in the platform names the
chip's audio profile; the board supplies the data.
