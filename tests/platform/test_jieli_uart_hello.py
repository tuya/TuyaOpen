import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
APP = ROOT / "platform" / "JIELI" / "tuyaos" / "entry" / "jieli_app_entry.c"
OUTPUT = ROOT / "platform" / "JIELI" / "tuyaos" / "tuyaos_adapter" / "src" / "system" / "tkl_output.c"


class JieliUartHelloTest(unittest.TestCase):
    def test_app_entry_initializes_tuya_layer_before_application(self):
        source = APP.read_text(encoding="utf-8")
        self.assertIn("(void)tkl_init();", source)
        self.assertIn("tuya_app_main();", source)

    def test_uart_log_adapter_writes_to_vendor_console(self):
        source = OUTPUT.read_text(encoding="utf-8")
        self.assertIn("void tkl_log_output", source)
        self.assertIn("vprintf(format, args);", source)

    def test_adapter_sources_are_present(self):
        adapter = ROOT / "platform" / "JIELI" / "tuyaos" / "tuyaos_adapter"
        self.assertTrue((adapter / "src/system/tkl_output.c").is_file())
        self.assertTrue((adapter / "src/driver/tkl_uart.c").is_file())
        self.assertTrue((adapter / "src/system/tkl_system.c").is_file())
        self.assertTrue((adapter / "CMakeLists.txt").is_file())


if __name__ == "__main__":
    unittest.main()
