# Prebuilt firmware

`tuya_t5_pocket_ai_QIO_0.0.1.bin` is a ready-to-flash image of the
[`tuya_t5_pocket_ai`](../tuya_t5_pocket_ai) demo for the **TUYA_T5AI_POCKET**
board. It is provided so the board can be brought up without setting up the
toolchain first.

| Item | Value |
| :--- | :--- |
| Board | TUYA_T5AI_POCKET (T5AI / BK7258) |
| App | `apps/tuya_t5_pocket/tuya_t5_pocket_ai` |
| App version | 0.0.1 |
| Flash mode | QIO |
| T5AI platform commit | `e324ca0ffd5f7dbedaa28ae01fc48ba2e2493096` |
| MD5 | `036ea0aacbc97e4dc1db37fd3c56e2bd` |

## Flashing

With the TuyaOpen toolchain installed, from the app directory:

```bash
cd apps/tuya_t5_pocket/tuya_t5_pocket_ai
tos.py flash -p <port>
```

Or flash this image directly with the bundled tool:

```bash
tools/tyutool/tyutool_cli write -d t5 -f apps/tuya_t5_pocket/bin/tuya_t5_pocket_ai_QIO_0.0.1.bin -p <port>
```

## Rebuilding

This image is a build artifact, not a source file. To rebuild it from the
current tree:

```bash
cd apps/tuya_t5_pocket/tuya_t5_pocket_ai
tos.py clean
tos.py config choice -c TUYA_T5AI_POCKET.config
tos.py build
```

The output lands in `.build/bin/tuya_t5_pocket_ai_QIO_0.0.1.bin`.
