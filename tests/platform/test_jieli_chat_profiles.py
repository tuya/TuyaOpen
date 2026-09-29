import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
PROFILE_DIR = ROOT / "apps/tuya.ai/your_chat_bot/config"
CHAT_MAIN = ROOT / "src/ai_components/ai_main/src/ai_chat_main.c"
JIELI_APP_MAIN = ROOT / "platform/JIELI/tuyaos_switch_app_main.c"


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
                    "CONFIG_ENABLE_JIELI_NATIVE_KEY=y",
                ):
                    self.assertIn(setting, config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_ONESHOT is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_WAKEUP is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_MODE_FREE is not set", config)
                self.assertIn("# CONFIG_ENABLE_COMP_AI_DISPLAY is not set", config)
                self.assertIn("# CONFIG_ENABLE_DISPLAY is not set", config)
                self.assertIn("# CONFIG_ENABLE_LIBLVGL is not set", config)

    def test_native_k1_events_drive_ai_hold_mode(self):
        chat_main = CHAT_MAIN.read_text(encoding="utf-8")
        platform_main = JIELI_APP_MAIN.read_text(encoding="utf-8")
        self.assertIn("void ai_chat_jieli_key_event(int event)", chat_main)
        self.assertIn("__ai_button_function_cb", chat_main)
        self.assertIn("!defined(ENABLE_JIELI_NATIVE_KEY)", chat_main)
        self.assertIn("key->value != KEY_K1", platform_main)
        self.assertIn("KEY_EVENT_HOLD", platform_main)
        self.assertIn("KEY_EVENT_UP", platform_main)
        self.assertIn("register_sys_event_handler(SYS_KEY_EVENT", platform_main)


if __name__ == "__main__":
    unittest.main()
