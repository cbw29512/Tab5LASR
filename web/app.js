import {normalizeStatus, formatNumber, shouldRetryStream, viewerState} from "./model.js";

const byId = id => document.getElementById(id);
const dom = {
  stream: byId("stream"), placeholder: byId("placeholder"),
  title: byId("placeholder-title"), description: byId("placeholder-description"),
  connection: byId("connection")
};
let streamLoaded = false;
let lastStatus = {state: "starting", wifi_state: "disconnected"};
let reconnecting = false;
let pollInFlight = false;
let lastStreamAttemptAt = 0;

function draw() {
  try {
    const state = viewerState(lastStatus, streamLoaded);
    const live = state === "live";
    dom.connection.dataset.state = state;
    dom.connection.textContent = live ? "Live · Tab5" :
      state === "error" ? "Device error" :
      state === "stale" ? "Camera stale" : "Connecting";
    dom.stream.hidden = !live;
    dom.placeholder.hidden = live;
    dom.title.textContent = state === "error" ? "Camera unavailable" :
      state === "stale" ? "Camera stream interrupted" : "Waiting for Tab5 camera";
    dom.description.textContent = lastStatus.error ??
      "Camera lens faces the target. Use this browser behind the camera.";
    byId("capture-fps").textContent = formatNumber(lastStatus.fps_capture);
    byId("stream-fps").textContent = formatNumber(lastStatus.fps_stream);
    byId("frame-age").textContent = formatNumber(lastStatus.frame_age_ms, " ms");
    byId("battery").textContent = lastStatus.battery_mv == null ?
      "—" : `${(lastStatus.battery_mv / 1000).toFixed(2)} V`;
    byId("resolution").textContent = lastStatus.width && lastStatus.height ?
      `${lastStatus.width} × ${lastStatus.height}` : "—";
    byId("dropped").textContent = lastStatus.dropped_frames ?? "—";
  } catch (error) {
    console.error("LASR viewer render failed", error);
  }
}

function openStream() {
  try {
    streamLoaded = false;
    lastStreamAttemptAt = Date.now();
    dom.stream.onload = () => { streamLoaded = true; draw(); };
    dom.stream.onerror = () => { streamLoaded = false; draw(); };
    // Video uses :81 so the long MJPEG handler never blocks status on :80.
    const streamUrl = new URL("/stream", window.location.href);
    streamUrl.port = "81";
    streamUrl.searchParams.set("reload", String(Date.now()));
    dom.stream.src = streamUrl.href;
    draw();
  } catch (error) {
    console.error("Could not open the Tab5 MJPEG stream", error);
  }
}

async function poll() {
  if (pollInFlight) return;
  pollInFlight = true;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);
  try {
    const response = await fetch("/api/status", {
      cache: "no-store", signal: controller.signal, headers: {Accept: "application/json"}
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    lastStatus = normalizeStatus(await response.json());
  } catch (error) {
    console.warn("Tab5 status request failed", error);
    lastStatus = {state: "error", wifi_state: "disconnected", error: "Tab5 is not reachable"};
    streamLoaded = false;
  } finally {
    clearTimeout(timeout);
    pollInFlight = false;
    draw();
    // Retry only when the device confirms a usable camera and C6 connection.
    if (shouldRetryStream(lastStatus, streamLoaded, Date.now() - lastStreamAttemptAt)) {
      openStream();
    }
  }
}

function reconnect() {
  if (reconnecting) return;
  reconnecting = true;
  try { openStream(); void poll(); }
  catch (error) { console.error("LASR reconnect failed", error); }
  finally { reconnecting = false; }
}
byId("reconnect").addEventListener("click", reconnect);
openStream();
void poll();
setInterval(() => { void poll(); }, 2000);
