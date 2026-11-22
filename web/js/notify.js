// The Send test button and how the last notification went, under the Notifications settings
const NotifyCard = (() => {
  const POLL_MS = 1500;
  const WAIT_MS = 120000;   // the grill retries a failed message on every service, give it time
  const SERVICES = [["telegram", "Telegram"], ["ntfy", "ntfy"]];

  let button, status;
  let settings = null;
  let testing = false;

  // Whether the settings have at least one service ready to send
  function anySetUp(s) {
    return !!(s && ((s.telegram_token_set && s.telegram_chat_id) || s.ntfy_url));
  }

  // One line per service that is set up and sent something: [{ text, error }]
  function describe(state) {
    if (!state) return [];
    return SERVICES.filter(([key]) => state[key] && state[key].set_up && state[key].last).map(([key, label]) => {
      const last = state[key].last;
      if (!last.ok) {
        // The grill's errors start with the service name, which the line already has
        const why = last.error.startsWith(label + ": ") ? last.error.slice(label.length + 2) : last.error;
        return { text: label + (last.test ? ": test failed, " : ": last notification failed, ") + why, error: true };
      }
      if (last.test) return { text: label + ": test sent.", error: false };
      return { text: label + ": last notification sent " + (last.seconds_ago < 60 ? "just now" : Format.ago(last.seconds_ago)) + ".", error: false };
    });
  }

  // Done when nothing is queued any more and every service that is set up reported on a test
  // that is younger than the time since we asked for it
  function testFinished(state, elapsedMs) {
    if (!state || state.sending) return false;
    const services = SERVICES.filter(([key]) => state[key] && state[key].set_up);
    return services.length > 0 && services.every(([key]) => {
      const last = state[key].last;
      return last && last.test && last.seconds_ago * 1000 <= elapsedMs + POLL_MS;
    });
  }

  function show(lines) {
    status.replaceChildren(...lines.map((line) => {
      const p = document.createElement("p");
      p.className = "hint" + (line.error ? " hint-error" : "");
      p.textContent = line.text;
      return p;
    }));
    status.hidden = lines.length === 0;
  }

  async function refresh() {
    try {
      const state = await Api.get("/api/notify");
      if (!testing) show(describe(state));
    } catch (error) {
      // The status line is a nice-to-have, the settings still work without it
    }
  }

  async function sendTest() {
    testing = true;
    button.disabled = true;
    show([{ text: "Sending…", error: false }]);
    try {
      await Api.post("/api/notify/test", {});
      const started = Date.now();
      for (;;) {
        await new Promise((resolve) => setTimeout(resolve, POLL_MS));
        const state = await Api.get("/api/notify").catch(() => null);
        if (testFinished(state, Date.now() - started)) { show(describe(state)); break; }
        if (Date.now() - started > WAIT_MS) { show([{ text: "No answer from the grill yet. Check your phone in a minute.", error: true }]); break; }
      }
    } catch (error) {
      show([{ text: error.message, error: true }]);
    } finally {
      testing = false;
      button.disabled = !anySetUp(settings);
    }
  }

  function build() {
    const footer = document.createElement("div");
    footer.className = "notify-footer";
    footer.innerHTML = '<button type="button" class="button">Send test</button><div class="notify-status" role="status" hidden></div>';
    button = footer.querySelector("button");
    status = footer.querySelector(".notify-status");
    button.addEventListener("click", sendTest);
    return footer;
  }

  function fill(newSettings) {
    settings = newSettings;
    if (!button) return;
    if (!testing) button.disabled = !anySetUp(settings);
    button.title = anySetUp(settings) ? "" : "Set up Telegram or ntfy first";
    refresh();
  }

  if (typeof Settings !== "undefined") Settings.addFooter("notify", { build, fill });
  return { anySetUp, describe, testFinished };
})();

if (typeof module === "object" && module.exports) module.exports = NotifyCard;
