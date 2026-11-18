"""Read-only clearance check for enlarging factory app from 1 MiB to 2 MiB.

Only reports addresses/counts and hashes; never prints actual flash contents.
Does not write/erase/flash the device. All checks are necessary, not sufficient,
for a deliberate future partition-table migration.
"""
from __future__ import annotations

import argparse
import hashlib
import logging
import sys
from dataclasses import dataclass
from pathlib import Path

from inspect_flash_layout import FLASH_BYTES, parse_partitions

LOG = logging.getLogger("tab5_expansion")
START = 0x110000
END = 0x210000
REGION_BYTES = END - START
KNOWN_DICE_SHA256 = "52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01"


@dataclass(frozen=True)
class RegionCheck:
    non_erased: int
    first_non_erased: int | None


def inspect_region(region: bytes, flash_offset: int = START) -> RegionCheck:
    """Check every byte: 0xFF is erased state for the SPI flash region."""
    if len(region) != REGION_BYTES:
        raise ValueError(f"Expected exactly {REGION_BYTES} bytes, got {len(region)}")
    first = next((i for i, b in enumerate(region) if b != 0xFF), None)
    count = sum(b != 0xFF for b in region)
    return RegionCheck(count, flash_offset + first if first is not None else None)


def read_verified_dice_backup(filename: Path) -> bytes:
    data = filename.read_bytes()
    if len(data) != FLASH_BYTES:
        raise ValueError("Original readback must be exactly 16 MiB")
    digest = hashlib.sha256(data).hexdigest().upper()
    if digest != KNOWN_DICE_SHA256:
        raise ValueError("Original dice backup hash does not match VERIFIED private release")
    parts = parse_partitions(data)
    names = [(p.name, p.offset, p.size) for p in parts]
    expected = [("nvs", 0x9000, 0x6000),
                ("phy_init", 0xF000, 0x1000),
                ("factory", 0x10000, 0x100000)]
    if names != expected:
        raise ValueError(f"Unexpected installed partition metadata: {names}")
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dice_full_backup", type=Path)
    parser.add_argument("--read1", type=Path, required=True,
                        help="Read-only physical Tab5 read of 0x110000 + 0x100000")
    parser.add_argument("--read2", type=Path, required=True,
                        help="Second independent physical read of same addresses")
    args = parser.parse_args()
    try:
        dice = read_verified_dice_backup(args.dice_full_backup)
        snapshot = dice[START:END]
        read1 = args.read1.read_bytes()
        read2 = args.read2.read_bytes()
        digest1 = hashlib.sha256(read1).hexdigest()
        digest2 = hashlib.sha256(read2).hexdigest()
        if digest1 != digest2:
            raise ValueError("Two current-device region reads have different SHA-256 values")
        original = inspect_region(snapshot)
        current = inspect_region(read1)
        LOG.info("Verified original 16MiB dice backup and exact partition metadata")
        LOG.info("Proposed extra 1MiB range: 0x%06X through 0x%06X (exclusive)", START, END)
        LOG.info("Original non-erased bytes: %d; first address: %s",
                 original.non_erased,
                 hex(original.first_non_erased) if original.first_non_erased is not None else "none")
        LOG.info("Current non-erased bytes: %d; first address: %s",
                 current.non_erased,
                 hex(current.first_non_erased) if current.first_non_erased is not None else "none")
        if original.non_erased or current.non_erased:
            LOG.error("BLOCKED: Proposed expansion intersects existing non-erased flash bytes")
            return 1
        LOG.warning("Both snapshots show erased bytes only. This does NOT prove flash ownership.")
        LOG.warning("Requires full current-camera backup and separate verified migration review.")
        LOG.info("READ-ONLY EXPANSION REGION CHECK PASSED; NO FLASH WAS WRITTEN")
        return 0
    except (OSError, ValueError) as exc:
        LOG.error("READ-ONLY EXPANSION CHECK FAILED: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
