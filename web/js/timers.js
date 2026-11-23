// Timers on the grill page: a chip per timer with a live countdown and a cross to stop it, and a
// "+ Timer" button that opens a small dialog to start one. The timers run on the grill itself; the
// countdown here ticks between status polls from the remaining_s the grill sent.
const Timers = (() => {
  const QUICK_MINUTES = [5, 10, 15, 30, 60];

  let row, chips, addButton;
  let timers = [];
  let syncedAt = 0;
  let ticker = null;
  let dialog = null;

  function secondsLeft(timer) {
    return Math.max(0, timer.remaining_s - (Date.now() - syncedAt) / 1000);
  }

  function chip(timer) {
    const el = document.createElement("div");
    el.className = "timer-chip" + (timer.done ? " done" : "");
    el.dataset.id = String(timer.id);
    el.innerHTML = '<svg class="icon" aria-hidden="true"><use href="#i-timer"/></svg>' +
      '<span class="timer-name"></span><span class="timer-left"></span>' +
      '<button type="button" class="timer-stop" aria-label="Stop timer"><svg class="icon"><use href="#i-close"/></svg></button>';
    el.querySelector(".timer-name").textContent = timer.name;
    el.querySelector(".timer-stop").setAttribute("aria-label", "Stop " + timer.name);
    el.querySelector(".timer-stop").addEventListener("click", () => stop(timer.id, el));
    return el;
  }

  function tick() {
    chips.querySelectorAll(".timer-chip").forEach((el) => {
      const timer = timers.find((t) => String(t.id) === el.dataset.id);
      if (!timer) return;
      el.querySelector(".timer-left").textContent = timer.done ? "Time's up" : Format.timerLeft(secondsLeft(timer));
    });
  }

  function render() {
    const key = timers.map((t) => t.id + "|" + t.name + "|" + t.done).join(",");
    if (chips.dataset.key !== key) {
      chips.dataset.key = key;
      chips.replaceChildren(...timers.map(chip));
    }
    row.classList.toggle("has-timers", timers.length > 0);
    addButton.disabled = timers.length >= 6;
    tick();
    const running = timers.some((t) => !t.done);
    if (running && !ticker) ticker = setInterval(tick, 1000);
    if (!running && ticker) { clearInterval(ticker); ticker = null; }
  }

  async function stop(id, el) {
    el.classList.add("stopping");
    try {
      await Api.post("/api/timers/cancel", { id });
      timers = timers.filter((t) => t.id !== id);
      render();
    } catch (error) {
      el.classList.remove("stopping");
      if (error.status === 404) { timers = timers.filter((t) => t.id !== id); render(); }
    }
  }

  function buildDialog() {
    dialog = document.createElement("dialog");
    dialog.className = "confirm-dialog timer-dialog";
    dialog.setAttribute("aria-labelledby", "timer-dialog-title");
    dialog.innerHTML =
      '<h2 id="timer-dialog-title">New timer</h2>' +
      '<form method="dialog" novalidate>' +
      '<label class="field"><span>Name</span><input name="name" maxlength="24" autocomplete="off" placeholder="Timer"></label>' +
      '<label class="field"><span>Minutes</span><input name="minutes" type="number" inputmode="numeric" min="1" max="1440" required></label>' +
      '<div class="timer-quick">' + QUICK_MINUTES.map((m) => '<button type="button" class="chip-button" data-minutes="' + m + '">' + m + '</button>').join("") + '</div>' +
      '<p class="hint hint-error" role="status" hidden></p>' +
      '<div class="confirm-dialog-buttons">' +
      '<button type="button" class="button" data-cancel>Cancel</button>' +
      '<button type="submit" class="button primary">Start</button>' +
      '</div></form>';
    const form = dialog.querySelector("form");
    const minutes = form.elements.minutes;
    const error = dialog.querySelector(".hint-error");
    dialog.querySelectorAll("[data-minutes]").forEach((button) => button.addEventListener("click", () => {
      minutes.value = button.dataset.minutes;
    }));
    dialog.querySelector("[data-cancel]").addEventListener("click", () => dialog.close());
    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      const value = parseInt(minutes.value, 10);
      if (!(value >= 1 && value <= 1440)) { error.textContent = "Fill in 1 to 1440 minutes."; error.hidden = false; minutes.focus(); return; }
      const submit = form.querySelector('[type="submit"]');
      submit.disabled = true;
      try {
        const timer = await Api.post("/api/timers", { name: form.elements.name.value.trim(), minutes: value });
        timers.forEach((t) => { t.remaining_s = secondsLeft(t); });   // re-based on now
        syncedAt = Date.now();
        timers = timers.concat(timer);
        render();
        dialog.close();
      } catch (e) {
        error.textContent = e.message;
        error.hidden = false;
      } finally {
        submit.disabled = false;
      }
    });
    Dialog.place(dialog);
  }

  function openDialog() {
    if (!dialog) buildDialog();
    const form = dialog.querySelector("form");
    form.reset();
    dialog.querySelector(".hint-error").hidden = true;
    if (!Dialog.isModal(dialog)) Dialog.reshow(dialog);
    form.elements.name.focus();
  }

  function onStatus(status) {
    timers = (status.timers || []).slice();
    syncedAt = Date.now();
    render();
  }

  function mount(el) {
    row = el;
    row.innerHTML = '<div class="timer-chips"></div>' +
      '<button type="button" class="chip-button timer-add"><svg class="icon" aria-hidden="true"><use href="#i-plus"/></svg>Timer</button>';
    chips = row.querySelector(".timer-chips");
    addButton = row.querySelector(".timer-add");
    addButton.addEventListener("click", openDialog);
    App.onStatus(onStatus);
  }

  return { mount };
})();
