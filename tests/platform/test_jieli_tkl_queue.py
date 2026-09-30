import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
QUEUE_SOURCE = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter/src/system/tkl_queue.c"


class JieliTklQueueContractTest(unittest.TestCase):
    def test_post_copies_message_contents_into_queue_owned_storage(self):
        source = QUEUE_SOURCE.read_text(encoding="utf-8")

        # The vendor os_q_* API was dropped: the two SDKs disagree on
        # os_q_post_to_back()'s message semantics, so TKL queues are a local
        # ring buffer now. The contract that survives is that the message
        # contents land in the queue's own slot storage -- queueing the
        # address of a caller-local variable would store a dead stack
        # pointer.
        post_source = source[
            source.index("OPERATE_RET tkl_queue_post"):
            source.index("OPERATE_RET tkl_queue_fetch")
        ]
        self.assertNotIn("os_q_", post_source)
        self.assertIn(
            "memcpy(jieli_queue->slots + (size_t)slot * jieli_queue->message_size, data, jieli_queue->message_size)",
            source,
            "TKL queues store message contents, not pointers to them.",
        )

    def test_fetch_copies_queued_message_out_of_queue_storage(self):
        source = QUEUE_SOURCE.read_text(encoding="utf-8")

        fetch_source = source[
            source.index("OPERATE_RET tkl_queue_fetch"):
            source.index("void tkl_queue_free")
        ]
        self.assertNotIn("os_q_", fetch_source)

        # The caller's buffer receives the message contents copied out of the
        # queue's slot storage, never a pointer into storage that the next
        # post would overwrite.
        self.assertIn(
            "memcpy(msg, jieli_queue->slots + (size_t)slot * jieli_queue->message_size, jieli_queue->message_size)",
            source,
        )
        # Both wait-forever and finite-timeout posts/fetches must block on the
        # free_slots/filled pair, so a timeout is honoured without polling.
        self.assertIn("tkl_semaphore_wait(jieli_queue->free_slots, timeout)", source)
        self.assertIn("tkl_semaphore_wait(jieli_queue->filled, timeout)", source)
        self.assertIn("(void)tkl_semaphore_post(jieli_queue->filled)", source)
        self.assertIn("(void)tkl_semaphore_post(jieli_queue->free_slots)", source)


if __name__ == "__main__":
    unittest.main()
