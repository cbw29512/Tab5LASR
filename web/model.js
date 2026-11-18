// Pure viewer state contract; no browser/network access in this module.
export const STATES = Object.freeze(["starting", "live", "stale", "error"]);

export function normalizeStatus(raw) {
  try {
    if (!raw || typeof raw !== "object") throw new Error("Bad status response");
    const state = STATES.includes(raw.state) ? raw.state : "error";
    const numeric = value =>
      typeof value === "number" && Number.isFinite(value) && value >= 0 ? value : null;
    return {
      state,
      camera: typeof raw.camera === "string" ? raw.camera : "unknown",
      width: numeric(raw.width), height: numeric(raw.height),
      fps_capture: numeric(raw.fps_capture), fps_stream: numeric(raw.fps_stream),
      frame_age_ms: numeric(raw.frame_age_ms),
      dropped_frames: numeric(raw.dropped_frames), battery_mv: numeric(raw.battery_mv),
      wifi_state: typeof raw.wifi_state === "string" ? raw.wifi_state : "disconnected",
      error: typeof raw.error === "string" ? raw.error.slice(0, 150) : null
    };
  } catch {
    return {state: "error", error: "Invalid device status", wifi_state: "error"};
  }
}

export function streamHealthy(status, imageLoaded) {
  try {
    return Boolean(imageLoaded && status?.state === "live" &&
      typeof status.frame_age_ms === "number" && status.frame_age_ms <= 3000);
  } catch { return false; }
}

export function formatNumber(value, suffix = "") {
  return typeof value === "number" && Number.isFinite(value)
    ? `${value.toFixed(1)}${suffix}` : "—";
}
