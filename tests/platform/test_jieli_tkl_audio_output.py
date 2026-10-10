import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
TKL_AUDIO = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_audio.c"
PLAYER_HEADER = ROOT / "src/audio_player/include/svc_ai_player.h"


def _tkl_source():
    return TKL_AUDIO.read_text(encoding="utf-8")


def _wl83_native_source():
    """The WL83 (AC792) SoC provider region of tkl_audio.c.

    The refactor folded the former tkl_audio_jieli_wl83_native.c provider into
    tkl_audio.c behind #if defined(JIELI_SELECTED_CHIP_WL83). Scoping to that
    region keeps the shared forward declarations (which precede it) out of the
    function-anchor search.
    """
    source = _tkl_source()
    end = source.index("#elif defined(JIELI_SELECTED_CHIP_WL82)")
    # The chip guard appears twice: once around the shared includes at the top
    # of the file and once around the provider itself. Take the last one.
    start = source.rindex("#if defined(JIELI_SELECTED_CHIP_WL83)", 0, end)
    return source[start:end]


def _byte_capacity(source, macro):
    match = re.search(rf"#define\s+{macro}\s+\((\d+)u?\s*\*\s*(\d+)u?\)", source)
    if match:
        return int(match.group(1)) * int(match.group(2))
    match = re.search(rf"#define\s+{macro}\s+(\d+)u?", source)
    if not match:
        raise AssertionError(f"missing byte-capacity definition for {macro}")
    return int(match.group(1))


def _without_comments(source):
    """Drop C comments so prose about an API is not mistaken for a call to it."""
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


