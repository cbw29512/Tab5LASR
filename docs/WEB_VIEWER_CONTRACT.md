# LASR web viewer: OPTIONAL companion to portable offline Tab5

**Product priority changed:** See [PORTABLE_STANDALONE_CONTRACT.md](PORTABLE_STANDALONE_CONTRACT.md). The Tab5 MUST work independently on its own battery, screen and onboard camera with no phone, computer, router or web viewer. The web viewer is a secondary convenience; it is not permitted to block offline scoring or session recording.

## Definition of Done (M2)

The current factory Tab5 camera lens is on the **same side as its screen**, so the existing mounting arrangement cannot show its own screen to a shooter while looking at the wall target. Rear-facing camera relocation or an optical redirect is a required hardware feasibility investigation; the existing browser viewer temporarily helps test the front-camera setup, but **must never be required for the finished portable product**.

**Optional web milestone**: A real Tab5 onboard CSI feed appears in a browser when Wi-Fi is available, with measured latency/FPS and battery-only operation. No HTML mock, passing build, or HTTP API unit test alone satisfies the milestone. The separate **primary offline product DoD** is defined in `PORTABLE_STANDALONE_CONTRACT.md`.

### Hardware architecture

```text
Paper target + SIRT laser
          |
          v  (front camera lens points AT target)
 Tab5 onboard CSI sensor -> bounded frame ownership
          |                    |
          |             local preview + battery safeguards
          v
   JPEG encoding (measure CPU/latency)
          |
          v
 ESP32-P4 LAN HTTP server -> ESP-Hosted SDIO -> onboard ESP32-C6 Wi-Fi
          |                                        |
          +----------------------------------------+
                      home Wi-Fi (2.4 GHz)
                                  |
                       browser on PC/phone
                         live feed + FPS
```

The browser **does not** use its own camera. An offline session never waits for Wi-Fi or a browser. In the first standalone experiment, Tab5 creates its own password-protected Wi-Fi hotspot so it can operate without a router (IP `192.168.4.1` only after successful C6 bring-up). No cloud, no external relay, no login, no account or remote internet access; home-LAN only. Avoid exposing HTTP camera feed to the public Internet.

### Important constraints, not assumptions

- Installed Tab5 factory app partition: **0x10000 / 1,048,576 bytes**. Working camera firmware occupies **about 948 KB**. Adding hosted Wi-Fi, JPEG encoding and an HTTP server may exceed this hard limit.
- A public Espressif `esp_video` simple video-server example uses a larger app partition. **Do not flash an oversized image; do not erase/repartition the physical device as a shortcut.** A deliberate independent migration/recovery plan is needed if unavoidable.
- ESP32-P4 has **no native Wi-Fi radio**. Tab5 ESP32-C6 supplies Wi-Fi through SDIO using a compatible `esp-hosted` host/slave pair. Factory ESP32-C6 images can be older/incompatible. **Don't flash the C6 without a separately verified C6 backup and compatibility plan.**
- Battery power and charging controls are locked to the previously tested P4/P5/P7 settings and supervised recovery implementation. LAN streaming adds load: verify battery-only + camera + Wi-Fi together.
- First web version supports **one viewing client** with bounded buffering. No multiple-client, recording or background network telemetry yet.
- The picture has been upright since the 180-degree fix. Color calibration is ongoing. The HTTP stream must retain the known-good orientation; never confuse display-only color modes with raw detection buffers.
- The laser appears on the **wall target**, and the camera sees it; the optional browser is a diagnostic/spectator viewer. The finished device must have the screen visible to the shooter while the camera views the target.

## Data schemas and state machines

```text
ViewerStatus {
  camera: "onboard_csi",
  state: "starting" | "live" | "stale" | "error",
  width: integer | null,
  height: integer | null,
  fps_capture: number | null,
  fps_stream: number | null,
  frame_age_ms: number | null,
  dropped_frames: integer | null,
  battery_mv: integer | null,
  charger_command_on: boolean | null,
  wifi_state: "disconnected" | "connecting" | "connected" | "error",
  error: string | null
}

FutureShotEvent {
  sequence: integer,
  timestamp_ms: integer,
  x_norm: number, y_norm: number,
  brightness: number,
  hit_zone: string | null
}
```

Camera frame lifecycle: one capture owner -> independent bounded JPEG copy for HTTP; never send borrowed V4L2 mmap memory after QBUF. Slow clients must drop stale frames, never stall acquisition. Keep shot detection on the raw frame path, not the display-color correction path.

Network state: OFF -> C6_POWERED -> HOSTED_UP -> WIFI_CONNECTING -> LAN_READY | ERROR. Viewer state: CONNECTING -> LIVE -> STALE | ERROR, with reconnect.

## HTTP contract (target routes, not currently running)

- `GET /`: responsive LAN dashboard from the same origin as live feed.
- `GET :80/api/status`: JSON matching `ViewerStatus` and header `Cache-Control: no-store`.
- `GET :81/stream`: `multipart/x-mixed-replace; boundary=frame` where each part is `image/jpeg`. Uses a dedicated server task, same device hostname on port 81; one client maximum.
- `GET /snapshot.jpg` (optional): latest full JPEG for connection diagnostics.
- HTTP dashboard is accessible only after Wi-Fi association and routing. The ESP32-C6 WPA2 hotspot uses a 12-character secret generated once and saved to NVS; it is never committed to GitHub or sent to the cloud. Host firmware compatibility is unverified. No hardcoded fake URL/IP in the webpage.

## Test acceptance

- Camera physically faces a target, **browser is behind the camera with shooter**.
- Verify stream from a browser on phone/PC on same Wi-Fi; inspect an actual moving scene.
- Browser displays disconnect/reconnect states without showing a false LIVE badge.
- Capture FPS, HTTP FPS, frame age, drops, source orientation and client latency measured.
- NP-F550 battery-only operation with camera + Wi-Fi + browser viewer active.
- C6 factory firmware compatibility logged; no silent C6 firmware changes.
- No data uploaded or sent outside home LAN; no exposure through Netlify or public hosting.

Reference implementation patterns: Espressif `esp_video/examples/simple_video_server` for JPEG/MJPEG and `esp_wifi_remote` / `esp_hosted` for ESP32-P4 Wi-Fi. They require adaptation to the Tab5 BSP and confirmed firmware layout.
