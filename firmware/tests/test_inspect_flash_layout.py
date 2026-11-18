import struct
import unittest

from tools.inspect_flash_layout import FLASH_BYTES, parse_partitions


class LayoutTests(unittest.TestCase):
    def make_dump(self, *, second_offset=0x110000, second_size=0x4000):
        dump = bytearray(b"\xff" * FLASH_BYTES)
        struct.pack_into("<HBBII16sI", dump, 0x8000,
                         0x50AA, 0, 0, 0x10000, 0x100000,
                         b"factory\0", 0)
        struct.pack_into("<HBBII16sI", dump, 0x8020,
                         0x50AA, 1, 2, second_offset, second_size,
                         b"storage\0", 0)
        # MD5 record is optional; skip metadata, never show it as partition.
        dump[0x8040:0x8042] = b"\xeb\xeb"
        return bytes(dump)

    def test_factory_and_storage(self):
        parts = parse_partitions(self.make_dump())
        self.assertEqual([p.name for p in parts], ["factory", "storage"])
        self.assertEqual(parts[0].end, 0x110000)

    def test_overlap_rejected(self):
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            parse_partitions(self.make_dump(second_offset=0x100000))

    def test_out_of_flash_rejected(self):
        with self.assertRaisesRegex(ValueError, "Invalid partition"):
            parse_partitions(self.make_dump(second_offset=0xFF0000, second_size=0x20000))

    def test_partial_dump_rejected(self):
        with self.assertRaisesRegex(ValueError, "complete 16 MiB"):
            parse_partitions(b"partial")


if __name__ == "__main__":
    unittest.main()
