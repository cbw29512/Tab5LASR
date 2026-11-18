# LASR USB-C session download — first transport implementation

**2026-11-18.** USB Serial/JTAG protocol + Windows Python client, **not flashed or proven on physical Tab5**.

## Definition of Done

When an M5Stack Tab5 is connected by a data-capable USB-C cable, a Windows laptop can query its completed microSD sessions and copy `session.json`, `target.json` and `shots.csv`, preserving the originals. No Internet, cloud, SD-card removal, Wi-Fi, or USB Mass Storage is needed. Transfers of incomplete/pending sessions and arbitrary paths must be refused. Show an error on disconnection or CRC mismatch; never claim an unverified copy.

## Architecture / choice

The existing Tab5 ESP32-P4 USB Serial/JTAG console enumerates as a Windows COM device. The LASR1 responder runs in a separate FreeRTOS task and **only exports verified completed sessions** when no session is recording. This is a read-only byte-transfer protocol, *not* a USB removable drive. The first USB method avoids changing SD ownership and avoids experimental TinyUSB Mass Storage, which otherwise requires an exclusive unmount/eject/remount state machine.

## Data and states

- Firmware session IDs: `session-NNNNNN` (six digits), filename allowlist: `session.json`, `shots.csv`, `target.json`.
- Request frames: `LASR1 LIST` or `LASR1 GET session-NNNNNN shots.csv` followed by LF.
- Responses: `!LASR|LIST_BEGIN`, `!LASR|ITEM|session-000001`, `!LASR|LIST_END`; or `!LASR|BEGIN|session-000001|shots.csv|<byte-count>`, `!LASR|DATA|<seq>|<hex-bytes>`, `!LASR|END|<CRC32-hex>`.
- Download client state: AWAIT_BEGIN -> DATA(n) -> CRC/size VERIFIED -> HOST FILE COPIED; any parse, timeout or CRC failure -> ERROR, then optional retry.
- Noise from ordinary ESP-IDF logs is ignored when not prefixed with `!LASR|`.
- The Tab5 reads only if the SD task reports READY/SAVED/TEST_OK. Recordings are not exported while active. Data is sent only to someone physically connected to the cable.

## Windows invocation (AFTER hardware validation and firmware installation)

```powershell
python -m pip install pyserial
python .\tools\LASR_USB_DOWNLOAD.py --port COM5
python .\tools\LASR_USB_DOWNLOAD.py --port COM5 --session session-000001
python .\tools\LASR_USB_DOWNLOAD.py --port COM5 --all
```

`COM5` is historical, **not guaranteed**. Confirm the actual COM port. Download defaults to `%USERPROFILE%\Downloads\LASR-Sessions`, preserves existing copies, verifies byte count and CRC32, and never deletes the microSD originals.

## Current limitations / blockers

1. Physical enumeration/console input on the user's Tab5 has **not been tested**. The desktop script and C wire encoder have passed host-level unit tests only.
2. Console logs can interleave; if corrupt, receiver rejects the file and retries. Actual throughput/timeouts must be measured.
3. USB Serial/JTAG and firmware flashing share the port; do **not** run the downloader during flashing or monitor sessions.
4. Firmware compile and 1 MiB size gate and safe app-only flashing must pass before an on-device test.
5. Actual laser detector event delivery to the microSD worker is separate future work. Files in the browser MVP are not on the Tab5.
6. Actual USB Mass Storage drive mode remains optional and requires additional exclusive-ownership verification. No such behavior is claimed.
7. No Wi-Fi session download yet: ESP32-C6 compatibility + existing oversized network camera build are unresolved in PR #6.

## Validation evidence

- `gcc` host test: rejects path traversal, validates allowlist and CRC32, streams a finalized session.
- `python -m unittest discover -s tools/tests -v`: verifies frames, rejects block loss, malformed session IDs and CRC corruption.
- Still pending: physical USB-C COM transfer, Windows receive/CRC comparison against physical microSD, disconnect/retry behavior, battery-only isolation.

## Safety

No firmware or partition flashing is authorized by adding these files. Preserve camera orientation, Color 2/4 setting, P4/P5/P7 battery guards, and private full-flash backups. No arbitrary-file or delete requests are accepted.