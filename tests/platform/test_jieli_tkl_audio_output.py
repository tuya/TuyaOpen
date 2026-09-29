import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
TKL_AUDIO = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio.c"
NATIVE_AUDIO = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio_jieli_wl83_native.c"
PLAYER_HEADER = ROOT / "src/audio_player/include/svc_ai_player.h"


def _byte_capacity(source, macro):
    match = re.search(rf"#define\s+{macro}\s+\((\d+)u?\s*\*\s*(\d+)u?\)", source)
    if match:
        return int(match.group(1)) * int(match.group(2))
    match = re.search(rf"#define\s+{macro}\s+(\d+)u?", source)
    if not match:
        raise AssertionError(f"missing byte-capacity definition for {macro}")
    return int(match.group(1))


class JieliTklAudioOutputContractTest(unittest.TestCase):
    def test_tkl_accepts_a_full_ai_player_decode_chunk(self):
        tkl_source = TKL_AUDIO.read_text(encoding="utf-8")
        player_source = PLAYER_HEADER.read_text(encoding="utf-8")

        tkl_capacity = _byte_capacity(tkl_source, "JIELI_AUDIO_MAX_PLAY_BYTES")
        player_capacity = _byte_capacity(player_source, "AI_PLAYER_DECODEBUF_SIZE")

        self.assertGreaterEqual(tkl_capacity, player_capacity)

    def test_tkl_frame_limit_fits_in_wl83_playback_queue(self):
        tkl_source = TKL_AUDIO.read_text(encoding="utf-8")
        native_source = NATIVE_AUDIO.read_text(encoding="utf-8")

        tkl_capacity = _byte_capacity(tkl_source, "JIELI_AUDIO_MAX_PLAY_BYTES")
        queue_capacity = _byte_capacity(native_source, "WL83_PLAY_QUEUE_SIZE")

        self.assertLessEqual(tkl_capacity, queue_capacity)

    def test_playback_stop_drains_before_closing_the_output(self):
        tkl_source = TKL_AUDIO.read_text(encoding="utf-8")
        stop_start = tkl_source.index("OPERATE_RET tkl_ao_stop(")
        stop_end = tkl_source.index("OPERATE_RET tkl_ao_uninit(", stop_start)
        stop_source = tkl_source[stop_start:stop_end]
        self.assertLess(stop_source.index("s_backend.ops.ao_flush"),
                        stop_source.index("s_backend.ops.ao_stop"))

    def test_native_flush_waits_for_software_queue_and_dac_completion(self):
        native_source = NATIVE_AUDIO.read_text(encoding="utf-8")
        flush_start = native_source.index("int jieli_audio_native_ao_flush(")
        flush_end = native_source.index("\n}", flush_start) + 2
        flush_source = native_source[flush_start:flush_end]
        self.assertIn("audio_dac_idle", flush_source)
        self.assertIn("queued_bytes == 0", flush_source)
        self.assertIn("s_play.writer_inflight", flush_source)
        self.assertNotIn("s_play.used = 0", flush_source)
        self.assertNotIn("s_play.read_pos = s_play.write_pos", flush_source)


if __name__ == "__main__":
    unittest.main()
