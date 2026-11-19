# Browser-first camera viewer

The Tab5 lens and screen are on the same face. Mount the Tab5 with its lens facing the paper target; the shooter uses this page from a phone or computer behind the device.

## Status

**The web frontend is implemented, but no Tab5 network server is installed or claimed live yet.** On its own, serving `web/` via a local file/web server shows a CONNECTING or ERROR state, not a simulated picture. Native ESP32-P4 Wi-Fi and MJPEG camera transport must pass compilation, C6 compatibility and battery testing before a physical live URL exists.

## Entry points

- `index.html`: accessible browser interface and diagnostics
- `app.js`: asynchronous browser status polling and MJPEG image integration
- `model.js`: pure schema normalization, stale frame gating
- `styles.css`: responsive mobile/desktop viewer
- `tests/model.test.mjs`: Node.js built-in tests (run `node --test web/tests/*.test.mjs` from repo root)

## API to be served by onboard P4 firmware

`GET /api/status` JSON; `GET /stream` continuous JPEG multipart. All paths **same origin**; no hardcoded remote address, secrets, public hosting or phone-camera permission. See `docs/WEB_VIEWER_CONTRACT.md`.

## Physical firmware constraints

Existing installed app has only ~100 KB free in factory 1 MiB partition. Wi-Fi uses separate onboard ESP32-C6 over SDIO. Do not flash a larger image or C6 firmware without a deliberate, independently validated recovery process. Normal battery operation is required with streaming enabled.

## Physical USB laser test — implemented without Tab5 Wi-Fi

Use `tools/OPEN_LASR_USB_TEST.ps1` from Windows PowerShell to host
`web/serial-test.html` at `http://127.0.0.1:8765/serial-test.html`.
The launcher checks the local page and starts a loopback-only Python file
server. It does **not** flash the Tab5. Desktop Chrome/Edge use the user-
approved Web Serial port to read the existing real ESP32-P4 USB log.

1. Connect Tab5 via USB; close every other serial monitor or flash utility.
2. Open the page, click **Connect Tab5 USB** and choose the connected port.
3. On the Tab5 screen, enable **LASER SCAN ON** with its camera aimed at a
   laser-free paper target, then test with a non-firing SIRT trainer.
4. `UNVERIFIED candidate t=...us x=... y=...` serial lines plot raw camera
   coordinates. Scanner and CSI FPS diagnostics appear only when emitted.
5. Copy the diagnostic log and inspect camera capture/preview FPS.

**No mock shots or remote video:** The USB page displays *only* actual
serial events. It cannot control the Tab5 scan toggle, access live CSI pixels,
save calibrated scores or sessions, and does not transmit a camera feed.
A working phone/PC **Wi-Fi live video viewer** still requires independently
verified Tab5 ESP32-C6/SDIO networking, JPEG encoding, camera frame ownership,
and a safe app size/recovery plan. The current 1 MiB diagnostic is too
close to full for adding those features without a separate migration.
