import test from "node:test";
import assert from "node:assert/strict";
import {normalizeStatus, streamHealthy, formatNumber, shouldRetryStream, viewerState} from "../model.js";

test("stream never reports live solely from camera status", () => {
  const status = normalizeStatus({state:"live",frame_age_ms:100});
  assert.equal(streamHealthy(status,false),false);
  assert.equal(streamHealthy(status,true),true);
});

test("stale frames cannot show live", () => {
  const status = normalizeStatus({state:"live",frame_age_ms:4000});
  assert.equal(streamHealthy(status,true),false);
});

test("invalid status is an error", () => {
  assert.equal(normalizeStatus(null).state,"error");
  assert.equal(normalizeStatus({state:"busy"}).state,"error");
});

test("missing measurements remain unknown, not invented", () => {
  const s = normalizeStatus({state:"starting",fps_capture:-2,battery_mv:null});
  assert.equal(s.fps_capture,null);
  assert.equal(s.battery_mv,null);
  assert.equal(formatNumber(null),"—");
});

test("known fields pass unchanged", () => {
  const s = normalizeStatus({state:"live",camera:"onboard_csi",
    fps_capture:12.5,width:640,height:480,frame_age_ms:100});
  assert.equal(s.width,640);
  assert.equal(s.fps_capture,12.5);
});

test("stalled camera reports STALE instead of perpetually connecting", () => {
  const status = normalizeStatus({state:"stale",wifi_state:"connected",frame_age_ms:8500});
  assert.equal(viewerState(status,true),"stale");
  assert.equal(viewerState({state:"error"},false),"error");
  assert.equal(viewerState({state:"starting"},false),"starting");
});

test("MJPEG reconnect is throttled and requires real Tab5 connectivity", () => {
  const status = normalizeStatus({state:"live",wifi_state:"connected",frame_age_ms:90});
  assert.equal(shouldRetryStream(status,false,100),false);
  assert.equal(shouldRetryStream(status,false,5000),true);
  assert.equal(shouldRetryStream(status,true,7000),false);
  assert.equal(shouldRetryStream({...status,wifi_state:"disconnected"},false,7000),false);
  assert.equal(shouldRetryStream({...status,state:"error"},false,7000),false);
});

test("stale encoder may reconnect without falsely claiming LIVE", () => {
  const status = normalizeStatus({state:"stale",wifi_state:"connected",frame_age_ms:9000});
  assert.equal(shouldRetryStream(status,false,6000),true);
  assert.equal(viewerState(status,false),"stale");
});
