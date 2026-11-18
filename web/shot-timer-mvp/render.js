(() => {
  const {settings,session,PHASES,handleError}=window.LASR;
  const clock=window.LASRClock;
  const ids=["confirmBtn","recalibrateBtn","startBtn","pauseBtn","stopBtn","demoShotBtn",
    "delaySelect","startLabel","alignmentHeadline","alignmentDetail","alignSymbol",
    "phaseBadge","timer","shots","score","lastShot","lastSplit","sessionMessage",
    "shotRows","countdownOverlay","countdownNumber","countdownSubtext","audioStatus",
    "targetName","templateLabel","saveStatus","profileInfo","exportBtn","templatesBtn"];
  const ui=Object.fromEntries(ids.map(id=>[id,document.getElementById(id)]));
  const messages={CHECK_ALIGNMENT:"Align green corners, then confirm before Start.",
    READY:"Ready. Start triggers countdown beeps and a GO signal.",
    COUNTDOWN:"Get ready — clock starts at GO, not when Start was clicked.",
    ACTIVE:"Clock running. Demo Shot or target tap simulates a hit.",
    PAUSED:"Paused. Resume repeats the countdown without losing splits.",
    COMPLETE:"Session complete. Export CSV to keep the simulated shot log."};
  function updateTiming(now=clock.nowMs()) {
    try {
      ui.timer.textContent=clock.formatTime(clock.elapsedMs(now));
      const counting=session.phase===PHASES.COUNTDOWN;
      const go=session.phase===PHASES.ACTIVE&&now<session.goFlashUntilMs;
      ui.countdownOverlay.hidden=!(counting||go);
      ui.countdownNumber.textContent=counting?String(clock.countdownNumber(now)):"GO!";
      ui.countdownSubtext.textContent=counting?"WAIT FOR START SIGNAL":"SHOT TIMER RUNNING";
    } catch(error){handleError("Updating timer display",error);}
  }
  function renderShots() {
    try {
      const host=ui.shotRows;host.replaceChildren();
      for(const shot of session.shots.slice().reverse()) {
        const tr=document.createElement("tr");
        for(const value of [String(shot.number),clock.formatTime(shot.elapsedMs),
          `+${clock.formatTime(shot.splitMs,false)}`,shot.zone]) {
          const td=document.createElement("td");td.textContent=value;tr.appendChild(td);
        }
        host.appendChild(tr);
      }
      if(!session.shots.length) {
        const tr=document.createElement("tr"),td=document.createElement("td");
        td.colSpan=4;td.className="no-shots";td.textContent="No simulated shots yet";tr.appendChild(td);host.appendChild(tr);
      }
    } catch(error){handleError("Updating shot table",error);}
  }
  function render() {
    try {
      const {phase,alignmentConfirmed:confirmed}=session;
      const busy=[PHASES.COUNTDOWN,PHASES.ACTIVE,PHASES.PAUSED].includes(phase);
      const t=window.LASRTemplates.TEMPLATES.find(t=>t.id===settings.activeTemplateId);
      ui.targetName.textContent=t.name;ui.templateLabel.textContent=t.name;
      ui.profileInfo.textContent=settings.savedAt?"SAVED TARGET PROFILE · BROWSER ONLY":"UNSAVED ALIGNMENT · DEMO";
      ui.saveStatus.textContent=!settings.persistent?"Temporary · browser storage blocked":settings.savedAt?"Saved in browser":"Not saved yet";
      ui.alignmentHeadline.textContent=confirmed?"Alignment confirmed":"Confirm alignment";
      ui.alignmentDetail.textContent=confirmed?"Manual confirmation; no camera verification.":"Drag corners to match the paper target.";
      ui.alignSymbol.textContent=confirmed?"✓":"!";ui.alignSymbol.classList.toggle("checked",confirmed);
      ui.confirmBtn.disabled=busy||confirmed;
      ui.confirmBtn.textContent=confirmed?"✓ ALIGNMENT CONFIRMED":"✓ CONFIRM & SAVE ALIGNMENT";
      ui.recalibrateBtn.disabled=busy;ui.templatesBtn.disabled=busy;ui.delaySelect.disabled=busy;
      ui.startBtn.disabled=![PHASES.READY,PHASES.PAUSED,PHASES.COMPLETE].includes(phase);
      ui.startLabel.textContent=phase===PHASES.PAUSED?"RESUME (BEEPS)":phase===PHASES.COMPLETE?"NEW SESSION":"START SHOOTING";
      ui.pauseBtn.disabled=phase!==PHASES.ACTIVE;ui.stopBtn.disabled=!busy;
      ui.demoShotBtn.disabled=phase!==PHASES.ACTIVE;ui.exportBtn.disabled=!session.shots.length;
      ui.phaseBadge.textContent=phase===PHASES.ACTIVE?"SHOOTING":phase===PHASES.COUNTDOWN?"GET READY":phase===PHASES.CHECK?"WAITING":phase;
      ui.phaseBadge.className=`phase ${phase===PHASES.COUNTDOWN?"countdown":phase===PHASES.ACTIVE?"active":phase===PHASES.READY?"ready":phase===PHASES.PAUSED?"paused":phase===PHASES.COMPLETE?"complete":""}`;
      ui.shots.textContent=String(session.shots.length);ui.score.textContent=String(session.score);
      const last=session.shots.at(-1);
      ui.lastShot.textContent=last?clock.formatTime(last.elapsedMs):"—";
      ui.lastSplit.textContent=last?clock.formatTime(last.splitMs,false):"—";
      ui.sessionMessage.textContent=messages[phase];
      ui.audioStatus.textContent=session.audioAvailable===false?"AUDIO BLOCKED · VISUAL GO":"AUDIBLE START · LOCAL";
      renderShots();window.LASRTargetView.renderTarget();updateTiming();
    } catch(error){handleError("Rendering session",error);}
  }
  window.LASRView={ui,render,updateTiming};
})();