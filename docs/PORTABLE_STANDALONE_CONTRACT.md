# LASR portable standalone contract — PRIMARY PRODUCT DEFINITION

## Definition of Done

One M5Stack Tab5 is carried to any indoor target location, mounted using
its tripod thread, and used on battery **without a computer, phone, router,
NucBox, Internet connection, external display, or cloud service**. The user
can use the Tab5 **touchscreen** to start/stop a session, calibrate a paper
target, see hits/scores, and review/save the session. A web viewer may
optionally mirror the session, but is never necessary to use the product.

The **camera must face the target** while the **Tab5 screen faces the shooter**.
The factory camera and display are on the same face, so this physical requirement
is **not met** by the present casing. The team must validate a rear-facing
optical mounting solution before advertising the device as self-contained.

Current prototype camera output/color is NOT calibrated. User prefers
**COLOR 2/4 (red/blue swap)** for display; this is not a raw sensor correction.

## Physical design investigations (no purchase or case modification approved)

A Tab5 teardown confirms the MIPI-CSI camera **FPC cable is detachable**.
This makes relocating the original camera module physically plausible, but
does **not** verify the connector pin-count, pitch, contact orientation,
allowable cable length or MIPI signal integrity. A longer replacement
flex cable must only be selected after examining the actual connector,
cable and camera-module mounting on the owned hardware. Do not substitute
a generic Raspberry Pi camera/ribbon or assume compatibility.

Investigate, in order:

1. Non-destructive rearward optical path (mirror/prism) and its target
   parallax/image reversal, if mechanically practical; this preserves wiring.
2. A rear-facing custom bracket/case holding the same original camera module
   and a correctly matched FPC cable, with lens opening and strain relief.
   Keep NP-F550 battery access, charging airflow/clearance, M5-Bus,
   1/4-inch tripod thread and the existing power/recovery button accessible.
3. Separate rear camera module only if original sensor relocation fails;
   requires verified CSI interface, driver support, power, and signal path.

**No physical alteration before preserving the working camera and battery
state and confirming a reversible assembly plan.**

## Product operation

- No Wi-Fi dependency for camera capture, target calibration, shot detection,
  scoring, session history or display; local Wi-Fi is optional for sharing
  results and remote spectator viewing.
- No PC or phone needed for initial setup or operation; screen must remain
  visible while the lens points at the wall/paper target.
- Camera raw frame detection is independent of display-only Color 2 transform.
- Session file storage: microSD preferred, with explicit mount/save errors and
  an on-screen indication of unsaved results; no phantom persistent storage.
- Real-time performance measured on actual battery hardware; camera FPS,
  detection latency, dot localization error, duplicate-shot count, false-positive
  rate, and battery runtime are acceptance data, not invented promises.
- Shot trigger is transition from absent to present; require observed laser-off
  before rearming. Exact threshold and debounce determined with real SIRT data.
- Local screen must retain the guarded charger/battery recovery controls.

## Data schema (design contract, not implemented)

\`\`\`text
StandaloneSession {
  id: local monotonically allocated identifier
  state: "idle"|"calibrating"|"armed"|"active"|"paused"|"complete"|"error"
  started_at_ms: integer | null
  target_bounds_normalized: [left, top, right, bottom] | null
  total_shots: nonnegative integer
  hits: nonnegative integer
  storage_state: "unknown"|"ready"|"saving"|"saved"|"error"
  last_error: string | null
}

LaserDetector {
  state: "uninitialized"|"calibrating"|"armed"|"laser_present"|"rearming"|"error"
  threshold_profile: physical-calibration-data | null
  last_dot: [x_normalized, y_normalized, intensity] | null
  last_frame_timestamp_us: integer | null
  rejected_blobs: nonnegative integer
}

SessionEvent {
  shot_sequence: positive integer
  timestamp_ms: integer
  x_normalized: float in [0,1]
  y_normalized: float in [0,1]
  zone: string | null
  score: integer | null
  raw_confidence: nonnegative float
}
\`\`\`

## State transitions

On boot -> IDLE (no network needed). Calibrate -> CALIBRATING -> ARMED
after confirmed target lock. First ON transition -> ACTIVE and records a
single shot -> laser PRESENT; wait for laser OFF -> REARMING -> ARMED.
Pause is always available; complete triggers a local save. Storage errors
never falsely report a successful save. Camera errors show a diagnostic
instead of fake scores. Optional Wi-Fi failure never terminates a live
offline shooting session.

## Delivery milestones

1. Preserve existing CSI, battery, Color 2 startup, backups and flash gate.
2. Physically validate lens orientation and rear-face mounting/optical path.
3. Implement/calibrate offline laser detector and in-device scoring display.
4. Implement verified microSD session saving and browsing.
5. Add optional ESP32-C6 local network viewer/phone companion.
6. Physical field test: boot disconnected, target facing away from user,
   screen towards shooter, multiple shots, battery-only, save/review offline.

## Evidence

- M5Stack Tab5 product specifications: MIPI-CSI 2MP front-facing camera,
  NP-F550 battery option, microSD and tripod mounting.
- CNX Software May 2025 Tab5 teardown: detached camera FPC cable.
- GitHub hardware camera/battery validations and permanently verified private
  dice-roller and LASR-camera backup images.
