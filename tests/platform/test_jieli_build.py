import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "platform" / "JIELI"))

import jieli_build
from tools.jieli_build import audio_profile, board_config, sdk_overlay


def _tkl_audio_source():
    return (ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio.c").read_text(
        encoding="utf-8"
    )


def _tkl_chip_region(source, chip_guard):
    """Extract one chip's provider region from the folded tkl_audio.c.

    The refactor deleted the per-chip provider files and folded them into
    tkl_audio.c behind JIELI_SELECTED_CHIP_* guards. The WL83 guard also wraps
    the shared includes at the top of the file, so anchor on the guard that
    directly precedes the #elif separating the two providers.
    """
    if chip_guard == "WL82":
        marker = "#elif defined(JIELI_SELECTED_CHIP_WL82)"
        start = source.index(marker)
        return source[start:]
    end = source.index("#elif defined(JIELI_SELECTED_CHIP_WL82)")
    start = source.rindex("#if defined(JIELI_SELECTED_CHIP_WL83)", 0, end)
    return source[start:end]


class JieliBuildTest(unittest.TestCase):
    def test_resolve_sdk_root_prefers_environment(self):
        with tempfile.TemporaryDirectory() as temp:
            sdk = pathlib.Path(temp) / "sdk"
            makefile = sdk / "apps/demo/demo_hello/board/wl82/Makefile"
            makefile.parent.mkdir(parents=True)
            makefile.write_text("# test SDK marker")
            resolved = jieli_build.resolve_sdk_root(
                {"JIELI_SDK_ROOT": str(sdk)}, ROOT / "platform" / "JIELI"
            )
            self.assertEqual(sdk, resolved)

    def test_build_command_targets_ac7916a_demo(self):
        command = jieli_build.build_make_command(
            pathlib.Path("/sdk"), pathlib.Path("/toolchain/bin"), jobs=3
        )
        expected_board = pathlib.Path("/sdk") / "apps/demo/demo_hello/board/wl82"
        self.assertEqual(command[0:3], ["make", "-C", str(expected_board)])
        self.assertIn(f"TOOL_DIR={pathlib.Path('/toolchain/bin').as_posix()}", command)
        self.assertIn("-j3", command)
        self.assertEqual(command[-2:], ["pre_build", "../../../../../cpu/wl82/tools/sdk.elf"])

    def test_elf_alone_is_not_a_flash_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            tools = pathlib.Path(temp)
            (tools / "sdk.elf").write_bytes(b"elf")
            with self.assertRaises(jieli_build.BuildError):
                jieli_build.find_qio_artifact(tools)

    def test_find_qio_artifact_accepts_jieli_package(self):
        with tempfile.TemporaryDirectory() as temp:
            tools = pathlib.Path(temp)
            package = tools / "jl_isd.ufw"
            package.write_bytes(b"firmware")
            self.assertEqual(package, jieli_build.find_qio_artifact(tools))

    def test_staging_tree_replaces_vendor_app_entry(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            sdk = root / "sdk"
            demo_board = sdk / "apps/demo/demo_hello/board/wl82"
            demo_board.mkdir(parents=True)
            (demo_board / "Makefile").write_text(
                "c_SRC_FILES := ../../../../../apps/demo/demo_hello/app_main.c\n"
                "INCLUDES := -I../../../../../include_lib\n"
                "LFLAGS += --start-group \\\n"
                "    ../../../../../cpu/wl82/liba/cpu.a \\\n"
                "    --end-group\n"
                "c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)\n",
                encoding="utf-8",
            )
            (demo_board / "board.c").write_text(
                '#include "asm/includes.h"\n'
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "UART2_PLATFORM_DATA_END();\n"
                "REGISTER_DEVICES(device_table) = {\n"
                "};\n"
                "void debug_uart_init() { uart_init(&uart2_data); }\n"
                "void board_early_init(void) {\n"
                "    devices_init();\n"
                "}\n"
                "void board_init(void)\n"
                "{\n"
                "}\n",
                encoding="utf-8",
            )
            demo_include = sdk / "apps/demo/demo_hello/include"
            demo_include.mkdir(parents=True)
            (demo_include / "app_config.h").write_text(
                "#ifndef APP_CONFIG_H\n"
                "#define APP_CONFIG_H\n"
                "#define __FLASH_SIZE__ (8 * 1024 * 1024)\n"
                "#define __SDRAM_SIZE__ (8 * 1024 * 1024)\n"
                "#endif\n",
                encoding="utf-8",
            )
            for name in ("include_lib", "lib", "tools"):
                (sdk / name).mkdir()
            (sdk / "cpu/wl82").mkdir(parents=True)
            (sdk / "apps/common").mkdir(parents=True)
            staging = root / "staging"

            jieli_build.create_staging_tree(
                sdk,
                staging,
                ROOT,
                tuya_lib_dir=root / "tuya_lib",
                uart_log_port=1,
                platform_root=ROOT / "platform" / "JIELI",
                chip=jieli_build.resolve_chip(chip_name="wl82"),
            )

            staged_makefile = staging / "build/apps/demo/demo_hello/board/wl82/Makefile"
            content = staged_makefile.read_text(encoding="utf-8")
            # The vendor entry point is replaced by the TuyaOpen entry...
            self.assertNotIn("../../../../../apps/demo/demo_hello/app_main.c", content)
            self.assertIn("../../../../../tuyaos/entry/jieli_app_entry.c", content)
            self.assertTrue((staging / "build/tuyaos/entry/jieli_app_entry.c").is_file())
            # ...and the adapter sources come from the shared manifest.
            self.assertIn("../../../../../tuyaos_adapter/src/system/tkl_output.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_wifi.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_audio.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_vad.c", content)
            self.assertIn("-I../../../../../tuyaos_adapter/include/system", content)
            # The audio profile's vendor runtime is staged onto the build too.
            self.assertIn("../../../../../apps/common/audio_music/audio_config.c", content)
            self.assertIn("../../../../../cpu/wl82/liba/audio_server.a", content)

    def test_ac79_official_pb3_log_uart_is_applied_to_staging(self):
        with tempfile.TemporaryDirectory() as temp:
            board = pathlib.Path(temp) / "board.c"
            board.write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "    .port = PORT_REMAP,\n"
                "    .tx_pin = IO_PORTB_03,\n"
                "UART2_PLATFORM_DATA_END();\n"
                "void debug_uart_init() { uart_init(&uart2_data); }\n"
            )
            jieli_build.configure_ac79_log_uart(board, uart_port=1, baudrate=1000000)
            content = board.read_text()

        self.assertIn("UART1_PLATFORM_DATA_BEGIN(uart1_data)", content)
        self.assertIn(".baudrate = 1000000", content)
        self.assertIn(".tx_pin = IO_PORTB_03", content)
        self.assertIn("uart_init(&uart1_data);", content)

    def test_service_uart_uses_uart0_on_pa5_pa6_away_from_pb3_log_uart(self):
        with tempfile.TemporaryDirectory() as temp:
            board = pathlib.Path(temp) / "board.c"
            board.write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "    .port = PORT_REMAP,\n"
                "    .tx_pin = IO_PORTB_03,\n"
                "    .rx_pin = -1,\n"
                "    .flags = UART_DEBUG,\n"
                "UART2_PLATFORM_DATA_END();\n"
                "REGISTER_DEVICES(device_table) = {\n"
                "    {\"uart2\", &uart_dev_ops, (void *)&uart2_data },\n"
                "};\n"
            )
            jieli_build.configure_service_uart(board)
            content = board.read_text()

        # The Tuya logical UART0/TAL CLI routes to UART0 on PA5/PA6 and is
        # registered alongside - never instead of - the vendor uart2 device.
        self.assertIn("UART0_PLATFORM_DATA_BEGIN(uart0_data)", content)
        self.assertIn(".baudrate = 115200", content)
        self.assertIn(".port = PORTA_5_6", content)
        self.assertIn(".tx_pin = IO_PORTA_05", content)
        self.assertIn(".rx_pin = IO_PORTA_06", content)
        self.assertIn('{"uart0", &uart_dev_ops, (void *)&uart0_data }', content)
        self.assertIn('{"uart2", &uart_dev_ops, (void *)&uart2_data }', content)

    def test_jieli_board_exposes_media_gate_for_tdd_audio(self):
        kconfig = (ROOT / "boards/JIELI/Kconfig").read_text(encoding="utf-8")
        self.assertRegex(kconfig, r"config ENABLE_MEDIA\s+bool[^\n]*\n\s+default n")

    def test_ac791_devkit_k1_ladder_is_calibrated_for_the_adc_button(self):
        """The DevKit K1 sits on the ADC ladder (PB1 / AD_CH_PB01).

        The refactor dropped the build-time board.c key patching; the ladder
        calibration now lives in the board's adkey_button_config.h and
        board_com_api registers it through Tuya's ADC button TDD when
        ENABLE_JIELI_ADKEY_BUTTON is selected.
        """
        ladder = (ROOT / "boards/JIELI/AC79_DevKitBoard/adkey_button_config.h").read_text(
            encoding="utf-8"
        )
        self.assertRegex(ladder, r"#define\s+JIELI_K1_ADC_CHANNEL\s+3U")
        self.assertRegex(ladder, r"#define\s+JIELI_K1_PRESSED_MIN\s+824U")
        self.assertRegex(ladder, r"#define\s+JIELI_K1_PRESSED_MAX\s+1007U")

        board_api = (ROOT / "boards/JIELI/board_com_api.c").read_text(encoding="utf-8")
        self.assertIn("tdd_adc_button_register", board_api)
        self.assertIn(".adc_ch = JIELI_K1_ADC_CHANNEL", board_api)

        kconfig = (ROOT / "boards/JIELI/Kconfig").read_text(encoding="utf-8")
        self.assertIn("config ENABLE_JIELI_ADKEY_BUTTON", kconfig)
        self.assertIn("select ENABLE_BUTTON_ADC", kconfig)
        self.assertIn("config ENABLE_JIELI_NATIVE_KEY", kconfig)

    def test_ac792_board_k1_uses_slot_zero_of_the_native_adkey_driver(self):
        """AC792N's K1 is the first slot of the SDK ADKEY ladder.

        SDK ADKEY V0 is midpoint(0, V1=192), so slot 0 covers raw values
        0..96 - the calibration header must keep K1 there, and the staged
        entry must translate the driver's KEY_K1 sys events into Tuya TDL
        button codes for the AI chat chain.
        """
        ladder = (ROOT / "boards/JIELI/AC792N_Develop_Board/adkey_button_config.h").read_text(
            encoding="utf-8"
        )
        self.assertRegex(ladder, r"#define\s+JIELI_K1_ADC_CHANNEL\s+0U")
        self.assertRegex(ladder, r"#define\s+JIELI_K1_PRESSED_MIN\s+0U")
        self.assertRegex(ladder, r"#define\s+JIELI_K1_PRESSED_MAX\s+96U")

        entry = (ROOT / "platform/JIELI/tuyaos/entry/jieli_app_entry.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("KEY_K1", entry)
        self.assertIn("ai_chat_jieli_key_event", entry)
        self.assertIn("TDL_BUTTON_LONG_PRESS_START", entry)
        self.assertLess(
            entry.index("register_sys_event_handler(SYS_KEY_EVENT"),
            entry.index("tuya_app_main();"),
        )

    def test_wl83_audio_source_is_available_in_staged_sdk_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            sdk = root / "vendor-sdk"
            for name in (
                "audio/log_config/lib_media_config.c",
                "apps/wifi_camera/board/wl83/sdk_config.h",
                "apps/wifi_camera/board/wl83/jlstream_node_cfg.h",
            ):
                path = sdk / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("/* vendor media config */", encoding="utf-8")
            staged = root / "staged-sdk"
            staged.mkdir()
            (staged / "apps").mkdir()

            sdk_overlay.stage_audio_sdk_sources(staged, sdk, "wl83")

            # The staged build resolves the profile's vendor audio inputs
            # relative to its own root, so the linked trees must surface there.
            self.assertTrue((staged / "audio/log_config/lib_media_config.c").is_file())
            self.assertTrue((staged / "apps/wifi_camera/board/wl83/sdk_config.h").is_file())
            self.assertTrue((staged / "apps/wifi_camera/board/wl83/jlstream_node_cfg.h").is_file())

        # A vendor checkout missing the configuration files must be rejected
        # before staging links anything.
        with tempfile.TemporaryDirectory() as temp:
            broken_sdk = pathlib.Path(temp) / "vendor-sdk"
            broken_sdk.mkdir()
            staged_broken = pathlib.Path(temp) / "staged-sdk"
            staged_broken.mkdir()
            with self.assertRaises(jieli_build.BuildError):
                sdk_overlay.stage_audio_sdk_sources(staged_broken, broken_sdk, "wl83")

    def test_full_stack_audio_profile_injects_runtime_sources_tasks_and_sdk_libs(self):
        expected = {
            "wl82": {
                "sources": ("apps/common/audio_music/audio_config.c",),
                "libs": ("cpu/wl82/liba/audio_server.a", "cpu/wl82/liba/media_app.a"),
                "includes": ("include_lib/media",),
                "flags": (),
                "tasks": ("audio_server", "audio_mix", "audio_encoder"),
            },
            "wl83": {
                "sources": (
                    "audio/log_config/lib_media_config.c",
                    "audio/common/audio_general.c",
                    "audio/common/audio_dvol.c",
                    "audio/cpu/wl83/audio_setup.c",
                    "audio/framework/plugs/source/adc_file.c",
                    "audio/framework/plugs/source/multi_ch_adc_file.c",
                    "audio/interface/player/a2dp_player.c",
                ),
                "libs": ("cpu/wl83/liba/media.a", "cpu/wl83/liba/stream_media_server.a"),
                "includes": (
                    "include_lib/media/cpu/wl83",
                    "include_lib/media/cvp",
                    "audio/common",
                    "audio/effect/spatial_effect",
                    "apps/wifi_camera/include",
                ),
                "flags": (
                    "objs/audio/log_config/lib_media_config.c.o: CFLAGS +=",
                    "objs/audio/cpu/wl83/audio_setup.c.o: CFLAGS +=",
                    "apps/wifi_camera/board/wl83/sdk_config.h",
                    "apps/wifi_camera/board/wl83/jlstream_node_cfg.h",
                    "audio/cpu/wl83/audio_config_def.h",
                    "apps/demo/demo_hello/board/wl83/tuya_board_audio_config.h",
                ),
                "tasks": ("audio_server", "audio_mix", "audio_encoder", "tuya_audio_capture"),
            },
        }
        for chip, needles in expected.items():
            with self.subTest(chip=chip), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp)
                makefile = root / "Makefile"
                makefile.write_text(
                    "c_SRC_FILES := app.c\n"
                    "INCLUDES :=\n"
                    "LFLAGS += --start-group\n"
                    "    ../../../../../cpu/wl83/liba/cpu.a \\\n"
                    "    --end-group\n"
                    "c_OBJS    :=\n",
                    encoding="utf-8",
                )
                app_main = root / "jieli_app_entry.c"
                app_main.write_text(
                    "const struct task_info task_info_table[] = {\n"
                    "    { 0, 0, 0, 0 },\n"
                    "};\n",
                    encoding="utf-8",
                )

                audio_profile.apply_audio_profile(makefile, app_main, chip)

                staged_makefile = makefile.read_text(encoding="utf-8")
                staged_app = app_main.read_text(encoding="utf-8")
                for needle in needles["sources"] + needles["libs"]:
                    self.assertIn(needle, staged_makefile)
                for include in needles["includes"]:
                    self.assertIn(f"-I../../../../../{include}", staged_makefile)
                for flag in needles["flags"]:
                    self.assertIn(flag, staged_makefile)
                for task in needles["tasks"]:
                    self.assertIn(f'"{task}"', staged_app)
                if chip == "wl83":
                    # Must stay in sync with WL83_CAPTURE_TASK_STACK/PRIORITY
                    # in tuyaos_adapter/src/driver/tkl_audio.c.
                    self.assertIn('{"tuya_audio_capture", 12, 768, 64}', staged_app)

    def test_wl83_board_audio_profile_does_not_register_an_audio_dev_ops_device(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            board = root / "board.c"
            board.write_text(
                '#include "asm/includes.h"\n'
                'REGISTER_DEVICES(device_table) = {\n};\n'
                'void board_early_init(void) {\n    devices_init();\n}\n'
                'void board_init(void) {\n}\n',
                encoding="utf-8",
            )

            board_config.configure_audio_board(
                board,
                ROOT / "boards/JIELI/AC792N_Develop_Board/audio_config.h",
                audio_profile.AUDIO_PROFILES["wl83"],
            )

            staged_board = board.read_text(encoding="utf-8")
            # wl83's adapter drives the ADC/DAC directly and opens no vendor
            # audio_server device, so the device table must stay free of
            # audio_dev_ops (unlike wl82, which registers one).
            self.assertNotIn("audio_dev_ops", staged_board)
            # The PA mute / VCM bring-up sequencing consumes the board's
            # JIELI_AUDIO_* macros and runs before and after devices_init().
            self.assertIn("gpio_direction_output(JIELI_AUDIO_PA_MUTE_PORT", staged_board)
            self.assertIn("dac_early_init(", staged_board)
            self.assertIn("devices_init();", staged_board)
            self.assertIn("JIELI_AUDIO_PA_RELEASE_DELAY_MS", staged_board)
            self.assertIn('#include "tuya_board_audio_config.h"', staged_board)
            staged_header = root / "tuya_board_audio_config.h"
            self.assertTrue(staged_header.is_file())
            self.assertEqual(
                staged_header.read_bytes(),
                (ROOT / "boards/JIELI/AC792N_Develop_Board/audio_config.h").read_bytes(),
            )

    def test_full_stack_audio_provider_is_registered_before_tkl_init(self):
        entry = (ROOT / "platform/JIELI/tuyaos/entry/jieli_app_entry.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("tkl_jieli_audio_prepare()", entry)
        self.assertLess(entry.index("tkl_jieli_audio_prepare()"), entry.index("tkl_init()"))

        tkl_source = _tkl_audio_source()

        # The per-chip provider files were folded into tkl_audio.c; the adapter
        # source manifest is what puts the audio adapter on both builds.
        adapter_root = ROOT / "platform" / "JIELI"
        for chip in ("wl82", "wl83"):
            sources = sdk_overlay.read_adapter_sources(adapter_root, chip)
            for needle in (
                "src/driver/tkl_audio.c",
                "src/driver/tkl_vad.c",
                "src/driver/tkl_adc.c",
                "src/driver/tkl_kws.c",
            ):
                self.assertIn(needle, sources)

        # The TKL layer calls the SoC provider directly; the backend ops
        # vtable (TKL_JIELI_AUDIO_BACKEND_OPS_T / tkl_jieli_audio_backend_install)
        # is gone.
        self.assertIn("OPERATE_RET tkl_jieli_audio_prepare(void)", tkl_source)
        self.assertIn("jieli_audio_native_prepare() == 0 ? OPRT_OK : OPRT_COM_ERROR", tkl_source)
        self.assertNotIn("TKL_JIELI_AUDIO_BACKEND_OPS_T", tkl_source)
        self.assertNotIn("tkl_jieli_audio_backend_install", tkl_source)
        self.assertIn("__map_native_result(jieli_audio_native_ao_write(", tkl_source)

        # wl82 (AC791) provider: the SDK audio_server backend.
        wl82_source = _tkl_chip_region(tkl_source, "WL82")
        self.assertIn("JIELI_AUDIO_PLAY_QUEUE_SIZE", wl82_source)
        self.assertIn("return -31;", wl82_source)
        self.assertIn("AUDIO_ENC_STOP", wl82_source)
        self.assertIn("AUDIO_DEC_STOP", wl82_source)
        self.assertIn("JIELI_AUDIO_CAPTURE_SESSION_COUNT 128u", wl82_source)
        failed_open = wl82_source.split(
            "ret = server_request(s_capture.server, AUDIO_REQ_ENC, &req);", 1
        )[1]
        failed_open = failed_open.split("return -1;", 1)[0]
        self.assertIn("session->enabled = 0", failed_open)
        self.assertIn("session->callback = NULL", failed_open)
        self.assertNotIn("session->allocated = 0", failed_open)

        # wl83 (AC792) provider: direct ADC/DAC, no audio_server.
        wl83_source = _tkl_chip_region(tkl_source, "WL83")
        self.assertIn("audio_adc_mic_start", wl83_source)
        self.assertIn("audio_dac_write", wl83_source)
        self.assertNotIn('server_open("audio_server"', wl83_source)

    def test_audio_build_rejects_unknown_chip(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            makefile = root / "Makefile"
            app_main = root / "main.c"
            makefile.write_text("c_OBJS    :=\n", encoding="utf-8")
            app_main.write_text("void x(void);\n", encoding="utf-8")
            with self.assertRaises(jieli_build.BuildError):
                audio_profile.apply_audio_profile(makefile, app_main, "wl99")


if __name__ == "__main__":
    unittest.main()
