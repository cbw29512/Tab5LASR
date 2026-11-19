import test from "node:test";
import assert from "node:assert/strict";
import {parseDiagnosticLine, boundedAdd} from "../serial-model.js";

test("real Tab5 candidate log yields raw unverified coordinates", () => {
  const hit = parseDiagnosticLine("W (123) lasr_csi_probe: UNVERIFIED candidate t=531000us x=0.3750 y=0.6250 (not recorded)");
  assert.deepEqual(hit && [hit.type,hit.t_us,hit.x_norm,hit.y_norm],
    ["candidate",531000,0.375,0.625]);
});
test("invalid or absent candidate coordinates are not fabricated", () => {
  assert.equal(parseDiagnosticLine("UNVERIFIED candidate t=42us x=1.20 y=0.3"),null);
  assert.equal(parseDiagnosticLine("UNVERIFIED candidate t=42us x=-1 y=0.3"),null);
  assert.equal(parseDiagnosticLine("laser visible but no candidate"),null);
  assert.equal(parseDiagnosticLine(null),null);
});
test("actual FPS and scanner timing logs are parsed independently", () => {
  const fps = parseDiagnosticLine("I lasr_csi: MIPI CSI 640x480 capture=14.3 FPS preview=14.3 FPS errors=0");
  const scan = parseDiagnosticLine("I lasr_csi_probe: CSI probe frames=120 bright=3 candidates=2 scan_last_us=12500 worst_us=18200");
  assert.deepEqual([fps.type,fps.fps_capture,fps.fps_preview,fps.errors],["fps",14.3,14.3,0]);
  assert.deepEqual([scan.type,scan.scan_last_us,scan.worst_us,scan.candidates],["scan",12500,18200,2]);
});
test("storage label is explicitly synthetic, not a scored shot", () => {
  assert.deepEqual(parseDiagnosticLine("CAMERA+SD TEST PASS: synthetic SD verified, CSI still streaming").passed,true);
  assert.deepEqual(parseDiagnosticLine("CAMERA+SD TEST FAILED: SD error").passed,false);
});
test("bounded serial and candidate histories drop oldest entries", () => {
  const history = [1,2,3];
  assert.deepEqual(boundedAdd(history,4,3),[2,3,4]);
  assert.deepEqual(boundedAdd(history,4,1),[4]);
  assert.deepEqual(boundedAdd([],7,0),[]);
});
