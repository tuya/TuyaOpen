import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
PROFILE_DIR = ROOT / "examples/peripherals/audio_codecs/output_speaker/config"
EXAMPLE = ROOT / "examples/peripherals/audio_codecs/output_speaker/src/example_output_speaker.c"
PLATFORM_MAIN = ROOT / "platform/JIELI/tuyaos_switch_app_main.c"
TKL_ASSERT = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/system/tkl_assert.c"


class JieliOutputSpeakerProfileTest(unittest.TestCase):
    def test_profiles_enable_jieli_audio_and_tuya_adc_k1(self):
        for name, board in (
            ("JIELI_AC79_DevKitBoard.config", "CONFIG_BOARD_CHOICE_AC79_DEVKITBOARD=y"),
            ("JIELI_AC792N_Develop_Board.config", "CONFIG_BOARD_CHOICE_AC792N_DEVELOP_BOARD=y"),
        ):
            with self.subTest(profile=name):
                config = (PROFILE_DIR / name).read_text(encoding="utf-8")
                for setting in (
                    "CONFIG_BOARD_CHOICE_JIELI=y",
                    board,
                    "CONFIG_ENABLE_MEDIA=y",
                    "CONFIG_ENABLE_JIELI_ADKEY_BUTTON=y",
                ):
                    self.assertIn(setting, config)
                self.assertNotIn("CONFIG_ENABLE_JIELI_NATIVE_KEY=y", config)

    def test_example_uses_tuya_button_events_and_no_native_key_path(self):
        source = EXAMPLE.read_text(encoding="utf-8")
        self.assertIn("tdl_button_create(BUTTON_NAME", source)
        self.assertIn("TDL_BUTTON_PRESS_DOWN", source)
        self.assertIn("TDL_BUTTON_PRESS_UP", source)
        self.assertNotIn("register_sys_event_handler(SYS_KEY_EVENT", source)
        self.assertNotIn('#include "event/key_event.h"', source)
        self.assertNotIn('#include "system/includes.h"', source)

    def test_playback_failure_is_reported(self):
        source = EXAMPLE.read_text(encoding="utf-8")
        self.assertIn("tdl_audio_play(sg_audio_hdl, frame_buf, out_len)", source)
        self.assertIn("playback failed", source)

    def test_full_stack_entry_supports_apps_without_ai_components(self):
        source = PLATFORM_MAIN.read_text(encoding="utf-8")
        self.assertRegex(source, r"ai_chat_jieli_key_event\(int event\).*weak")
        self.assertIn("if (ai_chat_jieli_key_event == NULL) return;", source)

    def test_jieli_assert_adapter_does_not_duplicate_sdk_random32(self):
        source = TKL_ASSERT.read_text(encoding="utf-8")
        self.assertNotIn("unsigned int random32(", source)


if __name__ == "__main__":
    unittest.main()
