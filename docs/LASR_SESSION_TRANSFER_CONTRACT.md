# LASR session export via Wi-Fi or USB-C — implementation contract
Date: 2026-11-18
Status: design approved by user request, NOT implemented, NOT hardware-tested.

## Definition of Done
From the Tab5 touchscreen, the user can browse previously SAVED sessions and copy a selected session to a Windows laptop using either (A) an offline Wi-Fi browser download or (B) the device's USB-C data port. Neither method requires removing the microSD card or an Internet connection. Original data on the Tab5 is retained after transfer by default. Shooting, saving and reviewing sessions remains fully offline without Wi-Fi, USB or laptop.

## Platform evidence
- M5Stack Tab5 specifications: ESP32-P4, companion ESP32-C6 for 2.4 GHz Wi-Fi, microSD, **USB-A HOST and USB-C 2.0 OTG**.
  https://docs.m5stack.com/en/core/Tab5
- ESP-IDF ESP32-P4 USB device stack demonstrates TinyUSB Mass Storage Class (MSC) using an SD/MMC medium. Actual Tab5 USB-C port routing, pinned IDF/BSP compatibility and USB enumeration must still be tested before promising a working removable drive.
  https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/usb_device.html
- Existing LAN MJPEG/ESP-Hosted Wi-Fi PR #6 is not yet flashable: binary exceeds installed factory app partition and C6 compatibility is unverified. A **small HTTP session downloader** may eventually be lighter than the camera stream, but is NOT proven to fit or operate until compiled and hardware tested.
  https://github.com/cbw29512/Tab5LASR/pull/6

## State and data schema (before implementation)
```text
TransferMode = IDLE | WIFI_STARTING | WIFI_READY | WIFI_TRANSFERRING |
               USB_PREPARING | USB_EXPORTED | USB_WAIT_EJECT |
               USB_REMOUNTING | ERROR

SessionFile {
  session_id: string (validated immutable ID)
  file_name: "session.json" | "shots.csv" | "target.json"
  byte_size: unsigned integer
  sha256: lowercase hex digest | null
  is_complete: boolean
}

TransferStatus {
  mode: TransferMode
  session_id: string | null
  bytes_sent: unsigned integer
  total_bytes: unsigned integer | null
  storage_mounted_on_tab5: boolean
  usb_host_owns_card: boolean
  verified: boolean
  last_error: string | null
}
```

## Workflow A — offline Wi-Fi download (preferred)
1. On Tab5, open **Sessions > Transfer > Wi-Fi**. Show local SSID, safe connection instructions and a user-authorized, time-limited transfer session. Never display or transmit Wi-Fi password in public logs or repository.
2. User connects a laptop/phone to the Tab5's *local* hotspot (or same permitted LAN) and opens the on-device browser page. No cloud or NucBox required.
3. Device lists saved, completed sessions with shot count, date if a reliable clock is available, and target name. Missing card presents an explicit error, not an empty false history.
4. Select a session; browser downloads **session ZIP** (manifest, JSON, CSV, target snapshot) or just **CSV**. If zip streaming is too costly, serve individual files first and defer archive bundling.
5. Response must use safe Content-Disposition filenames, bounded chunking, no path traversal, Content-Length when known, Cache-Control: no-store; optionally compute SHA-256 and confirm bytes.
6. Download is a **copy**. Tab5 retains its microSD files. A lost Wi-Fi connection does not alter session data or stop offline shooting.
7. Limit one active transfer client initially; use authenticated local access (WPA2 plus app-level authorization), no public Internet listener, and no unrestricted arbitrary filesystem API.

Proposed endpoints (not running):
- GET /api/sessions -> {sessions: [...]}
- GET /api/sessions/{id}/files -> {files:[...]}
- GET /api/sessions/{id}/download?format=csv|zip -> binary attachment
- GET /api/transfers/status -> TransferStatus

## Workflow B — USB-C removable-drive mode
1. On Tab5, **finish and verify session saves**, then select **Sessions > Transfer > USB-C**.
2. Block new recording and writes; flush pending journals, close files, and safely **unmount** the microSD filesystem from the Tab5.
3. Only when storage is exclusively released, activate the USB-C device Mass Storage Class using the verified Tab5 board wiring and matching ESP-IDF/TinyUSB APIs.
4. Windows may then mount the microSD as a removable drive; the user copies files from /LASR/sessions/ via File Explorer.
5. User clicks **Eject/Safely Remove** on Windows; Tab5 waits for USB host release, disables MSC and remounts the filesystem on Tab5.
6. Verify filesystem mount, read access and metadata before showing **READY TO SHOOT**.
7. If host does not eject, continue showing **USB TRANSFER MODE — SHOOTING DISABLED**; no simultaneous Tab5 FAT writes and Windows MSC writes, even in an error/reconnect case.
8. Exiting USB mode cannot silently format or overwrite the card. Preserve battery/charger/recovery paths and USB flashing access.

If physical USB-C MSC support proves impractical, a deliberate alternative is USB CDC-based **file transfer with an explicit Windows program/PowerShell script**; distinguish it from true drag-and-drop drive support. Do not claim serial port alone exposes files.

## Device UI
- Sessions list: target, shots, final score, elapsed time, storage state.
- Transfer buttons: **Download via Wi-Fi**, **USB-C Transfer Mode**, **Back**.
- Wi-Fi: connection information, clients, status/progress, **Cancel Transfer**.
- USB: full-screen notice **CARD EXPORTED TO WINDOWS — SHOOTING PAUSED; EJECT IN WINDOWS TO FINISH**.
- After copy, **Keep on Tab5** by default. Optional future **Delete Session** must require explicit confirmation and separate handling, never auto-delete after download.

## Safety/acceptance gate
- Do not flash or modify the currently working camera firmware, factory partition table, P4/P5/P7 power/charging controls, Color 2/4 display setting, or companion ESP32-C6 until separately verified recovery/compatibility gates are met.
- Validate a file's bytes/hash before and after USB/Wi-Fi transfer using a real microSD card.
- 100 repeated USB mount/unmount/eject cycles without corruption; unexpected cable removal, Windows write attempts, card removal, and brownout; preserve pre-existing files.
- Verify Windows enumerates the USB device on a real data-capable USB-C cable. Device port, charging, and battery-only behavior must be measured on hardware.
- Disconnect Wi-Fi midway and resume normal offline shooting; transferred files must remain intact.
- Never show success based solely on HTTP 200, a browser download dialog, or USB enumeration; file transfer confirmation requires actual readback / host-side integrity where measurable.
- Keep capture and scoring timing isolated from slow card I/O and network transfer. For initial MVP, restrict transfers to completed sessions to avoid timing disruption.

## Milestones
1. On-device session persistence and verified microSD writer (separate storage contract).
2. Offline session browser and filename-safe exports; minimal HTTP endpoint gated behind proven hosted Wi-Fi.
3. USB-C MSC feasibility on real board with separate card ownership state machine.
4. End-to-end physical acceptance and optional app integration.

**Current status:** Documented for implementation. Neither transfer method is running on the current Tab5 firmware.
