"""Read-only guard: verify the installed factory app partition in a full Tab5 dump."""
from __future__ import annotations
import argparse
import logging
import struct
import sys
from pathlib import Path

LOG = logging.getLogger("partition")
PARTITION_TABLE_OFFSET = 0x8000
ENTRY_SIZE = 32
EXPECTED_APP_OFFSET = 0x10000
EXPECTED_FLASH_BYTES = 16 * 1024 * 1024


def find_factory_partition(data: bytes) -> tuple[int, int]:
    """Parse original IDF 32-byte partition records, not a guessed app layout."""
    if len(data) < PARTITION_TABLE_OFFSET + 0x1000:
        raise ValueError("Device dump too short for partition table")
    table = data[PARTITION_TABLE_OFFSET : PARTITION_TABLE_OFFSET + 0xC00]
    for index in range(0, len(table), ENTRY_SIZE):
        entry = table[index:index + ENTRY_SIZE]
        if entry[:2] == b"\xff\xff":
            break
        if entry[:2] != b"\xaa\x50":  # IDF magic 0x50AA in little endian
            raise ValueError(f"Invalid partition entry magic at {index:#x}")
        kind, subtype = entry[2], entry[3]
        offset, size = struct.unpack_from("<II", entry, 4)
        if kind == 0 and subtype == 0:  # APP / FACTORY
            return offset, size
    raise ValueError("No factory application partition found")


def validate_dump(dump: bytes, image_length: int) -> tuple[int, int]:
    if len(dump) != EXPECTED_FLASH_BYTES:
        raise ValueError("Backup is not the complete 16 MiB device")
    offset, size = find_factory_partition(dump)
    if offset != EXPECTED_APP_OFFSET:
        raise ValueError(f"Unexpected factory app offset {offset:#x}")
    if size <= 0 or image_length <= 0 or image_length > size:
        raise ValueError(f"Camera image {image_length} exceeds app partition {size}")
    return offset, size


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("full_flash", type=Path)
    parser.add_argument("application", type=Path)
    args = parser.parse_args()
    try:
        dump = args.full_flash.read_bytes()
        image_length = args.application.stat().st_size
        offset, size = validate_dump(dump, image_length)
        LOG.info("PASS: installed factory app partition offset=%#x size=%d bytes", offset, size)
        LOG.info("PASS: candidate camera app=%d bytes", image_length)
        return 0
    except (OSError, ValueError, struct.error) as exc:
        LOG.error("FLASH GATE BLOCKED: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
