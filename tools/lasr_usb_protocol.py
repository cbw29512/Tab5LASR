"""Schema and state for read-only LASR1 USB Serial/JTAG file transfer.

This protocol copies VERIFIED session files; it never modifies the microSD.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import logging
import re
import zlib

LOG = logging.getLogger(__name__)
SESSION_ID = re.compile(r"session-\d{6}\Z")
ALLOWED_FILES = {"session.json", "target.json", "shots.csv"}
MAX_FILE = 10 * 1024 * 1024


@dataclass
class TransferState:
    session_id: str = ""
    filename: str = ""
    expected_size: int = -1
    next_sequence: int = 0
    buffer: bytearray = field(default_factory=bytearray)
    phase: str = "AWAIT_BEGIN"


class ProtocolError(ValueError):
    """Rejected frame, missing data, or corrupted transfer."""


class FileAssembler:
    def __init__(self) -> None:
        self.state = TransferState()

    def feed(self, raw_line: bytes) -> tuple[str, str, bytes] | None:
        try:
            line = raw_line.decode("ascii", errors="strict").strip()
            if not line.startswith("!LASR|"):
                return None  # Camera diagnostic logs are not transfer records.
            parts = line.split("|")
            kind = parts[1] if len(parts) >= 2 else ""
            state = self.state
            if kind == "ERROR":
                raise ProtocolError("Device rejected transfer: " + " ".join(parts[2:]))
            if kind == "BEGIN":
                if len(parts) != 5 or not SESSION_ID.fullmatch(parts[2]) or parts[3] not in ALLOWED_FILES:
                    raise ProtocolError("Malformed LASR transfer header")
                size = int(parts[4])
                if size < 0 or size > MAX_FILE:
                    raise ProtocolError("Invalid or oversized file")
                self.state = TransferState(parts[2], parts[3], size, 0, bytearray(), "DATA")
            elif kind == "DATA":
                if state.phase != "DATA" or len(parts) != 4:
                    raise ProtocolError("Unexpected LASR data block")
                sequence = int(parts[2])
                if sequence != state.next_sequence:
                    raise ProtocolError("Lost or duplicate data block")
                chunk = bytes.fromhex(parts[3])
                if not chunk or len(chunk) > 48 or len(state.buffer) + len(chunk) > state.expected_size:
                    raise ProtocolError("Invalid LASR chunk length")
                state.buffer.extend(chunk)
                state.next_sequence += 1
            elif kind == "END":
                if state.phase != "DATA" or len(parts) != 3:
                    raise ProtocolError("Unexpected LASR end marker")
                expected_crc = int(parts[2], 16)
                content = bytes(state.buffer)
                if len(content) != state.expected_size or zlib.crc32(content) != expected_crc:
                    raise ProtocolError("Incomplete file or CRC32 mismatch")
                result = (state.session_id, state.filename, content)
                self.state = TransferState(phase="COMPLETE")
                return result
            elif kind not in {"LIST_BEGIN", "LIST_END", "ITEM"}:
                LOG.debug("Ignored unexpected LASR command: %s", kind)
            return None
        except (ValueError, UnicodeError) as exc:
            LOG.error("Invalid USB transfer frame: %s", exc)
            if isinstance(exc, ProtocolError):
                raise
            raise ProtocolError(str(exc)) from exc