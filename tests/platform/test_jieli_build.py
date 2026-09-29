import os
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "platform" / "JIELI"))

import jieli_build


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
            demo = sdk / "apps/demo/demo_hello/board/wl82"
            demo.mkdir(parents=True)
            (demo / "Makefile").write_text(
                "c_SRC_FILES := ../../../../../apps/demo/demo_hello/app_main.c\n"
                "INCLUDES := -I../../../../../include_lib\n"
                "c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)\n"
            )
            (demo.parent / "app_main.c").write_text("void app_main(void) {}")
            (sdk / "apps/common").mkdir(parents=True)
            (sdk / "cpu").mkdir()
            (sdk / "include_lib").mkdir()
            (sdk / "lib").mkdir()
            tuyaopen = root / "tuyaopen"
            jieli_platform = tuyaopen / "platform/JIELI"
            jieli_platform.mkdir(parents=True)
            (jieli_platform / "tuyaos_app_main.c").write_text("void app_main(void) {}")
            adapter = tuyaopen / "platform/JIELI/tuyaos/tuyaos_adapter/src"
            adapter.mkdir(parents=True)
            (adapter / "tkl_output.c").write_text("void output(void) {}")
            staging = root / "staging"

            jieli_build.create_staging_tree(sdk, staging, tuyaopen, uart_log_port=1)

            staged_makefile = staging / "build/apps/demo/demo_hello/board/wl82/Makefile"
            content = staged_makefile.read_text()
            self.assertIn("../../../../../tuyaos_app_main.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/system/tkl_output.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_wifi.c", content)
            self.assertIn("-I../../../../../tuyaos_adapter/include/system", content)

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

    def test_service_uart_uses_uart2_away_from_pb3_log_uart(self):
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

        self.assertIn(".baudrate = 115200", content)
        self.assertIn(".port = PORTB_6_7", content)
        self.assertIn(".tx_pin = IO_PORTB_06", content)
        self.assertIn(".rx_pin = IO_PORTB_07", content)
        self.assertIn('{"uart2", &uart_dev_ops, (void *)&uart2_data }', content)

    def test_jieli_board_exposes_media_gate_for_tdd_audio(self):
        kconfig = (ROOT / "boards/JIELI/Kconfig").read_text(encoding="utf-8")
        self.assertRegex(kconfig, r"config ENABLE_MEDIA\s+bool[^\n]*\n\s+default n")

    def test_ac791_staging_configures_the_devkit_k1_adc_ladder(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            board = root / "board.c"
            app_config = root / "app_config.h"
            board.write_text(
                '#include "asm/includes.h"\n'
                "void board_init()\n{\n\tboard_power_init();\n}\n",
                encoding="utf-8",
            )
            app_config.write_text("#ifndef APP_CONFIG_H\n#define APP_CONFIG_H\n#endif\n", encoding="utf-8")

            jieli_build.configure_full_stack_k1_key(board, app_config, "wl82")
            jieli_build.configure_full_stack_app_config(app_config)

            board_source = board.read_text(encoding="utf-8")
            self.assertIn("IO_PORTB_01", board_source)
            self.assertIn(".ad_channel = 3", board_source)
            self.assertIn("KEY_K1", board_source)
            self.assertIn("TUYAOPEN_JIELI_ADC_INIT", board_source)
            self.assertIn("adc_init();", board_source)
            self.assertIn("key_driver_init();", board_source)
            self.assertLess(board_source.index("adc_init();"), board_source.index("key_driver_init();"))
            self.assertIn("#define CONFIG_KEY_ENABLE", app_config.read_text(encoding="utf-8"))

    def test_ac792_staging_uses_k1_slot_of_the_native_adkey_driver(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            board_dir = root / "board/wl83"
            board_dir.mkdir(parents=True)
            board = board_dir / "board.c"
            board.write_text("void board_init(void) {}\n", encoding="utf-8")
            (board_dir / "board_demo.h").write_text(
                "#define TCFG_ADKEY_VALUE_0                   KEY_POWER\n",
                encoding="utf-8",
            )

            jieli_build.configure_full_stack_k1_key(board, root / "app_config.h", "wl83")

            self.assertIn("KEY_K1", (board_dir / "board_demo.h").read_text(encoding="utf-8"))

    def test_wl83_audio_source_is_available_in_staged_sdk_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            sdk = root / "vendor-sdk"
            staged = root / "staged-sdk"
            source = sdk / "audio/log_config/lib_media_config.c"
            source.parent.mkdir(parents=True)
            source.write_text("/* vendor media config */", encoding="utf-8")
            (sdk / "apps/wifi_camera/board/wl83").mkdir(parents=True)
            for name in ("sdk_config.h", "jlstream_node_cfg.h"):
                (sdk / "apps/wifi_camera/board/wl83" / name).write_text(
                    "/* vendor board config */", encoding="utf-8"
                )
            (staged / "apps").mkdir(parents=True)

            jieli_build.stage_full_stack_audio_sdk_sources(staged, sdk, "wl83")

            makefile_cwd = staged / "apps/demo/demo_hello/board/wl83"
            injected_source = makefile_cwd / "../../../../../audio/log_config/lib_media_config.c"
            self.assertTrue(injected_source.resolve().is_file())
            self.assertTrue(
                (staged / "apps/wifi_camera/board/wl83/sdk_config.h").is_file()
            )

    def test_full_stack_audio_build_injects_runtime_sources_tasks_and_sdk_libs(self):
        expected = {
            "wl82": ("tkl_audio_jieli_server.c", "tkl_audio_jieli_server_native.c", "audio_config.c", "audio_server.a", "media_app.a"),
            "wl83": ("tkl_audio_jieli_server.c", "tkl_audio_jieli_wl83_native.c", "lib_media_config.c", "audio_general.c", "audio_setup.c", "adc_file.c", "multi_ch_adc_file.c", "media.a", "stream_media_server.a", "fs.a"),
        }
        for chip, needles in expected.items():
            with self.subTest(chip=chip), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp)
                makefile = root / "Makefile"
                makefile.write_text(
                    "c_SRC_FILES := app.c\n"
                    "INCLUDES :=\n"
                    "LFLAGS += --start-group\n"
                    "    ../../../../../cpu/wl83/liba/fs.a \\\n"
                    "    --end-group\n"
                    "LFLAGS += --start-group\n    --end-group\n"
                    "c_OBJS    :=\n",
                    encoding="utf-8",
                )
                app_main = root / "tuyaos_switch_app_main.c"
                app_main.write_text(
                    "const struct task_info task_info_table[] = {\n"
                    "    { 0, 0, 0, 0 },\n"
                    "};\n",
                    encoding="utf-8",
                )

                jieli_build.configure_full_stack_audio_build(makefile, app_main, chip, ROOT)

                staged_makefile = makefile.read_text(encoding="utf-8")
                staged_app = app_main.read_text(encoding="utf-8")
                for needle in needles:
                    self.assertIn(needle, staged_makefile)
                self.assertIn("tkl_audio.c", staged_makefile)
                self.assertIn("tkl_vad.c", staged_makefile)
                self.assertIn("tkl_audio_jieli_server.c", staged_makefile)
                if chip == "wl83":
                    self.assertEqual(staged_makefile.count("/liba/fs.a"), 2)
                    self.assertIn("audio/common/audio_general.c", staged_makefile)
                    self.assertNotIn("tkl_audio_jieli_server_native.c", staged_makefile)
                    self.assertIn("audio/framework/plugs/source/adc_file.c", staged_makefile)
                    self.assertIn("audio/framework/plugs/source/multi_ch_adc_file.c", staged_makefile)
                    self.assertIn("audio/cpu/wl83/audio_setup.c", staged_makefile)
                    self.assertIn("objs/audio/cpu/wl83/audio_setup.c.o: CFLAGS +=", staged_makefile)
                else:
                    self.assertIn("tkl_audio_jieli_server_native.c", staged_makefile)
                self.assertIn("tools/porting/adapter/media", staged_makefile)
                self.assertIn("tools/porting/adapter/vad", staged_makefile)
                self.assertIn("include_lib/media", staged_makefile)
                if chip == "wl83":
                    self.assertIn("include_lib/media/cpu/wl83", staged_makefile)
                    self.assertIn("include_lib/media/cpu/wl83/asm", staged_makefile)
                    self.assertIn("include_lib/media/framework/include", staged_makefile)
                    self.assertIn("include_lib/media/cvp", staged_makefile)
                    self.assertIn("apps/wifi_camera/include", staged_makefile)
                    self.assertIn("apps/wifi_camera/board/wl83", staged_makefile)
                    self.assertIn("apps/demo/demo_hello/board/wl83/tuya_board_audio_config.h", staged_makefile)
                    self.assertIn("audio/cpu/wl83", staged_makefile)
                    self.assertIn("audio/common", staged_makefile)
                    self.assertIn("audio/effect/spatial_effect", staged_makefile)
                    config_flags = "objs/audio/log_config/lib_media_config.c.o: CFLAGS +="
                    self.assertIn(config_flags, staged_makefile)
                    self.assertIn("apps/wifi_camera/board/wl83/sdk_config.h", staged_makefile)
                    self.assertIn("apps/wifi_camera/board/wl83/jlstream_node_cfg.h", staged_makefile)
                    self.assertIn("audio/cpu/wl83/audio_config_def.h", staged_makefile)
                    self.assertIn(
                        "objs/audio/common/audio_general.c.o: CFLAGS +=",
                        staged_makefile,
                    )
                tasks = ("audio_server", "audio_mix", "audio_encoder")
                if chip == "wl83":
                    tasks += ("tuya_audio_capture",)
                for task in tasks:
                    self.assertIn(f'"{task}"', staged_app)

    def test_wl83_board_audio_profile_does_not_reference_missing_audio_dev_ops(self):
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
            profile = root / "audio_config.h"
            profile.write_text("#define JIELI_AUDIO_TEST 1\n", encoding="utf-8")

            jieli_build.configure_full_stack_audio_board(board, profile, "wl83")

            staged_board = board.read_text(encoding="utf-8")
            self.assertNotIn("audio_dev_ops", staged_board)
            self.assertIn("JIELI_AUDIO_PA_RELEASE_DELAY_MS", staged_board)
            self.assertIn('#include "tuya_board_audio_config.h"', staged_board)
            self.assertTrue((root / "tuya_board_audio_config.h").is_file())

    def test_full_stack_audio_provider_is_registered_before_tkl_init(self):
        provider = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_server.c"
        wl82_native_provider = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_server_native.c"
        wl83_native_provider = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_wl83_native.c"
        app_main = (ROOT / "platform/JIELI/tuyaos_switch_app_main.c").read_text(encoding="utf-8")
        self.assertTrue(provider.is_file(), "full-stack audio provider source is missing")
        self.assertTrue(wl82_native_provider.is_file(), "WL82 audio_server provider source is missing")
        self.assertTrue(wl83_native_provider.is_file(), "WL83 ADC/DAC provider source is missing")
        self.assertIn("tkl_jieli_audio_server_install()", app_main)
        self.assertLess(app_main.index("tkl_jieli_audio_server_install()"), app_main.index("tkl_init()"))
        provider_text = provider.read_text(encoding="utf-8")
        wl82_text = wl82_native_provider.read_text(encoding="utf-8")
        wl83_text = wl83_native_provider.read_text(encoding="utf-8")
        self.assertIn("JIELI_AUDIO_PLAY_QUEUE_SIZE", wl82_text)
        self.assertIn("-31", wl82_text)
        self.assertIn("AUDIO_ENC_STOP", wl82_text)
        self.assertIn("AUDIO_DEC_STOP", wl82_text)
        self.assertIn("JIELI_AUDIO_CAPTURE_SESSION_COUNT 128u", wl82_text)
        failed_open = wl82_text.split("ret = server_request(s_capture.server, AUDIO_REQ_ENC, &req);", 1)[1]
        failed_open = failed_open.split("return -1;", 1)[0]
        self.assertIn("session->enabled = 0", failed_open)
        self.assertIn("session->callback = NULL", failed_open)
        self.assertNotIn("session->allocated = 0", failed_open)
        self.assertIn("tkl_jieli_audio_backend_install", provider_text)
        self.assertIn("audio_adc_mic_start", wl83_text)
        self.assertIn("audio_dac_write", wl83_text)
        self.assertNotIn('server_open("audio_server"', wl83_text)

    def test_audio_build_rejects_unknown_chip(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            makefile = root / "Makefile"
            app_main = root / "main.c"
            makefile.write_text("c_OBJS :=\n", encoding="utf-8")
            app_main.write_text("void x(void);\n", encoding="utf-8")
            with self.assertRaises(jieli_build.BuildError):
                jieli_build.configure_full_stack_audio_build(makefile, app_main, "wl99", ROOT)


if __name__ == "__main__":
    unittest.main()
