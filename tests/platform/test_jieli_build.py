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
        # TOOL_DIR is interpolated into vendor Makefile recipes, which make may
        # run through sh when one is on PATH; a Windows path with backslashes
        # gets mangled there, so the value must be shell-neutral.
        tool_dir_arg = next(arg for arg in command if arg.startswith("TOOL_DIR="))
        self.assertEqual(tool_dir_arg, "TOOL_DIR=/toolchain/bin")
        self.assertNotIn("\\", tool_dir_arg)
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
            demo_root = demo.parents[1]
            (demo_root / "app_main.c").write_text("void app_main(void) {}")
            app_include = demo_root / "include"
            app_include.mkdir()
            (app_include / "app_config.h").write_text(
                "#define __FLASH_SIZE__ 0\n#define __SDRAM_SIZE__ 0\n#endif\n"
            )
            (demo / "board.c").write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "    .port = PORT_REMAP,\n"
                "    .tx_pin = IO_PORTB_03,\n"
                "UART2_PLATFORM_DATA_END();\n"
                "void debug_uart_init() { uart_init(&uart2_data); }\n"
                "REGISTER_DEVICES(device_table) = {\n};\n"
                "void board_init(void)\n{\n}\n"
            )
            (sdk / "apps/common").mkdir(parents=True)
            (sdk / "cpu/wl82").mkdir(parents=True)
            (sdk / "include_lib").mkdir()
            (sdk / "lib").mkdir()
            (sdk / "tools").mkdir()
            tuyaopen = root / "tuyaopen"
            jieli_platform = tuyaopen / "platform/JIELI"
            jieli_platform.mkdir(parents=True)
            entry = jieli_platform / "tuyaos/entry"
            entry.mkdir(parents=True)
            (entry / "jieli_app_entry.c").write_text("void app_main(void) {}")
            adapter = tuyaopen / "platform/JIELI/tuyaos/tuyaos_adapter/src"
            (adapter / "system").mkdir(parents=True)
            (adapter / "driver").mkdir(parents=True)
            (adapter / "system/tkl_output.c").write_text("void output(void) {}")
            (adapter / "driver/tkl_wifi.c").write_text("void wifi(void) {}")
            adapter_root = adapter.parent
            (adapter_root / "include/system").mkdir(parents=True)
            (adapter_root / "adapter_sources.txt").write_text(
                "src/system/tkl_output.c\nsrc/driver/tkl_wifi.c\n"
            )
            staging = root / "staging"

            jieli_build.create_staging_tree(
                sdk,
                staging,
                tuyaopen,
                tuya_lib_dir=root / "tuya_lib",
                uart_log_port=1,
                platform_root=jieli_platform,
            )

            staged_makefile = staging / "build/apps/demo/demo_hello/board/wl82/Makefile"
            content = staged_makefile.read_text()
            self.assertIn("../../../../../tuyaos/entry/jieli_app_entry.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/system/tkl_output.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_wifi.c", content)
            self.assertIn("-I../../../../../tuyaos_adapter/include/system", content)
            self.assertIn("--start-group", content)
            self.assertIn((root / "tuya_lib" / "libtuyaapp.a").as_posix(), content)

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
        self.assertIn("UART0_PLATFORM_DATA_BEGIN(uart0_data)", content)
        self.assertIn(".port = PORTA_5_6", content)
        self.assertIn(".tx_pin = IO_PORTA_05", content)
        self.assertIn(".rx_pin = IO_PORTA_06", content)
        self.assertIn('{"uart2", &uart_dev_ops, (void *)&uart2_data }', content)

if __name__ == "__main__":
    unittest.main()
