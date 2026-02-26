## Tuya MimiClaw on T5AI

This project runs **MimiClaw**, an OpenClaw‑style local‑first AI assistant, on the **Tuya T5AI** development board and optional **Linux runtime**.  
Plug in power, connect Wi‑Fi, and your T5AI board becomes a 24/7 AI butler that talks to you via Feishu / Telegram / Discord, calls mainstream LLMs (DeepSeek, Qwen, Claude, GPT, etc.), remembers you in local text files, and can directly operate rich TuyaOpen hardware peripherals.

TuyaOpen provides drivers, display, peripherals, and connectivity layers, greatly expanding what MimiClaw can do on real products: AI lamps, pet robots, toys, dashboards, and more — all powered by an embedded agent that lives entirely on your device.


### What you get

- **Local‑first AI assistant** running on Tuya T5AI or Linux
- **Multiple LLM providers**: DeepSeek, Qwen, Claude, GPT, and other OpenAI/Anthropic‑compatible endpoints
- **Multiple chat channels**: Telegram / Discord / Feishu (configurable)
- **Persistent memory** stored as plain text on flash
- **Cron‑style scheduler & heartbeat loop** so the AI can wake itself up and act
- **Direct hardware control** via TuyaOpen drivers, sensors, and GPIOs

---

## What’s different from the upstream MimiClaw project

Upstream repository: [`memovai/mimiclaw`](https://github.com/memovai/mimiclaw)

This TuyaOpen port is **based on** the upstream MimiClaw project and keeps the core ideas (embedded agent loop, tools, cron, heartbeat, local memory), but makes several changes:

- **Target hardware**
  - **Tuya T5AI board** as a first‑class target, with TuyaOpen’s SDK, display, and peripherals
  - **Linux runtime** support for development, debugging, and running the agent directly on a host
- **Chat channels**
  - Adds first‑class **Feishu** support, in addition to **Telegram** and **Discord**
  - Unified CLI to switch the active channel at runtime
- **Model configuration**
  - Supports both **OpenAI** and **Anthropic** providers, plus **compatible base URLs**
  - CLI and config‑file driven model/provider selection
- **TuyaOpen integration**
  - Uses TuyaOpen’s rich **drivers, sensors, and displays**, enabling the AI to read sensors, drive GPIOs, and control more complex hardware devices
  - Fits into the TuyaOpen examples/app structure under `apps/tuya_mimiclaw`

Please refer to the upstream README for detailed architecture notes of MimiClaw itself; this document focuses on the TuyaOpen‑specific integration and usage.

---

## Quick migration & configuration guide (Tuya MimiClaw)

These steps assume you have already built and flashed the firmware (or are running the Linux runtime).  
Connect to the **serial console at 115200 baud**, open the programming port, and you will see the CLI.

### 1. Discover CLI commands

After opening the serial console (115200), type:

```text
help
```

This prints the list of supported CLI commands for this port.

### 2. Configure model provider & API

You can choose `openai` or `anthropic` as the provider. The project also supports **OpenAI/Anthropic‑compatible base URLs**.

Typical steps:

1. Select provider (for example, OpenAI or Anthropic compatible):
   - `set_model_provider openai`
   - or `set_model_provider anthropic`
2. Set API key:
   - `set_api_key sk-xxxx...`
3. (Optional) Use a compatible API base URL:
   - `set_api_url https://your-compatible-endpoint.example.com`
4. Set model name:
   - `set_model gpt-4o` (or any supported model by your provider)

If you provide a compatible URL, make sure both **API_KEY** and **API_URL** are set, otherwise calls will fail.

### 3. Connect to Wi‑Fi

Use the CLI to configure your router SSID and password:

```text
set_wifi <ssid> <password>
restart
```

After `restart`, the device will reconnect using the new Wi‑Fi settings.

### 4. Choose your chat channel (Telegram / Discord / Feishu)

You can run MimiClaw through different social channels. Currently supported:

- `telegram`
- `discord`
- `feishu`

Switch the active channel:

```text
set_channel_mode telegram
```

or

```text
set_channel_mode feishu
```

or

```text
set_channel_mode discord
```

Then, according to the selected channel, configure the required credentials (token / appid / secret, etc.) via the CLI. After configuration, reboot:

```text
restart
```

### 5. Configure via `mimi_secrets.h` (optional)

Instead of entering everything over serial, you can pre‑configure secrets at build time:

1. Copy the example file:

   ```bash
   cp apps/tuya_mimiclaw/mimi_secrets.h.example apps/tuya_mimiclaw/mimi_secrets.h
   ```

2. Edit `mimi_secrets.h` and fill in Wi‑Fi credentials, API keys, model provider, chat channel, and any optional settings.
3. Rebuild and flash the firmware so the new defaults are baked in.

### 6. Configuration priority rules

- **CLI settings have the highest priority.**  
  Any parameter set via the serial CLI overrides build‑time settings.
- If a value is **not set in the CLI**, the firmware falls back to **`mimi_secrets.h`**.
- If a required parameter is missing **both** in CLI and in `mimi_secrets.h`, the program will error or refuse to start the corresponding feature.

This makes it easy to ship sensible defaults in firmware while still allowing flexible per‑device overrides in the field.

---

## Supported chipsets / runtimes

| Name            | Support Status | Notes                                                                 |
| --------------- | -------------- | --------------------------------------------------------------------- |
| Tuya T5AI board | Supported      | Main embedded target; uses TuyaOpen SDK, display, and hardware stack. |
| Linux runtime   | Supported      | Run the same agent loop directly on a Linux host for dev and testing. |

---

## Acknowledgements

This project is **heavily inspired by and derived from** the upstream work in [`memovai/mimiclaw`](https://github.com/memovai/mimiclaw), which demonstrates how to run an OpenClaw‑style AI assistant on an ESP32‑S3 with:

- Pure C firmware, no Linux or heavy runtime
- Local‑first memory and on‑device text files (`SOUL.md`, `USER.md`, `MEMORY.md`, etc.)
- Tool calling, cron scheduler, and heartbeat‑driven autonomous tasks

The Tuya MimiClaw port keeps these core ideas and adapts them to the **TuyaOpen** ecosystem, focusing on:

- T5AI hardware, displays, and peripherals
- Integration with TuyaOpen’s AI/IoT workflows
- Expanded channel and model options for real products

Original MimiClaw is licensed under the **MIT License**; please refer to the upstream repository for full license details and ongoing development. We are deeply grateful to the MimiClaw authors and community for making this possible.

