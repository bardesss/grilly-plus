// Alarm in the browser: while the grill's alarm sounds, this page beeps, vibrates (Android) and puts a
// bell in the tab title. On by default, switched per browser in Settings. Browsers only allow sound
// after the page was tapped or clicked once, so the audio is unlocked on the first interaction.
const AlarmSound = (() => {
  const KEY = "grilly-plus.alarm-sound";
  const REPEAT_MS = 2000;
  let context = null;
  let timer = null;
  let title = null;

  function enabled() {
    try { return localStorage.getItem(KEY) !== "off"; } catch (error) { return true; }
  }

  function setEnabled(on) {
    try { localStorage.setItem(KEY, on ? "on" : "off"); } catch (error) { /* private mode: this session only */ }
    if (!on) stop();
  }

  function unlock() {
    const Context = window.AudioContext || window.webkitAudioContext;
    if (!Context) return null;
    if (!context) context = new Context();
    if (context.state === "suspended") context.resume();
    return context;
  }

  // Three short beeps
  function play() {
    const ctx = unlock();
    if (ctx && ctx.state === "running") {
      const start = ctx.currentTime + 0.02;
      for (let i = 0; i < 3; i++) {
        const at = start + i * 0.25;
        const tone = ctx.createOscillator();
        const gain = ctx.createGain();
        tone.type = "square";
        tone.frequency.value = 2000;
        gain.gain.setValueAtTime(0.0001, at);
        gain.gain.exponentialRampToValueAtTime(0.25, at + 0.01);
        gain.gain.setValueAtTime(0.25, at + 0.14);
        gain.gain.exponentialRampToValueAtTime(0.0001, at + 0.16);
        tone.connect(gain).connect(ctx.destination);
        tone.start(at);
        tone.stop(at + 0.17);
      }
    }
    if (navigator.vibrate) navigator.vibrate([150, 100, 150, 100, 150]);
  }

  function start() {
    if (timer !== null) return;
    play();
    timer = setInterval(play, REPEAT_MS);
    title = document.title;
    document.title = "🔔 Alarm · " + title;
  }

  function stop() {
    if (timer === null) return;
    clearInterval(timer);
    timer = null;
    if (navigator.vibrate) navigator.vibrate(0);
    if (title !== null) document.title = title;
    title = null;
  }

  function onStatus(status) {
    if (status.alarm_sounding && enabled()) start();
    else stop();
  }

  // The settings card for this browser
  function build() {
    const card = document.createElement("section");
    card.className = "card settings-card";
    card.innerHTML =
      '<h2 class="card-title"><span>This browser</span></h2>' +
      '<div class="setting-row"><label class="field-label" for="alarm-sound-switch">Sound when an alarm goes off</label>' +
      '  <input type="checkbox" class="switch" role="switch" id="alarm-sound-switch"></div>' +
      '<button type="button" class="link-button" id="alarm-sound-test">Play a test sound</button>' +
      '<p class="hint">Beeps here, together with the grill, while this page is open. Browsers only allow sound after you ' +
      'tapped the page once, and phones pause pages in the background, so keep it on screen. Saved in this browser only.</p>';
    const toggle = card.querySelector("#alarm-sound-switch");
    toggle.checked = enabled();
    toggle.addEventListener("change", () => setEnabled(toggle.checked));
    card.querySelector("#alarm-sound-test").addEventListener("click", play);
    return card;
  }

  // Polls while the page is in the background, so a tab left open behind others still sounds
  function wantsBackground() { return enabled(); }

  if (typeof App !== "undefined") {
    App.onStatus(onStatus);
    ["pointerdown", "keydown"].forEach((type) => document.addEventListener(type, unlock, { capture: true, passive: true }));
    document.getElementById("alarm-mute-btn").addEventListener("click", stop);
    Settings.addCard({ id: "browser", build, fill: () => {} });
    App.pollInBackground(wantsBackground);
  }

  return { enabled, setEnabled, play, stop };
})();
