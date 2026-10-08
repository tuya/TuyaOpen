import pathlib
import unittest

import yaml


ROOT = pathlib.Path(__file__).resolve().parents[2]


class JieliSdkConfigTest(unittest.TestCase):
    def test_sdk_metadata_describes_ac79_full_stack_inputs(self):
        path = ROOT / "platform/JIELI/sdk_config.yaml"
        self.assertTrue(path.is_file())
        config = yaml.safe_load(path.read_text(encoding="utf-8"))

        self.assertEqual(config["vendor_board"], "wl82")
        self.assertIn("JIELI_SDK_ROOT", config["sdk_root_env"])
        self.assertIn("JIELI_TOOL_DIR", config["toolchain_env"])
        self.assertIn("CONFIG_FREE_RTOS_ENABLE", config["defines"])
        self.assertIn("include_lib/newlib/include", config["include_dirs"])
        self.assertIn("cpu/wl82/liba/wl_wifi_sta.a", config["stack_libraries"])
        self.assertIn("cpu/wl82/liba/btstack.a", config["stack_libraries"])
        self.assertIn("cpu/wl82/liba/btctrler.a", config["stack_libraries"])

        required_tools = set(config["required_tools"])
        self.assertTrue({"clang", "lto-wrapper", "lto-ar", "objdump"} <= required_tools)


if __name__ == "__main__":
    unittest.main()
