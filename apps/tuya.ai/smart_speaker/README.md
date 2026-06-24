English | [简体中文](./README_zh.md)

# smart_speaker

TuyaOpen smart speaker application: voice dialogue, local prompts, DND, and DP control.

## Directory layout

```text
smart_speaker/
├── README.md          # This document (English)
├── README_zh.md       # Chinese version
├── test/              # Self-test scripts (see headers + test/README.md)
├── include/ / src/    # Application code
└── config/            # Board configs
```

## Quick start

### Build

```bash
cd apps/tuya.ai/smart_speaker
python3 ../../../tos.py build
```

Output: `dist/smart_speaker_1.0.0/smart_speaker_1.0.0.elf`

Default `x86_64_linux_ubuntu.config` uses the **production** cloud backend (manual E2E).
Self-tests require the **loopback** profile:

```bash
python3 ../../../tos.py config choice -c x86_64_linux_ubuntu_loopback.config
python3 ../../../tos.py build
```

### Self-test


### Production cloud E2E (x86 manual)

```bash
python3 ../../../tos.py config choice -c x86_64_linux_ubuntu.config
python3 ../../../tos.py build
./dist/smart_speaker_1.0.0/smart_speaker_1.0.0.elf
```

1. Wait for log `cloud_cap ... online` (~15–20s after boot).
2. **`r`** → speak 2–5s → **`t`** (host has no hardware VAD; **`t` is required**).
3. Wait for TTS to finish before the next round; use **`e`** for one-shot r+t cycle.
4. **`d`** — force interrupt if stuck (rare after Phase 14 oneshot release on session end).

```bash
bash test/dialog_flow_test.sh
bash test/dialog_mode_test.sh
bash test/alert_playback_test.sh
bash test/file_inject_test.sh
bash test/dp_test.sh
bash test/media_playback_test.sh
```

CLI quick reference: **[test/README.md](test/README.md)**. Per-script steps and cases: see each `test/*.sh` header comment.

## Architecture overview

```text
smart_speaker (product: DP, CLI, prompt mapping)
    │
    └── voice_app_compat (orch / cloud / player / DP / storage)
            │
            └── tuya_voice_service (SDK adapters)
                    └── tuya_ai_service / audio_player / audio_front
```

### Initialization chain

```text
tuya_main → app_smart_speaker_init()
  → cloud_cap_comm_register_backend(production)
  → voice_service_adapter_init() → cloud_cap_comm_init() → tuya_ai_agent_init()
  → voice_app_cloud_init() / voice_app_player_init()
  → audio_front_adapter_register_backend(speaker_front_backend)
  → audio_front_adapter_init/start()
  → voice_app_start() → mgr thread (200ms tick)
```

Uplink PCM: `ALSA / :fi` → `speaker_front_backend` → `voice_service_adapter_upload_audio()` → `tuya_ai_agent_upload_stream()` (no ring buffer).

## Module boundaries

| ID | Convergence | Benefit |
|----|-------------|---------|
| A1 | `voice_service_adapter` multi-subscribe event bus | orch / player share `TTS_*` events |
| A2 | `is_dialoging()` not aliased to online | Online state not mistaken for active dialog |
| A3 | Cloud backend / init / callback owners separated | No implicit state from double init |
| A4 | compat decoupled via `voice_app_product_port` | No private `speaker_*` includes in compat |
| A5 | Product DP owned by `speaker_dp.c` | compat keeps uplink MQTT bridge only |
| A6 | `ai_skill_play_ops_register()` fail-fast | Global singleton not silently overridden |

**Principles:** single source of truth · one-way dependency (`smart_speaker → voice_app_compat → tuya_ai_service`) · fail-fast · incremental evolution.

## Verification gaps

| ID | Note |
|----|------|
| G1 | Requires real `tuya_config_local.h` credentials; placeholder uuid soft-skips |
| G2 | DP uplink MQTT verified hermetically only; real cloud needs credentials |
| G3 | Host `tos.py build` must run **serially** (shared `platform/LINUX/build`) |

## CLI summary

| Command | Description |
|---------|-------------|
| `:mode single\|free\|multi` | Dialog mode (recommended; mutually exclusive) |
| `:ct on\|off` / `:mt on\|off` | Continue / multi-turn (legacy aliases) |
| `:vol N` | Volume 0–100 |
| `:alert N` | Trigger prompt event |
| `:play_url URL` | BG music URL |
| `:play_file` / `:play_prefix` | Local MP3 |
| `:fi` / `:dump_*` | Uplink inject / PCM dump |
| `:state` | Print device state |

Keys `s/r/t/e/f` etc.: see [test/README.md](test/README.md) and `test/dialog_flow_test.sh` header.

## Media playback (summary)

| Capability | Status |
|------------|--------|
| FG prompts / BG music / FG preempts BG | ✅ |
| Event→prompt mapping / wakeup reply tone | ✅ |
| playlist NEXT/PREV / `play_mp3_file` / DP205 sync | ✅ |
| Suppress dingdong during TTS / async FG EOS resumes BG | ✅ |
| BT music (DP206/5/6) | Scaffold only; full stack deferred |
| Linux ALSA underrun | Occasional; prepare/start recovery in place |

See [voice_app_compat README](../../../src/voice_components/voice_app_compat/README.md).

## Key source files

| Role | Path |
|------|------|
| App entry | `src/app_smart_speaker.c` |
| CLI / keys | `src/speaker_cmd.c` |
| Prompts / DND | `src/speaker_hw.c` |
| DP | `src/speaker_dp.c` |
| PCM / VAD backend | `src/speaker_front_backend.c` |
| Uplink inject | `src/speaker_file_inject.c` |
| Orchestrator | `src/voice_components/voice_app_compat/src/voice_app_orch.c` |
| Cloud bridge | `src/tuya_voice_service/cloud_cap_comm_adapter/` |

## Maintenance notes

- Keep host example builds serial
- Changes to `voice_app_interrupt_dialog()` need high-impact review
- Use real credentials and cloud DP verification before release
- DP208 `reply`/`CTalk` non-bool JSON values are ignored (no error DP uplink)

## Related documentation

| Document | Content |
|----------|---------|
| [test/README.md](test/README.md) | Self-test index, CLI quick ref, one-shot regression |
| [voice_app_compat README](../../../src/voice_components/voice_app_compat/README.md) | Compat API, dialog modes, engineering backlog |
| [tuya_voice_service README](../../../src/tuya_voice_service/README.md) | SDK adapter layer |
| [examples/voice_service README](../../../examples/voice_service/README.md) | Host regression examples |
