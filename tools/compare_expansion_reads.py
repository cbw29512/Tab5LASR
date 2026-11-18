"""Read-only comparison: original dice flash vs two CURRENT expansion-region reads.

Never displays raw device bytes. Metadata alone cannot authorize erasing flash.
"""
from __future__ import annotations

import argparse
import hashlib
import logging
import sys
from dataclasses import dataclass
from pathlib import Path

from audit_factory_expansion import (
    END, KNOWN_DICE_SHA256, REGION_BYTES, START, read_verified_dice_backup
)

LOG = logging.getLogger("tab5_flash_compare")
BLOCK_BYTES = 0x10000


@dataclass(frozen=True)
class Comparison:
    matching_current_reads: bool
    same_as_original: bool
    old_non_erased: int
    new_non_erased: int
    changed_blocks: int


def compare(original: bytes, current1: bytes, current2: bytes) -> Comparison:
    """Compare bytes exactly—not merely the count of non-erased bytes."""
    if any(len(p) != REGION_BYTES for p in (original, current1, current2)):
        raise ValueError("One or more region images have incorrect size")
    changed_blocks = sum(
        original[off:off + BLOCK_BYTES] != current1[off:off + BLOCK_BYTES]
        for off in range(0, REGION_BYTES, BLOCK_BYTES)
    )
    return Comparison(
        matching_current_reads=current1 == current2,
        same_as_original=original == current1,
        old_non_erased=sum(v != 0xFF for v in original),
        new_non_erased=sum(v != 0xFF for v in current1),
        changed_blocks=changed_blocks,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dice_backup", type=Path)
    parser.add_argument("current_read1", type=Path)
    parser.add_argument("current_read2", type=Path)
    args = parser.parse_args()
    try:
        original = read_verified_dice_backup(args.dice_backup)[START:END]
        current1 = args.current_read1.read_bytes()
        current2 = args.current_read2.read_bytes()
        result = compare(original, current1, current2)
        LOG.info("Test range 0x%06X..0x%06X (end exclusive)", START, END)
        LOG.info("Current physical reads match exactly: %s", result.matching_current_reads)
        LOG.info("Current region matches original dice readback exactly: %s",
                 result.same_as_original)
        LOG.info("Non-erased bytes original=%d, current=%d",
                 result.old_non_erased, result.new_non_erased)
        LOG.info("Different 64 KiB blocks vs original: %d of %d",
                 result.changed_blocks, REGION_BYTES // BLOCK_BYTES)
        LOG.warning("BLOCKED: Non-erased data exists. Ownership is UNKNOWN.")
        LOG.warning("This report does not authorize erasing or changing the partition table.")
        if not result.matching_current_reads:
            LOG.error("Current physical reads differ; investigate acquisition reliability")
            return 1
        return 0
    except (OSError, ValueError) as exc:
        LOG.error("Could not compare flash regions: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
