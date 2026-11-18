(() => {
  const { session, PHASES, handleError } = window.LASR;
  // Monotonic elapsed-time math prevents 1-second setInterval drift.
  function nowMs() {
    try { return performance.now(); }
    catch (error) { handleError("Reading monotonic clock", error); return 0; }
  }
  function elapsedMs(now = nowMs()) {
    try {
      const active = session.phase === PHASES.ACTIVE && session.runStartedAtMs !== null;
      return Math.max(0, session.accumulatedMs + (active ? now - session.runStartedAtMs : 0));
    } catch (error) { handleError("Calculating elapsed time", error); return 0; }
  }
  function formatTime(ms, includeMinutes = true) {
    try {
      const centiseconds = Math.max(0, Math.floor(ms / 10));
      const cs = String(centiseconds % 100).padStart(2, "0");
      const seconds = Math.floor(centiseconds / 100);
      if (!includeMinutes) return `${seconds}.${cs}s`;
      return `${String(Math.floor(seconds / 60)).padStart(2, "0")}:${String(seconds % 60).padStart(2, "0")}.${cs}`;
    } catch (error) { handleError("Formatting time", error); return "—"; }
  }
  function beginCountdown(delaySeconds, now = nowMs()) {
    try {
      // Time is frozen until GO; on resume elapsedMs excludes the pause and delay.
      if (![3, 5, 10].includes(delaySeconds)) throw new Error("Unsupported start delay");
      session.phase = PHASES.COUNTDOWN;
      session.delaySeconds = delaySeconds;
      session.countdownStartedAtMs = now;
      session.countdownEndsAtMs = now + delaySeconds * 1000;
      session.goFlashUntilMs = 0;
    } catch (error) { handleError("Arming start countdown", error); throw error; }
  }
  function countdownNumber(now = nowMs()) {
    try {
      if (session.phase !== PHASES.COUNTDOWN) return null;
      return Math.max(1, Math.ceil((session.countdownEndsAtMs - now) / 1000));
    } catch (error) { handleError("Reading countdown", error); return null; }
  }
  function startClock(now = nowMs()) {
    try {
      if (session.phase !== PHASES.COUNTDOWN || now < session.countdownEndsAtMs) return false;
      // Use the precomputed GO instant, not an arbitrary delayed interval callback.
      session.runStartedAtMs = session.countdownEndsAtMs;
      session.phase = PHASES.ACTIVE;
      session.goFlashUntilMs = now + 650;
      return true;
    } catch (error) { handleError("Starting shot clock", error); return false; }
  }
  function freezeClock(now = nowMs()) {
    try {
      if (session.phase === PHASES.ACTIVE) {
        session.accumulatedMs = elapsedMs(now);
        session.runStartedAtMs = null;
      }
    } catch (error) { handleError("Freezing shot clock", error); }
  }
  function appendShot(sample, now = nowMs()) {
    try {
      if (session.phase !== PHASES.ACTIVE) return false;
      const timeMs = Math.round(elapsedMs(now));
      const previous = session.shots.at(-1);
      const splitMs = previous ? timeMs - previous.elapsedMs : timeMs;
      session.shots.push({ ...sample, number: session.shots.length + 1,
        elapsedMs: timeMs, splitMs: Math.max(0, splitMs) });
      if (sample.score > 0) session.hits += 1;
      session.score += sample.score;
      return true;
    } catch (error) { handleError("Recording demo shot", error); return false; }
  }
  window.LASRClock = { nowMs, elapsedMs, formatTime, beginCountdown,
    countdownNumber, startClock, freezeClock, appendShot };
})();