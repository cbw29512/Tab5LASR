(() => {
  const {session,settings,PHASES,handleError}=window.LASR;
  const {ui,render,updateTiming}=window.LASRView;
  const clock=window.LASRClock,audio=window.LASRAudio;
  let startRequest=0;
  function clearResults() {
    try {Object.assign(session,{accumulatedMs:0,runStartedAtMs:null,countdownStartedAtMs:null,
      countdownEndsAtMs:null,goFlashUntilMs:0,shots:[],hits:0,score:0,lastError:null});}
    catch(error){handleError("Resetting results",error);}
  }
  function confirmAlignment() {
    try {
      if(session.phase!==PHASES.CHECK)return;
      if(!window.LASRStorage.saveCurrent()) {
        console.warn("[LASR MVP] Alignment accepted in memory only; persistent storage unavailable");
      }
      session.alignmentConfirmed=true;session.phase=PHASES.READY;render();
    } catch(error){handleError("Confirming alignment",error);}
  }
  function editCorners() {
    try {
      if(![PHASES.CHECK,PHASES.READY,PHASES.COMPLETE].includes(session.phase))return;
      clearResults();session.alignmentConfirmed=false;session.phase=PHASES.CHECK;render();
    } catch(error){handleError("Editing alignment",error);}
  }
  function chooseTemplate(id) {
    try {
      if(![PHASES.CHECK,PHASES.READY,PHASES.COMPLETE].includes(session.phase))return;
      window.LASRStorage.activate(id);clearResults();session.alignmentConfirmed=false;
      session.phase=PHASES.CHECK;document.getElementById("templateDialog").close();render();
    } catch(error){handleError("Changing template",error);}
  }
  async function startOrResume() {
    try {
      if(!session.alignmentConfirmed||![PHASES.READY,PHASES.PAUSED,PHASES.COMPLETE].includes(session.phase))return;
      if(session.phase===PHASES.COMPLETE)clearResults();
      const token=++startRequest;session.phase=PHASES.COUNTDOWN;session.audioAvailable=null;render();
      const available=await audio.prepareAudio();
      if(token!==startRequest||session.phase!==PHASES.COUNTDOWN)return;
      session.audioAvailable=available;
      const delay=Number(ui.delaySelect.value);
      clock.beginCountdown(delay);
      if(available)session.audioAvailable=audio.playCountdown(delay);
      render();
    } catch(error){handleError("Starting timer",error);
      session.phase=PHASES.PAUSED;render();}
  }
  function pause() {
    try {if(session.phase!==PHASES.ACTIVE)return;clock.freezeClock();session.phase=PHASES.PAUSED;render();}
    catch(error){handleError("Pausing timer",error);}
  }
  function stop() {
    try {
      if(![PHASES.COUNTDOWN,PHASES.ACTIVE,PHASES.PAUSED].includes(session.phase))return;
      ++startRequest;audio.cancelBeeps();clock.freezeClock();
      session.phase=PHASES.COMPLETE;render();
    } catch(error){handleError("Stopping timer",error);}
  }
  function registerDemoPoint(u,v) {
    try {
      if(session.phase!==PHASES.ACTIVE)return;
      const score=window.LASRTemplates.scoreAt(settings.activeTemplateId,u,v);
      const location=window.LASRAlignment.project(u,v);
      if(clock.appendShot({...location,...score,u,v,source:"demo"}))render();
    } catch(error){handleError("Registering simulated shot",error);}
  }
  function demoShot() {
    try {
      // Deterministic sample sequence, not a laser detector or a random shot generator.
      const positions=[[.50,.50],[.54,.49],[.44,.52],[.31,.27],[.69,.72],
        [.82,.28],[.5,.61],[.17,.51],[.65,.40],[.47,.46]];
      const [u,v]=positions[session.shots.length%positions.length];registerDemoPoint(u,v);
    } catch(error){handleError("Adding sample shot",error);}
  }
  function exportCsv() {
    try {
      if(!session.shots.length)return;
      const rows=[["shot","target","elapsed_ms","split_ms","zone","score","u","v","source"],
        ...session.shots.map(s=>[s.number,settings.activeTemplateId,s.elapsedMs,s.splitMs,s.zone,s.score,
          s.u.toFixed(4),s.v.toFixed(4),s.source])];
      const csv=rows.map(row=>row.map(v=>`"${String(v).replaceAll('"','""')}"`).join(",")).join("\r\n");
      const url=URL.createObjectURL(new Blob([csv],{type:"text/csv;charset=utf-8"}));
      const link=document.createElement("a");link.href=url;link.download=`lasr-demo-shots-${Date.now()}.csv`;
      document.body.appendChild(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    } catch(error){handleError("Exporting shot log",error);}
  }
  function tick() {
    try {
      const now=clock.nowMs();
      if(session.phase===PHASES.COUNTDOWN&&session.countdownEndsAtMs!==null&&clock.startClock(now))render();
      updateTiming(now);
    } catch(error){handleError("Updating session timer",error);}
  }
  function initialize() {
    try {
      window.LASRStorage.load(); // Reload saved geometry but NEVER auto-confirm alignment.
      window.LASRAlignment.init(render);
      const dialog=document.getElementById("templateDialog");
      ui.confirmBtn.addEventListener("click",confirmAlignment);ui.recalibrateBtn.addEventListener("click",editCorners);
      ui.startBtn.addEventListener("click",startOrResume);ui.pauseBtn.addEventListener("click",pause);
      ui.stopBtn.addEventListener("click",stop);ui.demoShotBtn.addEventListener("click",demoShot);
      ui.exportBtn.addEventListener("click",exportCsv);
      ui.templatesBtn.addEventListener("click",()=>{
        window.LASRTargetView.renderLibrary(chooseTemplate);dialog.showModal();
      });
      document.getElementById("closeTemplatesBtn").addEventListener("click",()=>dialog.close());
      document.getElementById("targetSvg").addEventListener("click",event=>{
        if(event.target.closest?.("[data-corner]"))return;
        if(session.phase!==PHASES.ACTIVE)return;
        const point=window.LASRAlignment.svgPoint(event);if(!point)return;
        const uv=window.LASRAlignment.unproject(point.x,point.y);
        if(uv)registerDemoPoint(uv.u,uv.v);
      });
      window.setInterval(tick,40);render();
      console.info("[LASR MVP] Ready: target templates, local profiles, simulated shots. NO camera connected.");
    } catch(error){handleError("Initializing MVP",error);}
  }
  initialize();
})();