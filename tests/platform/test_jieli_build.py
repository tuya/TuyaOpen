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
        self.assertIn(f"TOOL_DIR={pathlib.Path('/toolchain/bin')}", command)
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

if __name__ == "__main__":
    unittest.main()
