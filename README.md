# Tab5 LASR

Portable, offline-first **laser dry-fire target simulator** using an M5Stack Tab5, a USB UVC camera and a non-firing SIRT training pistol.

**Project status: planning / camera-first implementation.** The GitHub repository is public, but **device backups must be private**. No working Tab5 camera preview, web stream or laser detector is claimed yet.

## Development order — locked

1. **M1: USB camera on Tab5.** Enumerate a UVC webcam, negotiate MJPEG, show live frames on the Tab5 display, measure captured and displayed FPS, log USB failures and handle unplug/replug.
2. **M2: Web camera viewer.** Host a local browser page and MJPEG stream from the Tab5 so a PC or phone on the same Wi-Fi can view exactly the camera connected to the Tab5.
3. **M3: SIRT detection and targets.** Detect shot-laser rising edges and register paper target zones; only then add hit/miss scoring.

Do **not** implement target detection or scoring before M1/M2 camera acceptance.

## Mandatory pre-flash safety gate

Do not flash the currently functioning dice-roller Tab5 until an exact, **full-device, read-only 16 MiB flash backup** has been read twice and hashes match, and the verified copy is stored permanently and privately (not as a temporary GitHub Actions artifact). Preserve local archive, SHA-256 manifest, board details, restore instructions, and successful download-and-verify from the private backup destination.

The installed dice-roller firmware, source and sensitive device flash are **not** part of this public repository. Refer to [the existing dice roller](https://github.com/cbw29512/nothingbutadiceroller) for its battery recovery and flash compatibility contracts. Keep its charger/battery safeguards when adapting any firmware; retain original bootloader/partition tables and do not assume application-only flashing is safe without matching layout verification.

## Definition of Done (first two milestones)

**M1 camera:**
- USB UVC MJPEG webcam physically connected to Tab5 USB-A port; actual camera model and VID:PID documented
- Preview visible on Tab5 screen with consistent orientation and recoverable USB disconnect
- Capture FPS and screen-render FPS measured independently over a defined observation interval; report dropped frames and timeouts
- Test at 320x240 and a higher advertised camera mode only when enumeration supports it; don't claim 60/120 FPS without measurement
- No changes to existing dice-roller firmware prior to backup verification; regression check USB-to-battery handoff

**M2 browser viewer:**
- Same camera frames accessible as MJPEG over the local network, with local status endpoint
- Browser shows live/connecting/error states, source resolution, measured capture/stream FPS, and latest-frame age
- One client supported initially; multiple clients a separate performance test
- LAN-only default, no cloud upload, accounts, recordings or internet exposure
- Handles camera detach, Wi-Fi dropout, and browser reconnect without crashing; never reports simulated imagery as live

## Data schema — before code

Camera source: `CameraConfig { source: "usb_uvc", vid?: string, pid?: string, pixel_format: "MJPEG", width: uint, height: uint, requested_fps: uint }`

Internal frame: `CameraFrame { sequence: uint64, capture_monotonic_ms: uint64, width: uint, height: uint, jpeg_bytes: bytes }` — bounded buffers only; keep network and display processing from blocking USB capture.

Status response: `CameraStatus { state: CameraState, source: string|null, width: uint|null, height: uint|null, fps_captured: number, fps_displayed: number, fps_streamed: number, dropped_frames: uint, latest_frame_age_ms: uint|null, error_code: string|null }`

`CameraState = DISCONNECTED | PROBING | NEGOTIATING | STREAMING | ERROR`.

Web state: `ViewerState = CONNECTING | LIVE | STALE | DISCONNECTED | ERROR`.

## Planned local API (M2; not yet implemented)

- `GET /api/camera/status` → `CameraStatus` JSON
- `GET /api/camera/stream` → `multipart/x-mixed-replace; boundary=frame`, each part `image/jpeg`
- `GET /` → camera dashboard hosted by the same device as the stream, to avoid HTTPS/mixed-content problems

## Build and runtime flow

USB UVC source → bounded JPEG frame queue → **two consumers**: (1) Tab5 preview (2) Wi-Fi MJPEG HTTP stream → browser. No browser camera permissions required: the browser views frames served by Tab5, not the phone/PC's webcam.

For M1 start from Espressif's ESP-IDF USB UVC host example and Tab5 BSP. A community Tab5 USB-UVC demo reports 320x240 at 15 FPS, **not** proof of 60/120 FPS on this hardware.

## References

- [Official M5Stack Tab5 board documentation](https://docs.m5stack.com/en/core/Tab5)
- [ESP-IDF USB Host UVC example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/uvc)
- [Tab5 USB webcam preview proof-of-concept](https://github.com/Gobbbobb/m5stack_tab5_usb_host_uvc)
- [Espressif USB camera HTTP preview example](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_hub_dual_camera)

**Acceptance still pending:** camera model, physical FPS and stream behavior, verified full-flash backup, and physical battery testing.
