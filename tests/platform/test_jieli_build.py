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
            example = tuyaopen / "examples/get-started/jieli_uart_hello/src"
            example.mkdir(parents=True)
            (example / "example_jieli_uart_hello.c").write_text("void hello(void) {}")
            jieli_platform = tuyaopen / "platform/JIELI"
            jieli_platform.mkdir(parents=True)
            (jieli_platform / "tuyaos_app_main.c").write_text("void app_main(void) {}")
            adapter = tuyaopen / "platform/JIELI/tuyaos/tuyaos_adapter/src"
            adapter.mkdir(parents=True)
            (adapter / "tkl_output.c").write_text("void output(void) {}")
            staging = root / "staging"

            jieli_build.create_staging_tree(sdk, staging, tuyaopen)

            staged_makefile = staging / "build/apps/demo/demo_hello/board/wl82/Makefile"
            content = staged_makefile.read_text()
            self.assertIn("../../../../../tuyaos_app_main.c", content)
            self.assertIn("../../../../../tuyaopen_uart_hello.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/system/tkl_output.c", content)
            self.assertIn("../../../../../tuyaos_adapter/src/driver/tkl_wifi.c", content)
            self.assertIn("-I../../../../../tuyaos_adapter/include/system", content)

    def test_reference_log_uart_is_applied_to_staging(self):
        with tempfile.TemporaryDirectory() as temp:
            board = pathlib.Path(temp) / "board.c"
            board.write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "    .port = PORT_REMAP,\n"
                "    .tx_pin = IO_PORTB_03,\n"
                "UART2_PLATFORM_DATA_END();\n"
            )
            jieli_build.configure_reference_log_uart(board)
            content = board.read_text()

        self.assertIn(".baudrate = 115200", content)
        self.assertIn(".port = PORTB_6_7", content)
        self.assertIn(".tx_pin = IO_PORTB_06", content)

    def test_service_uart_is_added_separately_from_log_uart(self):
        with tempfile.TemporaryDirectory() as temp:
            board = pathlib.Path(temp) / "board.c"
            board.write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "UART2_PLATFORM_DATA_END();\n"
                "REGISTER_DEVICES(device_table) = {\n"
                "    {\"uart2\", &uart_dev_ops, (void *)&uart2_data },\n"
                "};\n"
            )
            jieli_build.configure_service_uart(board)
            content = board.read_text()

        self.assertIn("UART1_PLATFORM_DATA_BEGIN(uart1_data)", content)
        self.assertIn('.port = PORTB_3_4', content)
        self.assertIn('{"uart1", &uart_dev_ops, (void *)&uart1_data }', content)

    def test_full_stack_staging_enables_dual_bank_ota(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            sdk = root / "sdk"
            board = sdk / "apps/demo/demo_hello/board/wl82"
            board.mkdir(parents=True)
            (board / "Makefile").write_text(
                "c_SRC_FILES := ../../../../../apps/demo/demo_hello/app_main.c\n"
                "c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)\n"
            )
            (board / "board.c").write_text(
                "UART2_PLATFORM_DATA_BEGIN(uart2_data)\n"
                "    .baudrate = 1000000,\n"
                "    .port = PORT_REMAP,\n"
                "    .tx_pin = IO_PORTB_03,\n"
                "UART2_PLATFORM_DATA_END();\n"
                "REGISTER_DEVICES(device_table) = {\n"
                '    {"uart2", &uart_dev_ops, (void *)&uart2_data },\n'
                "};\n"
                "/**************************  POWER config ****************************/\n"
                "void board_init()\n"
                "{\n"
                "\tboard_power_init();\n"
                "}\n"
            )
            include = sdk / "apps/demo/demo_hello/include"
            include.mkdir(parents=True)
            (include / "app_config.h").write_text("#ifndef APP_CONFIG_H\n#define APP_CONFIG_H\n#endif\n")
            (sdk / "apps/demo/demo_hello/app_main.c").write_text("void app_main(void) {}")
            (sdk / "apps/common/update").mkdir(parents=True)
            (sdk / "apps/common/update/net_update.c").write_text("void net_update(void) {}")
            for name in ("cpu", "include_lib", "lib", "tools"):
                (sdk / name).mkdir()
            tuyaopen = root / "tuyaopen"
            jieli_platform = tuyaopen / "platform/JIELI"
            jieli_platform.mkdir(parents=True)
            (jieli_platform / "tuyaos_switch_app_main.c").write_text("void app_main(void) {}")
            adapter = tuyaopen / "platform/JIELI/tuyaos/tuyaos_adapter"
            adapter.mkdir(parents=True)
            staging = root / "staging"

            jieli_build.create_staging_tree(
                sdk, staging, tuyaopen, full_stack=True, tuya_lib_dir=root / "libs"
            )

            content = (staging / "build/apps/demo/demo_hello/board/wl82/Makefile").read_text()
            self.assertIn("../../../../../apps/common/update/net_update.c", content)
            self.assertIn("-DCONFIG_DOUBLE_BANK_ENABLE=1", content)

    def test_find_ug_artifact_accepts_db_update_package(self):
        with tempfile.TemporaryDirectory() as temp:
            tools = pathlib.Path(temp)
            package = tools / "db_update_files_data.bin"
            package.write_bytes(b"ota")
            self.assertEqual(package, jieli_build.find_ug_artifact(tools))

    def test_elf_alone_is_not_a_ug_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            tools = pathlib.Path(temp)
            (tools / "sdk.elf").write_bytes(b"elf")
            with self.assertRaises(jieli_build.BuildError):
                jieli_build.find_ug_artifact(tools)

    def test_tuya_ug_name_follows_project_and_version(self):
        self.assertEqual(
            "switch_demo_UG_1.0.0.bin",
            jieli_build.tuya_ug_name(
                {"CONFIG_PROJECT_NAME": "switch_demo", "CONFIG_PROJECT_VERSION": "1.0.0"}
            ),
        )

    def test_sanitize_host_path_drops_posix_shells(self):
        # GNU make for Windows picks sh.exe as its SHELL whenever a POSIX
        # shell is on PATH (tos.py launched from Git Bash), and the vendor
        # recipes then mangle Windows paths (C:\JL\... -> C:JL...).
        raw = (
            "C:\\Program Files\\Git\\usr\\bin;C:\\Windows\\system32;"
            "C:\\Program Files\\Git\\cmd;D:\\tools\\bin"
        )
        cleaned = jieli_build.sanitize_host_path(raw)
        entries = cleaned.split(os.pathsep)
        self.assertNotIn("C:\\Program Files\\Git\\usr\\bin", entries)
        self.assertIn("C:\\Windows\\system32", entries)
        self.assertIn("C:\\Program Files\\Git\\cmd", entries)
        self.assertIn("D:\\tools\\bin", entries)

    def test_configure_tuya_reserved_area_inserts_window(self):
        # The dual-bank layout gives the second bank the whole upper half of
        # the flash ([0x1FC000, 0x3F5000)), which covers the old TuyaOpen
        # partitions at 0x300000: the flasher erased them and code protection
        # made them unwritable.  TuyaOpen data must live in a vendor reserved
        # area with OPT=1 so flashing and OTA leave it alone.
        with tempfile.TemporaryDirectory() as temp:
            ini = pathlib.Path(temp) / "isd_config.ini"
            ini.write_text(
                "[RESERVED_CONFIG]\n"
                "VM_ADR=0; [vm]\n"
                "VM_LEN=32K;\n"
                "VM_OPT=0;\n"
                "BTIF_ADR=AUTO; [bt]\n"
                "BTIF_LEN=0x1000;\n"
                "BTIF_OPT=1;\n"
                "PRCT_ADR=0;\n"
                "PRCT_LEN=CODE_LEN;\n"
                "PRCT_OPT=2;\n",
                encoding="utf-8",
            )
            changed = jieli_build.configure_tuya_reserved_area(ini)
            content = ini.read_text(encoding="utf-8")
            self.assertTrue(changed)
            self.assertIn("TUYA_ADR=0x3A0000;", content)
            self.assertIn("TUYA_LEN=0x55000;", content)
            self.assertIn("TUYA_OPT=1;", content)
            # stays inside [RESERVED_CONFIG], ahead of the PRCT block
            self.assertLess(content.index("TUYA_ADR"), content.index("PRCT_ADR"))
            # idempotent on a regenerated ini
            self.assertFalse(jieli_build.configure_tuya_reserved_area(ini))


if __name__ == "__main__":
    unittest.main()
