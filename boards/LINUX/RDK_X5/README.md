# RDK X5

RDK X5 uses the LINUX platform with the AArch64 GCC 14.2 cross toolchain.
Native builds on the X5 system use its installed GCC.

Initialize the SDK environment from the repository root (`.\export.ps1` on
Windows or `. ./export.sh` on Linux), then build:

```text
cd apps/tuya_cloud/switch_demo
tos.py config choice -d -c Linux.config
tos.py config set CONFIG_BOARD_CHOICE_RDK_X5=y
tos.py build
```

Copy the ELF from `dist/` to the X5 system and run it there. Configure valid
device credentials before building to test cloud connectivity.

This board registration keeps the shared Linux peripheral implementations
and numbering. I2C port 5 selects `/dev/i2c-5`; SPI port 2 selects
`/dev/spidev1.0`. UART device paths and GPIO line selection follow the existing
LINUX adapter. Peripheral wiring, pin mux and data transfers require separate
validation.
