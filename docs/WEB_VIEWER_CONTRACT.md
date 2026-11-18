# LASR web camera: browser is the primary display

## Definition of Done (M2)

The Tab5 camera lens is on the **same side as its screen**. In a shooting setup the lens faces a wall/paper target, which also makes the Tab5's screen face the target rather than the shooter. Consequently the shooter controls the system from a browser on a phone, tablet or PC; the Tab5 screen is for local diagnostics and battery recovery.

**M2 is complete only when a real Tab5 onboard CSI camera feed appears in a LAN browser**, with measured latency/FPS and battery-only operation. An HTML mock, a working build, or a passing HTTP API test alone do not satisfy M2.

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

The browser **does not** use its own camera. No cloud, no external relay, no login, no account or remote internet access; home-LAN only. Avoid exposing HTTP camera feed to the public Internet.

### Important constraints, not assumptions

- Installed Tab5 factory app partition: **0x10000 / 1,048,576 bytes**. Working camera firmware occupies **about 948 KB**. Adding hosted Wi-Fi, JPEG encoding and an HTTP server may exceed this hard limit.
- A public Espressif `esp_video` simple video-server example uses a larger app partition. **Do not flash an oversized image; do not erase/repartition the physical device as a shortcut.** A deliberate independent migration/recovery plan is needed if unavoidable.
- ESP32-P4 has **no native Wi-Fi radio**. Tab5 ESP32-C6 supplies Wi-Fi through SDIO using a compatible `esp-hosted` host/slave pair. Factory ESP32-C6 images can be older/incompatible. **Don't flash the C6 without a separately verified C6 backup and compatibility plan.**
- Battery power and charging controls are locked to the previously tested P4/P5/P7 settings and supervised recovery implementation. LAN streaming adds load: verify battery-only + camera + Wi-Fi together.
- First web version supports **one viewing client** with bounded buffering. No multiple-client, recording or background network telemetry yet.
- The picture has been upright since the 180-degree fix. Color calibration is ongoing. The HTTP stream must retain the known-good orientation; never confuse display-only color modes with raw detection buffers.
- The laser appears on the **wall target**, and the camera sees it; the shooter uses the browser to locate the dot even when the Tab5 screen cannot be seen.

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
- `GET /api/status`: JSON matching `ViewerStatus` and header `Cache-Control: no-store`.
- `GET /stream`: `multipart/x-mixed-replace; boundary=frame` where each part is `image/jpeg`. One client maximum in first cut.
- `GET /snapshot.jpg` (optional): latest full JPEG for connection diagnostics.
- HTTP dashboard is accessible only after Wi-Fi association and routing. No hardcoded fake URL/IP in the webpage.

## Test acceptance

- Camera physically faces a target, **browser is behind the camera with shooter**.
- Verify stream from a browser on phone/PC on same Wi-Fi; inspect an actual moving scene.
- Browser displays disconnect/reconnect states without showing a false LIVE badge.
- Capture FPS, HTTP FPS, frame age, drops, source orientation and client latency measured.
- NP-F550 battery-only operation with camera + Wi-Fi + browser viewer active.
- C6 factory firmware compatibility logged; no silent C6 firmware changes.
- No data uploaded or sent outside home LAN; no exposure through Netlify or public hosting.

Reference implementation patterns: Espressif `esp_video/examples/simple_video_server` for JPEG/MJPEG and `esp_wifi_remote` / `esp_hosted` for ESP32-P4 Wi-Fi. They require adaptation to the Tab5 BSP and confirmed firmware layout.
