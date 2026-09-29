import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
QUEUE_SOURCE = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/system/tkl_queue.c"


class JieliTklQueueContractTest(unittest.TestCase):
    def test_post_passes_owned_payload_pointer_to_jieli_os_queue(self):
        source = QUEUE_SOURCE.read_text(encoding="utf-8")

        self.assertRegex(
            source,
            r"os_q_post_to_back\(\s*&jieli_queue->queue,\s*copy\s*,",
            "Jieli OS queues store message pointers; posting &copy queues a dead stack address.",
        )

    def test_fetch_receives_queued_payload_pointer_before_copying_message(self):
        source = QUEUE_SOURCE.read_text(encoding="utf-8")

        self.assertRegex(
            source,
            r"os_q_pend\(&jieli_queue->queue,\s*jieli_tkl_timeout_to_ticks\(timeout\),\s*&copy\)",
        )
        self.assertIn("memcpy(msg, copy, jieli_queue->message_size)", source)


if __name__ == "__main__":
    unittest.main()
