// LASR USB diagnostic schema:
// Candidate {type:"candidate", t_us, x_norm, y_norm, raw}
// FPS {type:"fps", fps_capture, fps_preview, errors, raw}
// Scan {type:"scan", frames, bright, candidates, scan_last_us, worst_us, raw}
// Status {type:"sd_status", passed, raw}
// All data originates from real Tab5 USB serial logs, never simulated hits.
export function parseDiagnosticLine(input) {
  try {
    if (typeof input !== "string" || !input.trim()) return null;
    const raw = input.trim().slice(0, 512);
    const candidate = /UNVERIFIED candidate t=(\d+)us x=([0-9.]+) y=([0-9.]+)/.exec(raw);
    if (candidate) {
      const t_us = Number(candidate[1]);
      const x_norm = Number(candidate[2]);
      const y_norm = Number(candidate[3]);
      if (!Number.isSafeInteger(t_us) ||
          !Number.isFinite(x_norm) || !Number.isFinite(y_norm) ||
          x_norm < 0 || x_norm > 1 || y_norm < 0 || y_norm > 1) return null;
      return {type:"candidate", t_us, x_norm, y_norm, raw};
    }
    const fps = /capture=(\d+(?:\.\d+)?) FPS preview=(\d+(?:\.\d+)?) FPS errors=(\d+)/.exec(raw);
    if (fps) {
      const values = fps.slice(1).map(Number);
      if (values.some(v => !Number.isFinite(v))) return null;
      return {type:"fps", fps_capture:values[0], fps_preview:values[1], errors:values[2], raw};
    }
    const scan = /CSI probe frames=(\d+) bright=(\d+) candidates=(\d+) scan_last_us=(\d+) worst_us=(\d+)/.exec(raw);
    if (scan) {
      const values = scan.slice(1).map(Number);
      if (values.some(v => !Number.isSafeInteger(v))) return null;
      return {type:"scan", frames:values[0], bright:values[1],
        candidates:values[2], scan_last_us:values[3], worst_us:values[4], raw};
    }
    if (raw.includes("CAMERA+SD TEST PASS")) {
      return {type:"sd_status", passed:true, raw};
    }
    if (raw.includes("CAMERA+SD TEST FAILED")) {
      return {type:"sd_status", passed:false, raw};
    }
    return null;
  } catch (error) {
    console.warn("LASR diagnostic log parse rejected", error);
    return null;
  }
}

export function boundedAdd(existing, value, limit = 80) {
  try {
    if (!Array.isArray(existing) || !Number.isInteger(limit) || limit < 1) return [];
    return [...existing.slice(-(limit - 1)), value];
  } catch (error) {
    console.warn("LASR diagnostic history rejected", error);
    return [];
  }
}
