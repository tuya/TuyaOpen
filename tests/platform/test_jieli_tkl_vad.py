import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
VAD = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/driver/tkl_vad.c"


def _without_comments(source):
    """Drop C comments so prose about a call is not mistaken for the call."""
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


class JieliTklVadContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = VAD.read_text(encoding="utf-8")
        cls.code = _without_comments(cls.source)

    def section(self, start, end):
        begin = self.code.index(start)
        return self.code[begin:self.code.index(end, begin)]

    def definition(self, name):
        """The body of a function definition, not its forward declaration.

        tkl_jieli_vad_feed_capture is declared near the top of the file and
        defined at the bottom, so anchoring on the first occurrence would scope
        every assertion to the wrong region.
        """
        begin = self.code.rindex(f"{name}(")
        return self.code[begin:self.code.index("\n}", begin)]

    def test_capture_feeder_carries_partial_frames_across_callbacks(self):
        """A VAD frame longer than one capture callback must still be fed.

        The WL83 capture callback delivers a fixed 20 ms (640 bytes) per call,
        while tkl_vad_init accepts frames up to 60 ms. Slicing only the whole
        frames contained in a single callback made 30-60 ms configurations a
        silent no-op: the loop bound was never satisfied, tkl_vad_feed() was
        never called, and the status stayed NONE with no error reported
        anywhere. The feeder must accumulate instead.
        """
        feeder = self.definition("tkl_jieli_vad_feed_capture")

        # It must keep a carry-over length between calls.
        self.assertIn("s_vad_pcm_len", feeder)
        # ...and copy into the carry-over buffer rather than slicing in place.
        self.assertIn("memcpy(s_vad_pcm", feeder)
        # The loop must be bounded by the input size, not by whole frames only:
        # the old bound `offset + frame_bytes <= size` is exactly the defect.
        self.assertNotRegex(
            feeder,
            r"offset\s*\+\s*frame_bytes\s*<=",
            "the feeder must not skip a callback whose size is smaller than one frame",
        )
        self.assertIn("offset < size", feeder)

    def test_capture_feeder_clears_the_carry_over_when_it_feeds_a_frame(self):
        """The carry-over must reset on feed, or frames would be fed twice."""
        feeder = self.definition("tkl_jieli_vad_feed_capture")
        feed_call = feeder.index("tkl_vad_feed(s_vad_pcm")
        preceding = feeder[:feed_call]
        self.assertIn("s_vad_pcm_len = 0;", preceding)

    def test_every_lifecycle_transition_drops_stale_pcm(self):
        """Init, start, stop and deinit must not carry PCM across sessions.

        Otherwise the tail of one capture session is prepended to the first
        frame of the next, which misreports the frame's energy.
        """
        for start, end in (
            ("OPERATE_RET tkl_vad_init(", "static uint32_t __required_frame_bytes"),
            ("OPERATE_RET tkl_vad_start(", "OPERATE_RET tkl_vad_stop("),
            ("OPERATE_RET tkl_vad_stop(", "OPERATE_RET tkl_vad_deinit("),
            ("OPERATE_RET tkl_vad_deinit(", "void tkl_jieli_vad_feed_capture("),
        ):
            with self.subTest(function=start):
                self.assertIn("s_vad_pcm_len = 0;", self.section(start, end))

    def test_carry_over_buffer_fits_the_largest_accepted_frame(self):
        """The buffer is sized from the same constant init validates against."""
        self.assertIn("JIELI_VAD_MAX_FRAME_BYTES", self.code)
        self.assertIn("JIELI_VAD_MAX_FRAME_MS", self.code)
        # The feeder must refuse a frame it cannot hold rather than overrun it.
        feeder = self.definition("tkl_jieli_vad_feed_capture")
        self.assertIn("frame_bytes > sizeof(s_vad_pcm)", feeder)


if __name__ == "__main__":
    unittest.main()
