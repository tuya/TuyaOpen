import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
APP = ROOT / "platform" / "JIELI" / "tuyaos_app_main.c"
EXAMPLE = ROOT / "examples" / "get-started" / "jieli_uart_hello" / "src" / "example_jieli_uart_hello.c"


class JieliUartHelloTest(unittest.TestCase):
    def test_app_entry_has_uart_acceptance_messages(self):
        source = APP.read_text(encoding="utf-8") + EXAMPLE.read_text(encoding="utf-8")
        self.assertIn("TuyaOpen Jieli wl82", source)
        self.assertIn("UART Hello World", source)
        self.assertIn("tkl_log_output", source)
        self.assertIn("tkl_init", source)

    def test_adapter_sources_are_present(self):
        adapter = ROOT / "platform" / "JIELI" / "tuyaos" / "tuyaos_adapter"
        self.assertTrue((adapter / "src/tkl_output.c").is_file())
        self.assertTrue((adapter / "src/tkl_uart.c").is_file())
        self.assertTrue((adapter / "src/tkl_system.c").is_file())
        self.assertTrue((adapter / "CMakeLists.txt").is_file())


if __name__ == "__main__":
    unittest.main()
