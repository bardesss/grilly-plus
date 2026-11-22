// The Send test button and the state of the last notification, under the Notifications settings
const NotifyCard = (() => {
  const POLL_MS = 1500;
  const WAIT_MS = 90000;   // the grill retries a failed message, give it time

  let button, status;
  let settings = null;
  let testing = false;

  // What the status line says about the last message: "" when there is nothing to tell
  function describe(state) {
    if (!state || !state.last) return "";
    const last = state.last;
    if (last.ok) return last.test ? "Test sent. Check Telegram." : "Last notification sent " +
      (last.seconds_ago < 60 ? "just now" : Format.ago(last.seconds_ago)) + ".";
    return (last.test ? "Test failed: " : "Last notification failed: ") + last.error;
  }

  function show(text, isError) {
    status.textContent = text;
    status.hidden = text === "";
    status.classList.toggle("hint-error", !!isError);
  }

  async function refresh() {
    try {
      const state = await Api.get("/api/notify");
      if (!testing) show(describe(state), state.last && !state.last.ok);
      return state;
    } catch (error) {
      return null;
    }
  }

  async function sendTest() {
    testing = true;
    button.disabled = true;
    show("Sending…", false);
    try {
      await Api.post("/api/notify/test", {});
      const started = Date.now();
      // Done once the grill has no more queued messages and reports on a test message
      for (;;) {
        await new Promise((resolve) => setTimeout(resolve, POLL_MS));
        const state = await Api.get("/api/notify").catch(() => null);
        if (state && !state.sending && state.last && state.last.test && state.last.seconds_ago * 1000 <= Date.now() - started + POLL_MS) {
          show(describe(state), !state.last.ok);
          break;
        }
        if (Date.now() - started > WAIT_MS) { show("No answer from the grill yet. Check Telegram in a minute.", true); break; }
      }
    } catch (error) {
      show(error.message, true);
    } finally {
      testing = false;
      button.disabled = !ready();
    }
  }

  function ready() {
    return !!(settings && settings.telegram_token_set && settings.telegram_chat_id);
  }

  function build() {
    const footer = document.createElement("div");
    footer.className = "notify-footer";
    footer.innerHTML = '<button type="button" class="button">Send test</button><p class="hint" role="status" hidden></p>';
    button = footer.querySelector("button");
    status = footer.querySelector("p");
    button.addEventListener("click", sendTest);
    return footer;
  }

  function fill(newSettings) {
    settings = newSettings;
    if (!button) return;
    if (!testing) button.disabled = !ready();
    button.title = ready() ? "" : "Save a bot token and chat ID first";
    refresh();
  }

  Settings.addFooter("notify", { build, fill });
  return { describe };
})();
