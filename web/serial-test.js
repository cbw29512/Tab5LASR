import {parseDiagnosticLine, boundedAdd} from "./serial-model.js";
import {renderCandidates} from "./serial-plot.js";

// State first: USB serial port owns the stream, with bounded raw lines and
// candidates. No invented camera feed, shot score, or persistent sessions.
const state = {
  port:null, reader:null, active:false, buffer:"", lines:[], candidates:[],
  fps:null, scan:null, sd:null, lastActivity:null
};
const el = id => document.getElementById(id);
const view = {
  connect:el("connect"), disconnect:el("disconnect"), status:el("connection"),
  markers:el("hit-markers"), empty:el("empty-target"), shots:el("shots"),
  log:el("serial-log"), activity:el("last-activity")
};
function status(text, mode) {
  view.status.textContent = text;
  view.status.dataset.state = mode;
  view.connect.disabled = state.active;
  view.disconnect.disabled = !state.active;
}
function draw() {
  try {
    el("fps-capture").textContent = state.fps ? `${state.fps.fps_capture.toFixed(1)} FPS` : "—";
    el("fps-preview").textContent = state.fps ? `${state.fps.fps_preview.toFixed(1)} FPS` : "—";
    el("scan-time").textContent = state.scan ? `${(state.scan.scan_last_us / 1000).toFixed(1)} ms` : "—";
    el("scan-worst").textContent = state.scan ? `${(state.scan.worst_us / 1000).toFixed(1)} ms` : "—";
    el("bright-frames").textContent = state.scan?.bright ?? "—";
    el("sd-status").textContent = state.sd === null ? "Not reported" : state.sd ? "Synthetic PASS" : "FAILED";
    view.shots.textContent = `${state.candidates.length} candidate events (recent)`;
    view.activity.textContent = state.lastActivity ? `Last serial data ${state.lastActivity}` : "No serial data";
    view.log.textContent = state.lines.length ? state.lines.join("\n") : "No diagnostic lines received.";
    renderCandidates(view.markers, view.empty, state.candidates);
  } catch (error) {
    console.error("LASR USB diagnostic rendering failed", error);
  }
}
function acceptLine(input) {
  try {
    const line = input.replace(/\x1b\[[0-9;]*m/g, "").trim();
    if (!line) return;
    state.lastActivity = new Date().toLocaleTimeString();
    const item = parseDiagnosticLine(line);
    if (item?.type === "candidate") state.candidates = boundedAdd(state.candidates, item, 45);
    if (item?.type === "fps") state.fps = item;
    if (item?.type === "scan") state.scan = item;
    if (item?.type === "sd_status") state.sd = item.passed;
    if (item || /lasr_|CAMERA\+SD|MIPI CSI|SD diagnostic/i.test(line)) {
      state.lines = boundedAdd(state.lines, line.slice(0, 280), 75);
    }
    draw();
  } catch (error) {
    console.error("LASR USB diagnostic line handling failed", error);
  }
}
function consume(chunk) {
  try {
    state.buffer += chunk;
    // Prevent malformed/unterminated serial output from growing without bound.
    if (state.buffer.length > 8192) {
      state.buffer = state.buffer.slice(-8192);
      console.warn("LASR serial line exceeded input buffer; dropped old bytes");
    }
    let end;
    while ((end = state.buffer.indexOf("\n")) >= 0) {
      const line = state.buffer.slice(0, end);
      state.buffer = state.buffer.slice(end + 1);
      acceptLine(line);
    }
  } catch (error) {
    console.error("LASR serial data decoding failed", error);
  }
}
async function readSerial() {
  const decoder = new TextDecoder();
  try {
    state.reader = state.port.readable.getReader();
    while (state.active) {
      const {value, done} = await state.reader.read();
      if (done) break;
      if (value) consume(decoder.decode(value, {stream:true}));
    }
  } catch (error) {
    if (state.active) {
      console.error("Tab5 USB serial connection failed", error);
      status("USB read error", "error");
    }
  } finally {
    try { state.reader?.releaseLock(); }
    catch (error) { console.warn("Could not release Tab5 serial reader", error); }
    state.reader = null;
    try { await state.port?.close(); }
    catch (error) { console.warn("Could not close Tab5 serial port", error); }
    state.port = null;
    state.active = false;
    if (view.status.dataset.state !== "error") status("Disconnected", "idle");
    view.connect.disabled = false;
    view.disconnect.disabled = true;
  }
}
async function connect() {
  try {
    if (!("serial" in navigator)) {
      throw new Error("Use Chrome or Edge on localhost. Web Serial is unavailable here.");
    }
    if (state.active) return;
    const port = await navigator.serial.requestPort();
    await port.open({baudRate:115200});
    state.port = port;
    state.buffer = "";
    state.active = true;
    status("Connected · Tab5 USB", "connected");
    void readSerial();
  } catch (error) {
    console.error("Could not connect Tab5 USB serial port", error);
    status(`Connection failed: ${error.message}`, "error");
  }
}

async function disconnect() {
  try {
    state.active = false;
    if (state.reader) await state.reader.cancel();
    else if (state.port) await state.port.close();
    status("Disconnected", "idle");
  } catch (error) {
    console.error("Could not disconnect Tab5 USB serial port", error);
    status("Disconnect error", "error");
  }
}

view.connect.addEventListener("click", () => { void connect(); });
view.disconnect.addEventListener("click", () => { void disconnect(); });
el("clear").addEventListener("click", () => {
  state.lines = []; state.candidates = []; state.fps = null; state.scan = null;
  state.sd = null; state.lastActivity = null; draw();
});
el("copy").addEventListener("click", async () => {
  try {
    await navigator.clipboard.writeText(state.lines.join("\n"));
    el("copy").textContent = "Copied";
  } catch (error) {
    console.error("Could not copy LASR USB diagnostics", error);
    el("copy").textContent = "Copy failed";
  }
});
draw();
