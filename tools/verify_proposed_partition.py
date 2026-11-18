"""Validate compiled REVIEW partition table before any physical Tab5 migration.

No flash operations. The only expected change is factory length: 1->2 MiB.
"""
from __future__ import annotations

import argparse
import logging
import struct
import sys
from pathlib import Path

LOG = logging.getLogger("tab5_proposed_layout")
EXPECTED = (
    ("nvs", 1, 2, 0x9000, 0x6000),
    ("phy_init", 1, 1, 0xF000, 0x1000),
    ("factory", 0, 0, 0x10000, 0x200000),
)


def verify_proposed_partition(data: bytes) -> None:
    if len(data) not in (0xC00, 0x1000):
        raise ValueError(f"Unexpected partition table length {len(data)}")
    for i, (name, kind, sub, start, size) in enumerate(EXPECTED):
        raw = data[i * 32:(i + 1) * 32]
        magic, actual_kind, actual_sub, actual_start, actual_size = (
            struct.unpack_from("<HBBII", raw)
        )
        actual_name = raw[12:28].split(b"\0", 1)[0].decode("ascii", "replace")
        if (magic, actual_name, actual_kind, actual_sub, actual_start, actual_size) != (
            0x50AA, name, kind, sub, start, size
        ):
            raise ValueError(f"Candidate partition {i} differs from verified map: {actual_name}")
    # Next entry is either erased padding or IDF optional MD5 marker.
    if data[96:98] not in (b"\xff\xff", b"\xeb\xeb"):
        raise ValueError("Unexpected extra partition; candidate layout not approved")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("partition_table", type=Path)
    args = parser.parse_args()
    try:
        verify_proposed_partition(args.partition_table.read_bytes())
        LOG.info("REVIEW ONLY: NVS 0x9000/0x6000, PHY 0xF000/0x1000, factory 0x10000/0x200000")
        LOG.warning("Compiled layout passed; physical device is NOT authorized to flash.")
        return 0
    except (OSError, ValueError, struct.error) as exc:
        LOG.error("Proposed partition table blocked: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
