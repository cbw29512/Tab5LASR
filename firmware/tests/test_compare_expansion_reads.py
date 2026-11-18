import unittest
from tools.audit_factory_expansion import REGION_BYTES
from tools.compare_expansion_reads import compare


class ComparisonTests(unittest.TestCase):
    def test_same_count_different_bytes_are_detected(self):
        old = bytearray(b"\xff" * REGION_BYTES)
        new = bytearray(b"\xff" * REGION_BYTES)
        old[0] = 0
        new[1] = 0
        outcome = compare(bytes(old), bytes(new), bytes(new))
        self.assertEqual(outcome.old_non_erased, outcome.new_non_erased)
        self.assertFalse(outcome.same_as_original)
        self.assertEqual(outcome.changed_blocks, 1)

    def test_exactly_same_bytes(self):
        image = b"\x00" * REGION_BYTES
        outcome = compare(image, image, image)
        self.assertTrue(outcome.same_as_original)
        self.assertTrue(outcome.matching_current_reads)
        self.assertEqual(outcome.changed_blocks, 0)

    def test_two_physical_reads_disagree(self):
        image = b"\xff" * REGION_BYTES
        other = image[:-1] + b"\x00"
        self.assertFalse(compare(image, image, other).matching_current_reads)

    def test_partial_region_rejected(self):
        with self.assertRaisesRegex(ValueError, "incorrect size"):
            compare(b"short", b"short", b"short")


if __name__ == "__main__":
    unittest.main()
