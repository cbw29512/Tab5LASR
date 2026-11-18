import unittest

from tools.audit_factory_expansion import REGION_BYTES, START, inspect_region


class ExpansionRegionTests(unittest.TestCase):
    def test_erased_range(self):
        result = inspect_region(b"\xff" * REGION_BYTES)
        self.assertEqual(result.non_erased, 0)
        self.assertIsNone(result.first_non_erased)

    def test_single_written_byte_at_start(self):
        region = bytearray(b"\xff" * REGION_BYTES)
        region[0] = 0x00
        result = inspect_region(region)
        self.assertEqual(result.non_erased, 1)
        self.assertEqual(result.first_non_erased, START)

    def test_written_byte_at_end(self):
        region = bytearray(b"\xff" * REGION_BYTES)
        region[-1] = 0x01
        result = inspect_region(region)
        self.assertEqual(result.first_non_erased, START + REGION_BYTES - 1)

    def test_rejects_partial_region(self):
        with self.assertRaisesRegex(ValueError, "exactly"):
            inspect_region(b"\xff" * 30)


if __name__ == "__main__":
    unittest.main()
