const test = require("node:test");
const assert = require("node:assert/strict");
const Format = require("../js/format.js");

const probe = (temperature, target, minimum = 0) =>
  ({ temperature, target_temperature: target, minimum_temperature: minimum });

test("temperature has one decimal and the unit", () => {
  assert.equal(Format.temperature(71.44, "celcius"), "71.4°C");
  assert.equal(Format.temperature(160, "fahrenheit"), "160.0°F");
  assert.equal(Format.temperature(NaN, "celcius"), "–");
  assert.equal(Format.temperature(null, "celcius"), "–");
  assert.equal(Format.number(3), "3.0");
});

test("signal maps rssi to bars and a label", () => {
  assert.deepEqual(Format.signal(-50, true), { bars: 4, label: "Good", hotspot: false });
  assert.deepEqual(Format.signal(-60, true), { bars: 3, label: "Good", hotspot: false });
  assert.deepEqual(Format.signal(-70, true), { bars: 2, label: "Fair", hotspot: false });
  assert.deepEqual(Format.signal(-80, true), { bars: 1, label: "Weak", hotspot: false });
  assert.deepEqual(Format.signal(-95, true), { bars: 0, label: "Weak", hotspot: false });
  assert.deepEqual(Format.signal(-50, false), { bars: 0, label: "Hotspot", hotspot: true });
});

test("battery clamps the percentage and picks a level", () => {
  assert.deepEqual(Format.battery(82), { pct: 82, level: "good" });
  assert.deepEqual(Format.battery(50), { pct: 50, level: "warn" });
  assert.deepEqual(Format.battery(20), { pct: 20, level: "warn" });
  assert.deepEqual(Format.battery(19), { pct: 19, level: "bad" });
  assert.deepEqual(Format.battery(65535), { pct: 100, level: "good" });
  assert.deepEqual(Format.battery(-3), { pct: 0, level: "bad" });
});

test("probe status for no alarm, target and range", () => {
  assert.deepEqual(Format.probeStatus(probe(71.4, 0)), { kind: "none", text: "", progress: null });
  assert.deepEqual(Format.probeStatus(probe(71.4, 95)), { kind: "to-go", text: "+23.6° to go", progress: 71.4 / 95 });
  assert.deepEqual(Format.probeStatus(probe(95.2, 95)), { kind: "ready", text: "Ready", progress: 1 });
  assert.deepEqual(Format.probeStatus(probe(90, 93, 88)), { kind: "in-range", text: "In range", progress: 1 });
  assert.equal(Format.probeStatus(probe(80, 93, 88)).text, "Too low");
  assert.equal(Format.probeStatus(probe(99, 93, 88)).text, "Too high");
  assert.equal(Format.probeStatus(probe(-5, 95)).progress, 0);
});

test("alarm mode and label", () => {
  assert.equal(Format.alarmMode(probe(20, 0)), "off");
  assert.equal(Format.alarmMode(probe(20, 95)), "target");
  assert.equal(Format.alarmMode(probe(20, 93, 88)), "range");
  assert.equal(Format.alarmLabel(probe(20, 0)), "");
  assert.equal(Format.alarmLabel(probe(20, 95)), "→ 95°");
  assert.equal(Format.alarmLabel(probe(20, 93, 88)), "88–93°");
});

test("duration", () => {
  assert.equal(Format.duration(0), "");
  assert.equal(Format.duration(30), "<1m");
  assert.equal(Format.duration(720), "12m");
  assert.equal(Format.duration(18720), "5h 12m");
});

test("ago formats now and durations in the past", () => {
  assert.equal(Format.ago(0), "now");
  assert.equal(Format.ago(59), "now");
  assert.equal(Format.ago(250), "4m ago");
  assert.equal(Format.ago(7800), "2h 10m ago");
});

test("empty sockets are grouped into ranges", () => {
  assert.equal(Format.emptySockets([]), "");
  assert.equal(Format.emptySockets([8]), "Socket 8 empty");
  assert.equal(Format.emptySockets([4, 5, 6, 7, 8]), "Sockets 4–8 empty");
  assert.equal(Format.emptySockets([2, 4, 5, 6, 7, 8]), "Sockets 2, 4–8 empty");
  assert.equal(Format.emptySockets([1, 3]), "Sockets 1, 3 empty");
});

test("eta formats the time to target and hides unknown", () => {
  assert.equal(Format.eta(-1), "");
  assert.equal(Format.eta(undefined), "");
  assert.equal(Format.eta(30), "ready in <1m");
  assert.equal(Format.eta(600), "ready in ~10m");
  assert.equal(Format.eta(4800), "ready in ~1h 20m");
  assert.equal(Format.eta(3599), "ready in ~1h 0m");
});

test("changed fields", () => {
  assert.deepEqual(Format.changedFields({ a: 1, b: "x" }, { a: 1, b: "y", c: true }), { b: "y", c: true });
  assert.deepEqual(Format.changedFields({ a: 1 }, { a: 1 }), {});
});

test("off reason labels include the low battery switch-off", () => {
  assert.equal(Format.offReasonText("low_battery"), "Battery empty");
  assert.equal(Format.offReasonText("something_new"), "Unknown");
});

test("checkedAgo words", () => {
  assert.equal(Format.checkedAgo(-1), "Never checked");
  assert.equal(Format.checkedAgo(undefined), "Never checked");
  assert.equal(Format.checkedAgo(5), "checked just now");
  assert.equal(Format.checkedAgo(59), "checked just now");
  assert.equal(Format.checkedAgo(300), "checked 5 min ago");
  assert.equal(Format.checkedAgo(7300), "checked 2 h ago");
  assert.equal(Format.checkedAgo(3 * 86400 + 100), "checked 3 days ago");
});

test("updateStatus kinds and texts", () => {
  assert.deepEqual(Format.updateStatus({ state: "checking" }), { kind: "checking", text: "Checking GitHub…" });
  assert.deepEqual(Format.updateStatus({ state: "error", error: "No network" }), { kind: "error", text: "Couldn't check: No network" });
  assert.deepEqual(Format.updateStatus({ state: "idle", available: true, latest: "26.10.03" }), { kind: "available", text: "26.10.03 is available" });
  assert.deepEqual(Format.updateStatus({ state: "idle", available: false, latest: "26.09.28", checked_seconds_ago: 7300 }),
    { kind: "current", text: "You have the newest version · checked 2 h ago" });
  assert.deepEqual(Format.updateStatus({ state: "idle", latest: "", checked_seconds_ago: -1 }), { kind: "unknown", text: "Not checked yet" });
});
