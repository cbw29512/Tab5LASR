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

`GET /api/status` JSON; `GET /stream` continuous JPEG multipart. The control API uses the page's origin (:80); MJPEG uses the **same Tab5 host on port 81** because streaming occupies a dedicated HTTP server task. No hardcoded remote address, secrets, public hosting or phone-camera permission. See `docs/WEB_VIEWER_CONTRACT.md`.

## Physical firmware constraints

Existing installed app has only ~100 KB free in factory 1 MiB partition. Wi-Fi uses separate onboard ESP32-C6 over SDIO. Do not flash a larger image or C6 firmware without a deliberate, independently validated recovery process. Normal battery operation is required with streaming enabled.
