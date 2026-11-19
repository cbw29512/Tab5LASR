import test from "node:test";
import assert from "node:assert/strict";
import {parseUsbJpegLine} from "../serial-jpeg.js";

// FNV-1a checksums match the firmware's JPEG serial protocol.
function checksum(bytes) {
  let hash=2166136261;
  for(const byte of bytes) hash=Math.imul(hash^byte,16777619)>>>0;
  return hash.toString(16).padStart(8,"0");
}
function packet(bytes,seq=3) {
  return `LASRJPEGV1 ${seq} 320 176 ${bytes.length} ${checksum(bytes)} ${Buffer.from(bytes).toString("base64")}`;
}
test("accepted packet requires real device framing, JPEG delimiters and checksum",()=>{
  const bytes=Uint8Array.from([0xff,0xd8,1,2,3,4,0xff,0xd9]);
  const parsed=parseUsbJpegLine(packet(bytes));
  assert.equal(parsed.seq,3);
  assert.equal(parsed.width,320);
  assert.equal(parsed.height,176);
  assert.equal(parsed.size,8);
});
test("reject incomplete, forged, or invalid camera packets",()=>{
  const bytes=Uint8Array.from([0xff,0xd8,1,2,3,4,0xff,0xd9]);
  const valid=packet(bytes);
  assert.equal(parseUsbJpegLine(valid.slice(0,-3)),null);
  assert.equal(parseUsbJpegLine(valid.replace(" 320 "," 640 ")),null);
  assert.equal(parseUsbJpegLine(valid.replace(" 176 "," 480 ")),null);
  assert.equal(parseUsbJpegLine(valid.replace(" 8 "," 9 ")),null);
  assert.equal(parseUsbJpegLine(valid.replace("LASRJPEGV1","LASRJPEGV0")),null);
  assert.equal(parseUsbJpegLine(valid.replace(/ [a-f0-9]{8} /," 00000000 ")),null);
  assert.equal(parseUsbJpegLine("I lasr_csi: capture=5.0 FPS"),null);
});
