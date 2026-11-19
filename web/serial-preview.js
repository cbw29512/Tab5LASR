import {parseUsbJpegLine} from "./serial-jpeg.js";

// Visible preview comes only from complete validated Tab5 JPEG packets.
// Raw detector locations are NOT calibrated to a physical target.
const picture=document.getElementById("usb-camera");
const placeholder=document.getElementById("camera-waiting");
const metadata=document.getElementById("camera-meta");
const overlay=document.getElementById("camera-hit-markers");
const orientation=document.getElementById("orientation");
let lastSequence=0,accepted=0,lastFrameAt=0,lastHits=[];

function oriented(x,y,mode) {
  switch(mode) {
    case "raw":return [x,y];
    case "turn90":return [1-y,x];
    case "turn270":return [y,1-x];
    default:return [1-x,1-y];
  }
}
export function acceptCameraLine(line) {
  try {
    if (!line.startsWith("LASRJPEGV1 ")) return false;
    const frame=parseUsbJpegLine(line);
    if (!frame) {
      metadata.textContent="Incomplete/corrupt camera frame discarded";
      return true;
    }
    if (frame.seq<=lastSequence) return true;
    lastSequence=frame.seq;accepted++;lastFrameAt=performance.now();
    picture.src=`data:image/jpeg;base64,${frame.base64}`;
    picture.hidden=false;placeholder.hidden=true;
    metadata.textContent=`Real Tab5 CSI JPEG · ${frame.width}×${frame.height} · frame ${frame.seq} · ${accepted} received`;
    return true;
  } catch (error) {
    console.error("Could not show verified USB camera image",error);
    return true;
  }
}
export function renderCameraHitOverlay(hits) {
  try {
    lastHits=hits;
    overlay.replaceChildren();
    for(const hit of hits.slice(-30)){
      const [x,y]=oriented(hit.x_norm,hit.y_norm,orientation.value);
      const marker=document.createElement("span");
      marker.className="camera-hit";
      marker.style.left=`${x*100}%`;
      marker.style.top=`${y*100}%`;
      marker.title=`Uncalibrated CSI candidate: x=${hit.x_norm.toFixed(3)}, y=${hit.y_norm.toFixed(3)}`;
      overlay.append(marker);
    }
  } catch(error) {
    console.error("Camera overlay rendering failed",error);
  }
}
orientation.addEventListener("change",()=>renderCameraHitOverlay(lastHits));
export function clearCameraView() {
  try {
    picture.removeAttribute("src");picture.hidden=true;
    placeholder.hidden=false;
    overlay.replaceChildren();
    metadata.textContent="Waiting for real Tab5 camera frames over USB";
    lastSequence=0;accepted=0;lastFrameAt=0;lastHits=[];
  } catch(error) {
    console.error("Could not clear USB camera preview",error);
  }
}
export function cameraFrameAge() {
  return lastFrameAt ? Math.round(performance.now()-lastFrameAt) : null;
}
