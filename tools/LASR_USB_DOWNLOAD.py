"""Copy saved LASR sessions over the Tab5 USB Serial/JTAG COM port.

Example: python tools/LASR_USB_DOWNLOAD.py --port COM5 --all
Requires: python -m pip install pyserial
This is NOT an MSC removable drive; it uses the read-only LASR1 protocol.
"""
from __future__ import annotations

import argparse
import logging
from pathlib import Path
import sys
import time

from lasr_usb_protocol import FileAssembler, ProtocolError, SESSION_ID

LOG = logging.getLogger("lasr_usb_download")
FILES = ("session.json", "shots.csv", "target.json")


def read_list(link) -> list[str]:
    try:
        link.write(b"LASR1 LIST\n")
        sessions: set[str] = set()
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            line = link.readline().decode("ascii", errors="replace").strip()
            if line.startswith("!LASR|ITEM|"):
                ident = line.partition("!LASR|ITEM|")[2]
                if SESSION_ID.fullmatch(ident):
                    sessions.add(ident)
            elif line == "!LASR|LIST_END":
                return sorted(sessions)
            elif line.startswith("!LASR|ERROR|"):
                raise RuntimeError("Tab5 reported: " + line)
        raise TimeoutError("Tab5 did not finish listing sessions")
    except Exception:
        LOG.exception("Session listing failed")
        raise


def download_one(link, ident: str, name: str) -> bytes:
    try:
        for attempt in range(1, 4):
            assembler = FileAssembler()
            link.write(f"LASR1 GET {ident} {name}\n".encode("ascii"))
            deadline = time.monotonic() + 35
            try:
                while time.monotonic() < deadline:
                    result = assembler.feed(link.readline())
                    if result:
                        session, filename, data = result
                        if session != ident or filename != name:
                            raise ProtocolError("Wrong session or file returned")
                        return data
                raise TimeoutError("Timed out before verified end marker")
            except (ProtocolError, TimeoutError) as error:
                LOG.warning("%s/%s attempt %d: %s", ident, name, attempt, error)
                link.reset_input_buffer()
        raise RuntimeError("All USB transfer attempts failed")
    except Exception:
        LOG.exception("USB download failed")
        raise


def save_copy(folder: Path, name: str, data: bytes) -> Path:
    try:
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / name
        if path.exists():
            if path.read_bytes() == data:
                LOG.info("Already copied and identical: %s", path)
                return path
            for index in range(1, 1000):
                candidate = folder / f"{path.stem}-copy{index}{path.suffix}"
                if not candidate.exists():
                    path = candidate
                    break
            else:
                raise FileExistsError("Too many copies already exist")
        staging = path.with_name(path.name + ".partial")
        # An interrupted previous copy must never be mistaken for this one.
        if staging.exists():
            staging.unlink()
        with staging.open("xb") as stream:
            stream.write(data)
            stream.flush()
        staging.replace(path)
        return path
    except Exception:
        LOG.exception("Cannot save the verified LASR copy")
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description="Copy completed LASR sessions by USB-C cable")
    parser.add_argument("--port", required=True, help="Tab5 USB Serial/JTAG COM port, e.g. COM5")
    parser.add_argument("--all", action="store_true", help="Copy every completed session")
    parser.add_argument("--session", help="Copy one session-000001 identifier")
    parser.add_argument("--output", type=Path, default=Path.home() / "Downloads" / "LASR-Sessions")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    try:
        import serial  # pyserial is needed only for the real Windows COM connection.
        if args.session and not SESSION_ID.fullmatch(args.session):
            raise ValueError("--session must have form session-000001")
        with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=5) as link:
            link.reset_input_buffer()
            ids = read_list(link)
            if not args.all and not args.session:
                LOG.info("Completed LASR sessions: %s", ", ".join(ids) or "none")
                return 0
            chosen = ids if args.all else [args.session]
            for ident in chosen:
                if ident not in ids:
                    raise FileNotFoundError("Device has no verified completed session " + ident)
                for name in FILES:
                    content = download_one(link, ident, name)
                    dest = save_copy(args.output / ident, name, content)
                    LOG.info("Copied and verified %s (%d bytes)", dest, len(content))
        return 0
    except (ImportError, OSError, ValueError, RuntimeError, TimeoutError) as exc:
        LOG.error("USB session copy stopped: %s", exc)
        return 1


if __name__ == "__main__":
    sys.exit(main())