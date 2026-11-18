import struct
import unittest

from tools.verify_proposed_partition import verify_proposed_partition, EXPECTED


def make_table():
    binary = bytearray(b"\xff" * 0xC00)
    for i, (name, kind, sub, start, size) in enumerate(EXPECTED):
        struct.pack_into("<HBBII16sI", binary, i * 32, 0x50AA,
                         kind, sub, start, size, name.encode("ascii"), 0)
    return bytes(binary)


class CandidateLayoutTests(unittest.TestCase):
    def test_approved_three_partitions(self):
        verify_proposed_partition(make_table())

    def test_rejects_extra_partition(self):
        modified = bytearray(make_table())
        modified[96:98] = b"\xaa\x50"
        with self.assertRaisesRegex(ValueError, "extra partition"):
            verify_proposed_partition(modified)

    def test_rejects_factory_off_by_one(self):
        modified = bytearray(make_table())
        struct.pack_into("<I", modified, 64 + 8, 0x1F0000)
        with self.assertRaisesRegex(ValueError, "differs"):
            verify_proposed_partition(modified)

    def test_rejects_partial_table(self):
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            verify_proposed_partition(b"")
