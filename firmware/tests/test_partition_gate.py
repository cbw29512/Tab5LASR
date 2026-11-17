import unittest
import struct
from tools.verify_factory_partition import find_factory_partition, validate_dump

class PartitionGateTests(unittest.TestCase):
    def make_dump(self, *, offset=0x10000, size=0x100000):
        data = bytearray(b"\xff" * (16 * 1024 * 1024))
        struct.pack_into("<HBBII", data, 0x8000, 0x50AA, 0, 0, offset, size)
        return bytes(data)

    def test_expected_factory_partition(self):
        self.assertEqual(find_factory_partition(self.make_dump()), (0x10000, 0x100000))
        self.assertEqual(validate_dump(self.make_dump(), 500000), (0x10000, 0x100000))

    def test_wrong_partition_offset(self):
        with self.assertRaisesRegex(ValueError, "Unexpected factory app offset"):
            validate_dump(self.make_dump(offset=0x20000), 100)

    def test_image_too_large(self):
        with self.assertRaisesRegex(ValueError, "exceeds"):
            validate_dump(self.make_dump(), 0x100001)

    def test_dump_too_short(self):
        with self.assertRaisesRegex(ValueError, "complete 16 MiB"):
            validate_dump(b"too short", 100)

    def test_no_factory_entry(self):
        with self.assertRaisesRegex(ValueError, "No factory"):
            find_factory_partition(b"\xff" * (16 * 1024 * 1024))

if __name__ == "__main__":
    unittest.main()
