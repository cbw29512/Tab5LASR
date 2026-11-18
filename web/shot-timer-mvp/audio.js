(() => {
  const { handleError } = window.LASR;
  let context = null;
  let pending = [];
  // Sounds are generated locally. No audio files, downloads, or permissions API.
  async function prepareAudio() {
    try {
      const AudioClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioClass) throw new Error("Web Audio not supported in this browser");
      if (!context) context = new AudioClass();
      if (context.state === "suspended") await context.resume();
      if (context.state !== "running") throw new Error("Audio output could not start");
      return true;
    } catch (error) {
      console.warn("[Tab5 LASR mock] Beeps unavailable; visual GO remains active", error);
      return false;
    }
  }
  function sound(frequency, offsetSeconds, duration, volume) {
    try {
      const oscillator = context.createOscillator();
      const gain = context.createGain();
      oscillator.type = "sine";
      oscillator.frequency.setValueAtTime(frequency, context.currentTime);
      const at = context.currentTime + offsetSeconds;
      gain.gain.setValueAtTime(0.0001, at);
      gain.gain.exponentialRampToValueAtTime(volume, at + 0.008);
      gain.gain.exponentialRampToValueAtTime(0.0001, at + duration);
      oscillator.connect(gain).connect(context.destination);
      oscillator.start(at);
      oscillator.stop(at + duration + 0.01);
      pending.push(oscillator);
      oscillator.onended = () => { pending = pending.filter(item => item !== oscillator); };
    } catch (error) { handleError("Scheduling beep", error); }
  }
  function playCountdown(delaySeconds) {
    try {
      if (!context || context.state !== "running") return false;
      cancelBeeps();
      // Short preparation beeps; longer, higher-pitched GO signal at zero.
      for (let tick = 0; tick < delaySeconds; tick++) sound(610, tick, 0.12, 0.19);
      sound(1080, delaySeconds, 0.48, 0.35);
      return true;
    } catch (error) { handleError("Playing start beeps", error); return false; }
  }
  function cancelBeeps() {
    try {
      for (const oscillator of pending) {
        try { oscillator.stop(); }
        catch (error) { console.debug("[Tab5 LASR mock] Beep already ended", error); }
      }
      pending = [];
    } catch (error) { handleError("Cancelling beeps", error); }
  }
  window.LASRAudio = { prepareAudio, playCountdown, cancelBeeps };
})();