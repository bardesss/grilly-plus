const test = require("node:test");
const assert = require("node:assert/strict");
global.Format = require("../js/format.js");
const NotifyCard = require("../js/notify.js");

const sent = (extra) => ({ ok: true, error: "", seconds_ago: 3, test: false, ...extra });

test("anySetUp needs a Telegram token and chat ID, or an ntfy topic URL", () => {
  assert.equal(NotifyCard.anySetUp({ telegram_token_set: true, telegram_chat_id: "" }), false);
  assert.equal(NotifyCard.anySetUp({ telegram_token_set: true, telegram_chat_id: "42" }), true);
  assert.equal(NotifyCard.anySetUp({ ntfy_url_set: true }), true);
  assert.equal(NotifyCard.anySetUp({ pushover_user_set: true }), false);
  assert.equal(NotifyCard.anySetUp({ pushover_user_set: true, pushover_token_set: true }), true);
  assert.equal(NotifyCard.anySetUp({ webhook_url_set: true }), true);
  assert.equal(NotifyCard.anySetUp(null), false);
});

test("describe has a line per service that is set up and sent something", () => {
  const lines = NotifyCard.describe({
    sending: false,
    telegram: { set_up: true, last: sent({ test: true }) },
    ntfy: { set_up: true, last: sent({ ok: false, error: "ntfy: forbidden, check the access token" }) },
  });
  assert.deepEqual(lines, [
    { text: "Telegram: test sent.", error: false },
    { text: "ntfy: last notification failed, forbidden, check the access token", error: true },
  ]);
  assert.deepEqual(NotifyCard.describe({ telegram: { set_up: false, last: sent() }, ntfy: { set_up: true, last: null } }), []);
  assert.equal(NotifyCard.describe({ telegram: { set_up: true, last: sent({ seconds_ago: 600 }) } })[0].text,
    "Telegram: last notification sent " + Format.ago(600) + ".");
});

test("a test is finished when every service that is set up reported on it", () => {
  const state = (ntfyLast) => ({ sending: false, telegram: { set_up: true, last: sent({ test: true }) }, ntfy: { set_up: true, last: ntfyLast } });
  assert.equal(NotifyCard.testFinished(state(null), 3000), false);
  assert.equal(NotifyCard.testFinished(state(sent({ test: false })), 3000), false);
  assert.equal(NotifyCard.testFinished(state(sent({ test: true, seconds_ago: 60 })), 3000), false);   // an older test
  assert.equal(NotifyCard.testFinished(state(sent({ test: true, seconds_ago: 1 })), 3000), true);
  assert.equal(NotifyCard.testFinished({ ...state(sent({ test: true })), sending: true }, 3000), false);
});

test("a refused webhook says the status without repeating the name", () => {
  const lines = NotifyCard.describe({ webhook: { set_up: true, last: sent({ ok: false, test: true, error: "The webhook answered 404" }) } });
  assert.deepEqual(lines, [{ text: "Webhook: test failed, answered 404", error: true }]);
});
