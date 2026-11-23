// Alarm in the browser: while the grill's alarm sounds, this page shows a dialog with the probe and
// the reason, beeps, vibrates (Android) and puts a bell in the tab title. The sound is on by default
// and switched per browser in Settings. Browsers only allow sound after the page was tapped or clicked
// once, so the audio is unlocked on the first interaction.
const AlarmSound = (() => {
  const KEY = "grilly-plus.alarm-sound";
  const REPEAT_MS = 2000;
  let context = null;
  let timer = null;
  let title = null;
  let dialog = null;
  let dismissed = false;   // closed or muted: stays away until this alarm is over, the grill takes ~2 s to stop

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

  // Mute from the dialog or the bar at the top
  function muted() {
    stop();
    dismissed = true;
    if (dialog && dialog.open) dialog.close();
  }

  function buildDialog() {
    dialog = document.createElement("dialog");
    dialog.className = "confirm-dialog alarm-dialog";
    dialog.setAttribute("aria-labelledby", "alarm-dialog-title");
    dialog.innerHTML =
      '<h2 id="alarm-dialog-title"><svg class="icon" aria-hidden="true"><use href="#i-bell"/></svg><span></span></h2>' +
      '<ul class="alarm-dialog-list"></ul>' +
      '<div class="confirm-dialog-buttons">' +
      '<button type="button" class="button" data-close>Close</button>' +
      '<button type="button" class="button primary" data-mute>Mute</button>' +
      "</div>";
    Dialog.place(dialog);
    const close = () => { dismissed = true; dialog.close(); };
    dialog.querySelector("[data-close]").addEventListener("click", close);
    dialog.addEventListener("cancel", (event) => { event.preventDefault(); close(); });
    // The header's mute button does the request and keeps its own bar hidden while the grill catches up
    dialog.querySelector("[data-mute]").addEventListener("click", () => document.getElementById("alarm-mute-btn").click());
  }

  function showDialog(status) {
    if (!dialog) buildDialog();
    const alarming = status.probes.filter((probe) => probe.alarm && probe.connected);
    const messages = alarming.map((probe) => Format.alarmMessage(probe, status.temperature_unit))
      .concat((status.timers || []).filter((timer) => timer.done).map(Format.timerMessage));
    dialog.querySelector("h2 span").textContent =
      messages.length === 1 ? messages[0].title : messages.length > 1 ? messages.length + " alarms" : "Alarm";
    const list = dialog.querySelector(".alarm-dialog-list");
    list.replaceChildren(...messages.map((message) => {
      const item = document.createElement("li");
      if (messages.length > 1) {
        const strong = document.createElement("strong");
        strong.textContent = message.title;
        item.append(strong, document.createElement("br"));
      }
      item.append(message.detail);
      return item;
    }));
    // Also when something moved it off the top layer (Bitwarden does); this runs with every status
    if (!Dialog.isModal(dialog)) {
      Dialog.reshow(dialog);
      dialog.querySelector("[data-mute]").focus();
    }
  }

  function onStatus(status) {
    if (status.alarm_sounding) {
      if (!dismissed) showDialog(status);
      if (enabled()) start(); else stop();
    } else {
      dismissed = false;
      if (dialog && dialog.open) dialog.close();
      stop();
    }
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
    document.getElementById("alarm-mute-btn").addEventListener("click", muted);
    Settings.addCard({ id: "browser", build, fill: () => {} });
    App.pollInBackground(wantsBackground);
  }

  return { enabled, setEnabled, play, stop };
})();
