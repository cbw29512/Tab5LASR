"""Read-only guard for upgrading the installed SD diagnostic (never flashes)."""
from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

from verify_factory_partition import validate_dump

LOG = logging.getLogger("lasr_sd_upgrade")
SECTOR_SIZE = 4096
FLASH_BYTES = 16 * 1024 * 1024


def verify_state(camera: bytes, live: bytes, previous_image: bytes) -> None:
    """Ensure live flash is exactly camera backup plus the previous app-only flash.

    esptool erases whole 4 KiB sectors covering the application image. Bytes
    outside those sectors must still equal the preserved camera recovery dump.
    """
    if len(camera) != FLASH_BYTES or len(live) != FLASH_BYTES:
        raise ValueError("Both recovery and live flash must be exactly 16 MiB")
    app_offset, app_size = validate_dump(camera, len(previous_image))
    if not previous_image or previous_image[0] != 0xE9:
        raise ValueError("Previous binary is not an ESP-IDF application image")
    erased_bytes = ((len(previous_image) + SECTOR_SIZE - 1)
                    // SECTOR_SIZE) * SECTOR_SIZE
    if erased_bytes > app_size:
        raise ValueError("Previous image would erase beyond factory app partition")

    expected = bytearray(camera)
    expected[app_offset:app_offset + erased_bytes] = b"\xff" * erased_bytes
    expected[app_offset:app_offset + len(previous_image)] = previous_image
    if live != expected:
        mismatch = next(i for i, (actual, correct)
                        in enumerate(zip(live, expected)) if actual != correct)
        raise ValueError(
            f"Live flash differs from verified previous app-only flash "
            f"at offset {mismatch:#x}; refusing upgrade"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("camera_backup", type=Path)
    parser.add_argument("live_flash", type=Path)
    parser.add_argument("previous_diagnostic", type=Path)
    args = parser.parse_args()
    try:
        verify_state(args.camera_backup.read_bytes(),
                     args.live_flash.read_bytes(),
                     args.previous_diagnostic.read_bytes())
        LOG.info("PASS: verified previous SD diagnostic and unchanged non-app flash")
        return 0
    except (OSError, ValueError) as exc:
        LOG.error("SD UPGRADE BLOCKED: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
