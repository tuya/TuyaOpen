# Minimal TAL CLI Example

This example starts only the Tuya Abstract Layer (TAL) CLI and registers a
local `sys_random` command. It does not initialize Tuya IoT, the network
manager, Wi-Fi, Bluetooth, KV storage or provisioning.

## Build

```sh
cd examples/system/cli
tos.py build
```

The example does not define its own board or platform configuration. Select a
compatible TuyaOpen configuration that enables UART and the serial CLI before
building.

## Commands

```text
help
version
sys_random
sys_random 32 0
sys_random 64 256
sys_random 64 2147483647
```

`sys_random` calls `tal_system_get_random()` and checks that each returned
sample is within the requested range. It does not perform statistical
randomness-quality testing.
