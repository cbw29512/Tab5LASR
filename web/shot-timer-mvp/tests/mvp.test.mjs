import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import path from "node:path";
import {fileURLToPath} from "node:url";
const ROOT=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
function boot(persisted=new Map()) {
  // A small deterministic runtime allows testing the actual browser logic without network or firmware.
  const window={};
  const context=vm.createContext({window,console,Date,Math,performance:{now:()=>1000},
    document:{getElementById:()=>null},localStorage:{
      getItem:key=>persisted.get(key)??null,
      setItem:(key,value)=>persisted.set(key,String(value))
    }});
  for(const file of ["state.js","templates.js","storage.js","clock.js","alignment.js"]) {
    vm.runInContext(fs.readFileSync(path.join(ROOT,file),"utf8"),context,{filename:file});
  }
  return {window,persisted};
}
test("five original template records are selectable and differently scored",()=>{
  const {window:w}=boot();
  assert.deepEqual(Array.from(w.LASRTemplates.TEMPLATES,t=>t.id),
    ["bullseye","five-dot","silhouette","plates","blank"]);
  assert.equal(w.LASRTemplates.scoreAt("bullseye",.5,.5).score,10);
  assert.equal(w.LASRTemplates.scoreAt("five-dot",.3,.25).score,1);
  assert.equal(w.LASRTemplates.scoreAt("plates",.5,.5).zone,"DOT 3");
  assert.equal(w.LASRTemplates.scoreAt("blank",.25,.25).score,1);
  assert.equal(w.LASRTemplates.scoreAt("silhouette",.5,.5).score,5);
  assert.equal(w.LASRTemplates.scoreAt("bullseye",2,2).score,0);
});
test("alignment profile persists per target but confirmation never auto-restores",()=>{
  const persisted=new Map();const {window:w}=boot(persisted);
  w.LASRStorage.activate("five-dot");
  w.LASR.settings.corners[0]=[240,40];
  assert.equal(w.LASRStorage.saveCurrent(),true);
  w.LASRStorage.activate("plates");
  assert.equal(w.LASR.settings.corners[0][0],228);
  assert.equal(w.LASRStorage.saveCurrent(),true);
  w.LASRStorage.activate("five-dot");
  assert.equal(w.LASR.settings.corners[0][0],240);
  const reboot=boot(persisted).window;
  reboot.LASRStorage.load();
  assert.equal(reboot.LASR.settings.activeTemplateId,"five-dot");
  assert.equal(reboot.LASR.settings.corners[0][0],240);
  assert.equal(reboot.LASR.session.alignmentConfirmed,false);
  assert.equal(reboot.LASR.session.phase,"CHECK_ALIGNMENT");
});
test("corrupt and wrong-version browser settings fail closed",()=>{
  const {window:w,persisted}=boot();
  persisted.set(w.LASRStorage.KEY,JSON.stringify({schemaVersion:6,activeTemplateId:"bullseye"}));
  assert.equal(w.LASRStorage.load(),false);
  assert.equal(w.LASR.session.alignmentConfirmed,false);
  assert.equal(w.LASRStorage.validCorners([[1,1],[2,2]]),false);
});
test("bilinear preview coordinates round-trip for quadrilateral",()=>{
  const {window:w}=boot();
  w.LASR.settings.corners=[[230,30],[530,40],[500,355],[210,340]];
  const location=w.LASRAlignment.project(.31,.74);
  const result=w.LASRAlignment.unproject(location.x,location.y);
  assert.ok(Math.abs(result.u-.31)<.00001);
  assert.ok(Math.abs(result.v-.74)<.00001);
});
test("GO starts session, first and later splits exclude pause and countdown",()=>{
  const {window:w}=boot();const s=w.LASR.session,c=w.LASRClock;
  s.alignmentConfirmed=true;s.phase=w.LASR.PHASES.READY;
  c.beginCountdown(3,1000);
  assert.equal(c.startClock(3999),false);
  assert.equal(c.startClock(4000),true);
  assert.equal(c.appendShot({score:10,zone:"R1"},4450),true);
  assert.equal(s.shots[0].elapsedMs,450);
  assert.equal(s.shots[0].splitMs,450);
  c.freezeClock(4600);s.phase=w.LASR.PHASES.PAUSED;
  c.beginCountdown(3,5000);assert.equal(c.startClock(8000),true);
  assert.equal(c.appendShot({score:5,zone:"R5"},8300),true);
  assert.equal(s.shots[1].elapsedMs,900);
  assert.equal(s.shots[1].splitMs,450);
  assert.equal(s.score,15);
});
test("clock rejects shots outside active state and invalid delay",()=>{
  const {window:w}=boot();
  assert.equal(w.LASRClock.appendShot({score:10},1000),false);
  assert.throws(()=>w.LASRClock.beginCountdown(2,1000),/Unsupported start delay/);
});
test("HTML uses local scripts and exposes all required MVP controls",()=>{
  const html=fs.readFileSync(path.join(ROOT,"index.html"),"utf8");
  for(const id of ["templatesBtn","templateGrid","confirmBtn","startBtn","pauseBtn",
    "stopBtn","demoShotBtn","exportBtn","targetSvg","delaySelect"])
    assert.match(html,new RegExp(`id="${id}"`));
  assert.doesNotMatch(html,/\b(?:src|href)="https?:\/\//);
});