class JieliTklAudioOutputContractTest(unittest.TestCase):
    def test_tkl_accepts_a_full_ai_player_decode_chunk(self):
        tkl_source = _tkl_source()
        player_source = PLAYER_HEADER.read_text(encoding="utf-8")

        tkl_capacity = _byte_capacity(tkl_source, "JIELI_AUDIO_MAX_PLAY_BYTES")
        player_capacity = _byte_capacity(player_source, "AI_PLAYER_DECODEBUF_SIZE")

        self.assertGreaterEqual(tkl_capacity, player_capacity)

    def test_tkl_frame_limit_fits_in_wl83_playback_queue(self):
        tkl_source = _tkl_source()
        wl83_native_source = _wl83_native_source()

        tkl_capacity = _byte_capacity(tkl_source, "JIELI_AUDIO_MAX_PLAY_BYTES")
        queue_capacity = _byte_capacity(wl83_native_source, "WL83_PLAY_QUEUE_SIZE")

        self.assertLessEqual(tkl_capacity, queue_capacity)

    def test_playback_stop_drains_before_closing_the_output(self):
        tkl_source = _tkl_source()
        stop_start = tkl_source.index("OPERATE_RET tkl_ao_stop(")
        stop_end = tkl_source.index("OPERATE_RET tkl_ao_uninit(", stop_start)
        stop_source = tkl_source[stop_start:stop_end]
        self.assertLess(
            stop_source.index("jieli_audio_native_ao_flush(s_ao.stream)"),
            stop_source.index("jieli_audio_native_ao_stop(s_ao.stream)"),
        )

    def test_native_playback_initializes_the_jieli_volume_state_machine(self):
        """audio_dac_set_volume() alone leaves the DAC at zero gain.

        Under SYS_VOL_TYPE == VOL_TYPE_DIGITAL the vendor fade handler ignores
        the level passed to audio_dac_set_volume() and applies the mixer's own
        analog_volume_l/r and digital_volume instead
        (audio/common/audio_volume_mixer.c: audio_fade_in_fade_out). Those are
        zero-initialized and only ever filled in by app_audio_state_switch(),
        which every vendor app calls on entering a playback state
        (apps/*/mode/bt/*.c). digital_volume is Q14, so zero means silence.

        TuyaOpen drives the DAC directly and never runs the vendor app, so the
        adapter has to perform that state switch itself or the DAC plays
        silence no matter what volume is requested.
        """
        native_source = _without_comments(_wl83_native_source())
        start = native_source.index("int jieli_audio_native_ao_init(")
        end = native_source.index("\n}", start) + 2
        init_source = native_source[start:end]

        self.assertIn(
            "app_audio_state_switch(",
            init_source,
            "playback init must enter a JieLi audio state so analog_volume_l/r "
            "and digital_volume are initialized before the DAC channel starts",
        )

    def test_native_playback_writes_the_level_to_the_dac_digital_gain(self):
        """The requested level has to reach the DAC's digital gain directly.

        audio_dac_set_volume() only records a level, and under
        SYS_VOL_TYPE == VOL_TYPE_DIGITAL the fade handler applies the mixer's
        own digital_volume instead, so nothing in the vendor path turns a
        requested level into attenuation. The adapter therefore maps the 0-100
        level onto JieLi's 0-31 digital volume curve (audio/common/audio_dvol.c
        default_dig_vol_table: 1.5 dB per step, level 31 = 16384 = 0 dB) and
        writes the Q14 result with audio_dac_set_L/R_digital_vol().
        audio_dac_set_RL_digital_vol() is declared in audio_dac.h but not
        exported by media.a, so it cannot be used.

        Ordering matters: the fade handler runs at channel start and overwrites
        anything written before it, so the gain is applied after
        audio_dac_channel_start().
        """
        native_source = _without_comments(_wl83_native_source())

        helper_start = native_source.index("static void __play_apply_digital_volume(")
        helper_end = native_source.index("\n}", helper_start) + 2
        helper = native_source[helper_start:helper_end]
        self.assertIn("audio_dac_set_L_digital_vol(", helper)
        self.assertIn("audio_dac_set_R_digital_vol(", helper)
        self.assertIn("16384", helper, "the curve is Q14: 16384 is unity gain")
        self.assertIn("1.5", helper, "the vendor curve steps 1.5 dB per level")

        start = native_source.index("int jieli_audio_native_ao_start(")
        end = native_source.index("int jieli_audio_native_ao_stop(", start)
        start_source = native_source[start:end]
        self.assertIn("__play_apply_digital_volume()", start_source)
        self.assertLess(
            start_source.rindex("audio_dac_channel_start(NULL)"),
            start_source.rindex("__play_apply_digital_volume()"),
            "the gain must be applied after the channel starts, or the fade "
            "handler's digital_volume overwrites it",
        )

        set_vol_start = native_source.index("int jieli_audio_native_ao_set_volume(")
        set_vol_end = native_source.index("\n}", set_vol_start) + 2
        self.assertIn("__play_apply_digital_volume()", native_source[set_vol_start:set_vol_end])

    def test_native_playback_volume_keeps_the_jieli_zero_to_hundred_scale(self):
        """audio_dac_set_volume() takes JieLi's 0-100 level.

        The vendor volume mixer feeds it 0-100 values directly
        (audio/cpu/wl83/audio_config_def.h: IDLE_DEFAULT_MAX_VOLUME is 100, and
        audio_fade_in_fade_out() clamps its argument to that same maximum).
        Rescaling to a 0-15 hardware-looking range capped playback at 15% and
        turned the 80% default into a gain of 12, which is inaudible on the
        dev board's amplifier.
        """
        native_source = _without_comments(_wl83_native_source())
        start = native_source.index("int jieli_audio_native_ao_set_volume(")
        end = native_source.index("\n}", start) + 2
        set_volume_source = native_source[start:end]

        match = re.search(r"audio_dac_set_volume\(\s*s_play\.dac\s*,\s*([^;]+?)\)\s*;", set_volume_source)
        self.assertIsNotNone(match, "ao_set_volume must forward the level to audio_dac_set_volume")

        gain = match.group(1).strip()
        self.assertIn("volume", gain)
        self.assertNotRegex(
            gain,
            r"[*/]",
            "audio_dac_set_volume() expects the 0-100 level itself, not a rescaled "
            f"hardware range; got '{gain}'",
        )

    def test_native_capture_gain_keeps_the_adc_zero_to_nineteen_scale(self):
        """audio_adc_mic_set_gain() is a different scale from the DAC.

        include_lib/media/audio_adc.h documents MIC gain as 0(-8dB)~19(30dB),
        so the capture path's rescaling is correct and must not be "fixed" to
        match the playback path. Both capture entry points must feed the ADC a
        rescaled gain rather than the raw 0-100 level.
        """
        native_source = _without_comments(_wl83_native_source())
        self.assertIn("(volume * 19 + 50) / 100", native_source)

        for call in re.findall(r"audio_adc_mic_set_gain\([^;]+\);", native_source):
            self.assertRegex(
                call,
                r"(\* 19|\bgain\b)",
                f"capture gain must be rescaled to the ADC's 0-19 range; got '{call}'",
            )

    def test_native_flush_waits_for_software_queue_and_dac_completion(self):
        native_source = _wl83_native_source()
        flush_start = native_source.index("int jieli_audio_native_ao_flush(")
        flush_end = native_source.index("\n}", flush_start) + 2
        flush_source = native_source[flush_start:flush_end]
        self.assertIn("audio_dac_idle", flush_source)
        self.assertIn("queued_bytes == 0", flush_source)
        self.assertIn("s_play.writer_inflight", flush_source)
        self.assertNotIn("s_play.used = 0", flush_source)
        self.assertNotIn("s_play.read_pos = s_play.write_pos", flush_source)

    def test_wl82_never_registers_a_server_event_handler(self):
        # server_register_event_handler() binds the callback to the *calling*
        # task and requires that task to service a vendor message queue. Our
        # callers are TuyaOpen threads (tuya_app_main, ai_player) that never do,
        # so the server spins in "wait_send_event: <task>" and the next
        # server_request() deadlocks - measured on AC791 as tdl_audio_close()
        # never returning during a repeated open/close loop. The handler was a
        # no-op, so the registration is simply not made. The vendor uses
        # server_register_event_handler_to_task(..., "app_core"); if this ever
        # needs to come back it must take that form.
        source = _without_comments(_tkl_source())
        self.assertNotIn("server_register_event_handler(", source)


if __name__ == "__main__":
    unittest.main()
