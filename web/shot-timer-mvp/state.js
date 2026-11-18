(() => {
  // SCHEMA FIRST: all coordinates are normalized through the camera preview's SVG space.
  const PHASES = Object.freeze({CHECK: "CHECK_ALIGNMENT", READY: "READY", COUNTDOWN: "COUNTDOWN",
    ACTIVE: "ACTIVE", PAUSED: "PAUSED", COMPLETE: "COMPLETE"});
  const DEFAULT_CORNERS = Object.freeze([[228,28],[535,28],[535,371],[228,371]]);
  const session = {phase: PHASES.CHECK, alignmentConfirmed: false, shots: [], hits: 0,
    score: 0, accumulatedMs: 0, runStartedAtMs: null, countdownStartedAtMs: null,
    countdownEndsAtMs: null, goFlashUntilMs: 0, delaySeconds: 3,
    audioAvailable: null, lastError: null};
  const settings = {schemaVersion: 1, activeTemplateId: "bullseye", corners: DEFAULT_CORNERS.map(p=>[...p]),
    savedAt: null, persistent: false, profiles: {}};
  function handleError(operation, error) {
    try {
      console.error(`[LASR MVP] ${operation} failed`, error);
      session.lastError = `${operation} failed: ${error?.message || String(error)}`;
      const notice = document.getElementById("errorNotice");
      if (notice) notice.textContent = session.lastError;
    } catch (loggingError) { console.error("[LASR MVP] error reporting failed", loggingError); }
  }
  window.LASR = {PHASES, DEFAULT_CORNERS, session, settings, handleError};
})();