"""Regression guard: reboot must restore the user's Color 2 baseline.

The four display-only diagnostic modes remain available, but the provisional
byte-swap setting must not silently become the power-on default again.
"""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[2] / "onboard/main/onboard_color_ui.c"


class CameraColorStartupTest(unittest.TestCase):
    def test_color_two_is_boot_default(self):
        try:
            content = SOURCE.read_text(encoding="utf-8")
        except OSError as exc:
            self.fail(f"Cannot read camera color startup source: {exc}")
        found = re.search(
            r"static\s+onboard_color_mode_t\s+g_mode\s*=\s*(\w+)\s*;",
            content,
        )
        self.assertIsNotNone(found, "Color mode initializer missing")
        self.assertEqual(
            found.group(1), "ONBOARD_COLOR_SWAP_RED_BLUE",
            "Power cycling must restore the user-selected red/blue preview mode",
        )

    def test_color_mode_button_still_cycles(self):
        try:
            content = SOURCE.read_text(encoding="utf-8")
        except OSError as exc:
            self.fail(f"Cannot read diagnostic UI source: {exc}")
        self.assertIn("ONBOARD_COLOR_MODE_COUNT", content)
        self.assertIn("LV_EVENT_CLICKED", content)


if __name__ == "__main__":
    unittest.main()
