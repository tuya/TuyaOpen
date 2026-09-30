import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


class JieliConfigTest(unittest.TestCase):
    def test_switch_demo_disables_generic_nimble_for_jieli_btstack(self):
        app_config = (ROOT / "apps/tuya_cloud/switch_demo/app_default.config").read_text()
        self.assertIn("# CONFIG_ENABLE_NIMBLE is not set", app_config)
        self.assertNotIn("CONFIG_ENABLE_NIMBLE=y", app_config)

    def test_platform_registry_contains_jieli(self):
        config = (ROOT / "platform" / "platform_config.yaml").read_text()
        self.assertIn("name: JIELI", config)
        self.assertIn("repo: https://github.com/tuya/TuyaOpen-JieLi", config)
        self.assertIn("branch: master", config)
        # The registry entry must point at the exact commit the platform is
        # pinned to; a drifted registry entry would silently build against a
        # different platform tree than the one checked out on disk.
        self.assertIn("commit: 66418ae69c29eb04ba0bb1c9aa7c86f4c6d3cc4e", config)

    def test_board_catalog_contains_ac7916a(self):
        board_kconfig = (ROOT / "boards" / "Kconfig").read_text()
        self.assertIn("BOARD_ENABLE_JIELI", board_kconfig)
        self.assertIn('rsource "./JIELI/Kconfig"', board_kconfig)

        platform_kconfig = (ROOT / "platform" / "JIELI" / "Kconfig")
        self.assertTrue(platform_kconfig.is_file())
        platform_kconfig_text = platform_kconfig.read_text()
        self.assertIn("PLATFORM_JIELI", platform_kconfig_text)
        self.assertIn("config ENABLE_WIFI", platform_kconfig_text)
        self.assertIn("config ENABLE_BLUETOOTH", platform_kconfig_text)

        # The refactor replaced the PLATFORM_SKIP_DEFAULT_COMPONENTS escape
        # hatch with a chip-selection block: it maps the board's chip choice to
        # the JIELI_SELECTED_CHIP_WL8x define that the folded TKL adapter
        # sources (e.g. tkl_audio.c) branch on, and rejects unknown chips.
        platform_config = ROOT / "platform" / "JIELI" / "platform_config.cmake"
        platform_config_text = platform_config.read_text()
        self.assertIn("JIELI_SELECTED_CHIP_WL82", platform_config_text)
        self.assertIn("JIELI_SELECTED_CHIP_WL83", platform_config_text)
        self.assertIn("JIELI requires CONFIG_CHIP_CHOICE to be wl82 or wl83", platform_config_text)
        self.assertIn("${JIELI_ADAPTER_PATH}/include", platform_config_text)

        # The board catalog split into the AC79 DevKit (wl82) and the AC792N
        # Develop Board (wl83); AC7916A/AC7921A survive as legacy aliases.
        board_catalog = (ROOT / "boards" / "JIELI" / "Kconfig").read_text()
        self.assertIn("BOARD_CHOICE_AC7916A", board_catalog)
        self.assertIn('rsource "./AC79_DevKitBoard/Kconfig"', board_catalog)
        self.assertIn('rsource "./AC792N_Develop_Board/Kconfig"', board_catalog)

        devkit_kconfig = ROOT / "boards" / "JIELI" / "AC79_DevKitBoard" / "Kconfig"
        self.assertTrue(devkit_kconfig.is_file())
        self.assertIn("CHIP_WL82", devkit_kconfig.read_text())

        ac792n_kconfig = ROOT / "boards" / "JIELI" / "AC792N_Develop_Board" / "Kconfig"
        self.assertTrue(ac792n_kconfig.is_file())
        self.assertIn("CHIP_WL83", ac792n_kconfig.read_text())


if __name__ == "__main__":
    unittest.main()
