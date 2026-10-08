import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/peripherals/audio_codecs/tdd_audio/src/tdd_audio.c"
CONFIG = ROOT / "apps/tuya_cloud/switch_demo/app_default.config"
CODECS_CMAKE = ROOT / "src/peripherals/audio_codecs/CMakeLists.txt"
AI_PLAYER = ROOT / "src/audio_player/src/svc_ai_player.c"


class JieliTddAudioContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.config = CONFIG.read_text(encoding="utf-8")
        cls.codecs_cmake = CODECS_CMAKE.read_text(encoding="utf-8")

    def section(self, start, end):
        begin = self.source.index(start)
        return self.source[begin:self.source.index(end, begin)]

    def test_open_initializes_capture_and_playback_with_one_config(self):
        open_source = self.section("static OPERATE_RET __tdd_audio_open", "static OPERATE_RET __tdd_audio_play")
        self.assertIn("tkl_ai_init(&config, 1)", open_source)
        self.assertIn("__tdd_audio_start_output(hdl)", open_source)
        self.assertLess(open_source.index("__tdd_audio_start_output"),
                        open_source.index("tkl_ai_start(0, TKL_AI_0)"))
        output_source = self.section("static OPERATE_RET __tdd_audio_start_output", "static int __tkl_audio_frame_put")
        self.assertIn("tkl_ao_init(&hdl->ao_config, 1, &hdl->ao_handle)", output_source)
        self.assertIn("tkl_ao_start(0, TKL_AO_0, hdl->ao_handle)", output_source)
        self.assertLess(output_source.index("tkl_ao_set_vol(0, TKL_AO_0"),
                        output_source.index("tkl_ao_start(0, TKL_AO_0"))

    def test_open_logs_failure_stage_and_preserves_original_error(self):
        open_source = self.section("static OPERATE_RET __tdd_audio_open", "static OPERATE_RET __tdd_audio_play")
        output_source = self.section("static OPERATE_RET __tdd_audio_start_output", "static int __tkl_audio_frame_put")
        for message in ("tkl_ai_init failed", "speaker init/start failed", "tkl_ai_start failed"):
            self.assertIn(message, open_source)
        for message in ("tkl_ao_init failed", "tkl_ao_start failed", "tkl_ao_set_vol failed"):
            self.assertIn(message, output_source)
        error_cleanup = open_source[open_source.index("__error:"):]
        self.assertIn("return rt;", error_cleanup)
        self.assertNotIn("return -1;", error_cleanup)

    def test_play_restarts_output_after_play_stop(self):
        play_source = self.section("static OPERATE_RET __tdd_audio_play", "static OPERATE_RET __tdd_audio_set_volume")
        self.assertIn("__tdd_audio_start_output(hdl)", play_source)
        self.assertLess(play_source.index("__tdd_audio_start_output"), play_source.index("tkl_ao_put_frame"))
        self.assertIn("frame.buf_size = len", play_source)

    def test_play_retries_a_full_output_queue_without_dropping_the_frame(self):
        play_source = self.section("static OPERATE_RET __tdd_audio_play", "static OPERATE_RET __tdd_audio_set_volume")
        for setting in (
            "OPRT_BUFFER_NOT_ENOUGH",
            "JIELI_AUDIO_PLAY_RETRY_LIMIT",
            "JIELI_AUDIO_PLAY_RETRY_DELAY_MS",
            "tal_system_sleep",
            "OPRT_TIMEOUT",
        ):
            self.assertIn(setting, play_source)

    def test_play_stop_uses_declared_tkl_stop_api(self):
        config_source = self.section("static OPERATE_RET __tdd_audio_config", "static OPERATE_RET __tdd_audio_close")
        self.assertIn("TDD_AUDIO_DATA_HANDLE_T *hdl = (TDD_AUDIO_DATA_HANDLE_T *)handle", config_source)
        self.assertIn("tkl_ao_stop(0, TKL_AO_0", config_source)
        jieli_stop = config_source[config_source.index("#if defined(PLATFORM_JIELI)"):config_source.index("#else")]
        self.assertNotIn("tkl_ao_clear_buffer", jieli_stop)
        self.assertLess(jieli_stop.index("rt = tkl_ao_stop"), jieli_stop.index("hdl->ao_started = FALSE"))
        self.assertNotIn("if (OPRT_OK == rt)", jieli_stop)

    def test_ai_player_propagates_output_consumer_write_errors(self):
        player_source = AI_PLAYER.read_text(encoding="utf-8")
        begin = player_source.index("// No mixing")
        end = player_source.index("static void __ai_player_thread_cb", begin)
        output_source = player_source[begin:end]
        self.assertIn("rt = s_ai_player_ctx.consumer.write", output_source)
        self.assertIn("if (OPRT_OK != rt)", output_source)
        self.assertIn("return rt;", output_source)

    def test_close_stops_and_uninitializes_both_directions(self):
        close_source = self.section("static OPERATE_RET __tdd_audio_close", "OPERATE_RET tdd_audio_register")
        for call in ("tkl_ao_stop", "tkl_ao_uninit", "tkl_ai_stop", "tkl_ai_uninit"):
            self.assertIn(call, close_source)

    def test_switch_demo_enables_only_tdd_pcm_audio_without_aec(self):
        self.assertIn("#define AUDIO_PCM_FRAME_MS       20", self.source)
        self.assertIn("config.codectype = TKL_CODEC_AUDIO_PCM", self.source)
        for setting in (
            "CONFIG_ENABLE_AUDIO_CODECS=y",
            "CONFIG_ENABLE_MEDIA=y",
            "# CONFIG_ENABLE_AUDIO_AEC is not set",
        ):
            self.assertIn(setting, self.config)
        for setting in (
            "CONFIG_ENABLE_AI_COMPONENTS=y",
            "CONFIG_ENABLE_COMP_AI_AUDIO=y",
            "CONFIG_ENABLE_AI_PLAYER=y",
            "CONFIG_ENABLE_COMP_AI_AUDIO_CODEC_PCM=y",
            "CONFIG_ENABLE_COMP_AI_AUDIO_CODEC_OPUS=y",
            "CONFIG_ENABLE_COMP_AI_AUDIO_CODEC_SPEEX=y",
        ):
            self.assertNotIn(setting, self.config)

    def test_jieli_media_target_includes_tkl_audio_and_vad_headers(self):
        self.assertIn('CONFIG_PLATFORM_CHOICE STREQUAL "JIELI"', self.codecs_cmake)
        self.assertIn('${CMAKE_SOURCE_DIR}/tools/porting/adapter/media', self.codecs_cmake)
        self.assertIn('${CMAKE_SOURCE_DIR}/tools/porting/adapter/vad', self.codecs_cmake)
        self.assertIn('list(APPEND COMPONENT_PUBINC ${LIB_PRIVATE_INC})', self.codecs_cmake)


if __name__ == "__main__":
    unittest.main()
