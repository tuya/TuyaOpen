import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
PROFILE_DIR = ROOT / "apps/tuya.ai/your_chat_bot/config"
CHAT_MAIN = ROOT / "src/ai_components/ai_main/src/ai_chat_main.c"
JIELI_APP_MAIN = ROOT / "platform/JIELI/tuyaos/entry/jieli_app_entry.c"


class JieliChatProfileTest(unittest.TestCase):
    def test_profiles_select_native_audio_k1_and_hold_only(self):
        for name, board in (
            ("JIELI_AC79_DevKitBoard.config", "CONFIG_BOARD_CHOICE_AC79_DEVKITBOARD=y"),
            ("JIELI_AC792N_Develop_Board.config", "CONFIG_BOARD_CHOICE_AC792N_DEVELOP_BOARD=y"),
        ):
            with self.subTest(profile=name):
                config = (PROFILE_DIR / name).read_text(encoding="utf-8")
                for setting in (
                    board,
                    "CONFIG_ENABLE_MEDIA=y",
                    "CONFIG_ENABLE_AUDIO_CODECS=y",
                    "CONFIG_ENABLE_AI_COMPONENTS=y",
                    "CONFIG_ENABLE_COMP_AI_MODE_HOLD=y",
                    "CONFIG_ENABLE_JIELI_ADKEY_BUTTON=y",
                ):
                    self.assertIn(setting, config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_ONESHOT is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_WAKEUP is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_FREE is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_DISPLAY is not set", config)
                self.assertIn("# CONFIG_ENABLE_DISPLAY is not set", config)
                self.assertIn("# CONFIG_ENABLE_LIBLVGL is not set", config)

    def test_tdl_button_events_drive_ai_hold_mode(self):
        chat_main = CHAT_MAIN.read_text(encoding="utf-8")
        platform_main = JIELI_APP_MAIN.read_text(encoding="utf-8")
        # The board is the only key path: it registers K1 through
        # tdd_adc_button_register and the app consumes it through TDL. The app
        # must open its TDL button unconditionally under ENABLE_BUTTON -- no
        # native-key opt-out may return -- and the platform entry must carry
        # neither a vendor key handler nor any app symbol.
        self.assertNotIn("ai_chat_jieli_key_event", chat_main)
        self.assertNotIn("ENABLE_JIELI_NATIVE_KEY", chat_main)
        self.assertIn("__ai_button_function_cb", chat_main)
        self.assertIn("tdl_button_create(AI_CHAT_BUTTON_NAME", chat_main)
        self.assertRegex(
            chat_main,
            r"#if defined\(ENABLE_BUTTON\) && \(ENABLE_BUTTON == 1\)\s*"
            r"\n\s*TUYA_CALL_ERR_LOG\(__ai_chat_mode_open_button\(\)\);\s*"
            r"\n#endif",
        )
        self.assertNotIn("register_sys_event_handler", platform_main)
        self.assertNotIn("ai_chat", platform_main)
        self.assertIn("tuya_app_main();", platform_main)


if __name__ == "__main__":
    unittest.main()
