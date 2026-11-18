"""Read-only ESP-IDF flash partition report for a VERIFIED Tab5 16 MiB backup.

The tool never writes device flash, changes partitions, or prints NVS contents.
"""
from __future__ import annotations

import argparse
import logging
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

LOG = logging.getLogger("tab5_layout")
FLASH_BYTES = 16 * 1024 * 1024
TABLE_BASE = 0x8000
TABLE_BYTES = 0xC00
ENTRY_BYTES = 32
APP_OFFSET = 0x10000
APP_SIZE = 0x100000


@dataclass(frozen=True)
class Partition:
    """Metadata only. Never read or disclose content from any partition."""
    name: str
    kind: int
    subtype: int
    offset: int
    size: int
    flags: int

    @property
    def end(self) -> int:
        return self.offset + self.size


def parse_partitions(dump: bytes) -> list[Partition]:
    """Decode the standard ESP-IDF table, including its optional MD5 marker."""
    if len(dump) != FLASH_BYTES:
        raise ValueError(f"Expected complete 16 MiB readback, got {len(dump)} bytes")
    table = dump[TABLE_BASE:TABLE_BASE + TABLE_BYTES]
    partitions: list[Partition] = []
    for index in range(0, len(table), ENTRY_BYTES):
        raw = table[index:index + ENTRY_BYTES]
        magic = raw[:2]
        if magic == b"\xff\xff":
            break
        if magic == b"\xeb\xeb":  # Standard table MD5 record; not a partition.
            continue
        if magic != b"\xaa\x50":
            raise ValueError(f"Invalid table magic at 0x{TABLE_BASE + index:04X}")
        kind, subtype = raw[2], raw[3]
        offset, size = struct.unpack_from("<II", raw, 4)
        flags = struct.unpack_from("<I", raw, 28)[0]
        name = raw[12:28].split(b"\0", 1)[0].decode("ascii", "replace")
        if not name or not size or offset < TABLE_BASE + 0x1000 or offset + size > FLASH_BYTES:
            raise ValueError(f"Invalid partition name/range at index {index // ENTRY_BYTES}")
        partitions.append(Partition(name, kind, subtype, offset, size, flags))
    if not partitions:
        raise ValueError("No valid partition entries found")
    partitions.sort(key=lambda p: p.offset)
    for first, second in zip(partitions, partitions[1:]):
        if first.end > second.offset:
            raise ValueError(f"Overlapping partitions: {first.name} and {second.name}")
    factory = [p for p in partitions if p.kind == 0 and p.subtype == 0]
    if len(factory) != 1 or (factory[0].offset, factory[0].size) != (APP_OFFSET, APP_SIZE):
        raise ValueError("Factory partition does not match verified 0x10000 / 1 MiB")
    return partitions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("readback", type=Path, help="Verified LOCAL 16MiB flash file")
    args = parser.parse_args()
    try:
        # Read locally; print only metadata (never raw image bytes).
        data = args.readback.read_bytes()
        partitions = parse_partitions(data)
        LOG.info("FLASH: 0x%X bytes; table 0x%X", len(data), TABLE_BASE)
        for p in partitions:
            LOG.info("PART %-16s type=0x%02X sub=0x%02X "
                     "start=0x%06X end=0x%06X size=%7d flags=0x%X",
                     p.name, p.kind, p.subtype,
                     p.offset, p.end, p.size, p.flags)
        LOG.warning("Gaps are NOT proven free: NVS, boot data or vendor uses may exist.")
        LOG.warning("READ ONLY. No partition migration or firmware flash performed.")
        return 0
    except (OSError, ValueError, struct.error) as exc:
        LOG.error("Cannot inspect Tab5 flash layout: %s", exc)
        return 1


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    sys.exit(main())
