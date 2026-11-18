"""USB parser tests use simulated wire frames and do not need hardware."""
import unittest
import zlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lasr_usb_protocol import FileAssembler, ProtocolError  # noqa: E402


class UsbProtocolTests(unittest.TestCase):
    def test_verified_export_with_noise(self):
        payload = b"shot,elapsed_ms\n1,750\n"
        assembler = FileAssembler()
        self.assertIsNone(assembler.feed(b"I (usb): camera status\n"))
        self.assertIsNone(assembler.feed(f"!LASR|BEGIN|session-000001|shots.csv|{len(payload)}\n".encode()))
        self.assertIsNone(assembler.feed(f"!LASR|DATA|0|{payload.hex()}\n".encode()))
        result = assembler.feed(f"!LASR|END|{zlib.crc32(payload):08x}\n".encode())
        self.assertEqual(result, ("session-000001", "shots.csv", payload))

    def test_crc_rejects_corruption(self):
        a = FileAssembler()
        a.feed(b"!LASR|BEGIN|session-000001|target.json|1\n")
        a.feed(b"!LASR|DATA|0|61\n")
        with self.assertRaises(ProtocolError):
            a.feed(b"!LASR|END|00000000\n")

    def test_gap_and_path_escape(self):
        a = FileAssembler()
        with self.assertRaises(ProtocolError):
            a.feed(b"!LASR|BEGIN|../private|shots.csv|10\n")
        a = FileAssembler()
        a.feed(b"!LASR|BEGIN|session-000001|shots.csv|2\n")
        with self.assertRaises(ProtocolError):
            a.feed(b"!LASR|DATA|1|00\n")


if __name__ == "__main__":
    unittest.main()