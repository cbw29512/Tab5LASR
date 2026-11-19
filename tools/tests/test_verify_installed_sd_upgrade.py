"""Synthetic regression tests for the read-only app-only flash guard."""
import logging
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_installed_sd_upgrade import FLASH_BYTES, verify_state

LOG = logging.getLogger(__name__)
APP_OFFSET = 0x10000


class UpgradeGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            camera = bytearray(FLASH_BYTES)
            # ESP-IDF partition record: little-endian magic, factory APP.
            struct.pack_into("<HBBII", camera, 0x8000,
                             0x50AA, 0, 0, APP_OFFSET, 0x100000)
            cls.camera = bytes(camera)
            cls.old_app = b"\xe9" + b"\xa5" * 5079
            live = bytearray(camera)
            live[APP_OFFSET:APP_OFFSET + 8192] = b"\xff" * 8192
            live[APP_OFFSET:APP_OFFSET + len(cls.old_app)] = cls.old_app
            cls.live = bytes(live)
        except (ValueError, struct.error) as exc:
            LOG.exception("Could not create upgrade guard fixtures")
            raise

    def test_exact_app_only_flash_allowed(self):
        verify_state(self.camera, self.live, self.old_app)

    def test_unexpected_bootloader_change_blocked(self):
        modified = bytearray(self.live)
        modified[0x2000] ^= 1
        with self.assertRaisesRegex(ValueError, "offset"):
            verify_state(self.camera, bytes(modified), self.old_app)

    def test_wrong_current_diagnostic_blocked(self):
        modified = bytearray(self.live)
        modified[APP_OFFSET + 42] ^= 1
        with self.assertRaisesRegex(ValueError, "offset"):
            verify_state(self.camera, bytes(modified), self.old_app)

    def test_unexpected_erase_tail_blocked(self):
        modified = bytearray(self.live)
        modified[APP_OFFSET + 7000] = 0
        with self.assertRaisesRegex(ValueError, "offset"):
            verify_state(self.camera, bytes(modified), self.old_app)

    def test_wrong_flash_size_blocked(self):
        with self.assertRaisesRegex(ValueError, "16 MiB"):
            verify_state(self.camera, self.live[:-1], self.old_app)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
