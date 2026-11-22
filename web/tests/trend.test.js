const test = require("node:test");
const assert = require("node:assert/strict");
const Trend = require("../js/trend.js");

test("tierPoints dates every value from age and interval", () => {
  const points = Trend.tierPoints({ interval: 60, age: 30, values: [200, null, 210] }, 1000000);
  assert.deepEqual(points, [
    { t: 850000, c: 20 },
    { t: 910000, c: null },
    { t: 970000, c: 21 },
  ]);
});

test("fromResponse uses the coarse tier per probe", () => {
  const response = { probes: [
    { probe_id: 2, coarse: { interval: 60, age: 0, values: [700] } },
    { probe_id: 5, coarse: { interval: 120, age: 0, values: [] } },
  ] };
  assert.deepEqual(Trend.fromResponse(response, 5000), { 2: [{ t: 5000, c: 70 }], 5: [] });
  assert.deepEqual(Trend.fromResponse(null, 5000), {});
});

test("append adds at most one point per 10 seconds", () => {
  const points = [];
  assert.equal(Trend.append(points, 20, 1000), true);
  assert.equal(Trend.append(points, 21, 10999), false);
  assert.equal(Trend.append(points, 22, 11000), true);
  assert.deepEqual(points, [{ t: 1000, c: 20 }, { t: 11000, c: 22 }]);
});

test("toCelsius converts fahrenheit and rejects non-numbers", () => {
  assert.equal(Trend.toCelsius(212, "fahrenheit"), 100);
  assert.equal(Trend.toCelsius(50, "celcius"), 50);
  assert.equal(Trend.toCelsius(NaN, "celcius"), null);
  assert.equal(Trend.toCelsius(null, "celcius"), null);
});

test("display converts to the display unit and keeps gaps", () => {
  const points = [{ t: 1, c: 100 }, { t: 2, c: null }];
  assert.deepEqual(Trend.display(points, "fahrenheit"), [{ t: 1, v: 212 }, { t: 2, v: null }]);
  assert.deepEqual(Trend.display(points, "celcius"), [{ t: 1, v: 100 }, { t: 2, v: null }]);
});

test("thin keeps the newest half and thins the oldest half", () => {
  const points = Array.from({ length: 10 }, (_, i) => ({ t: i, c: i }));
  Trend.thin(points, 8);
  assert.ok(points.length <= 8);
  assert.deepEqual(points.slice(-5), [{ t: 5, c: 5 }, { t: 6, c: 6 }, { t: 7, c: 7 }, { t: 8, c: 8 }, { t: 9, c: 9 }]);
  assert.deepEqual(points.slice(0, -5), [{ t: 0, c: 0 }, { t: 2, c: 2 }, { t: 4, c: 4 }]);
});

test("append caps the series length over a long cook", () => {
  const points = [];
  let t = 0;
  for (let i = 0; i < 5000; i++) {
    t += 10000;
    Trend.append(points, i, t);
  }
  assert.ok(points.length <= 4320);
});

test("append inserts a gap point after an offline period", () => {
  const points = [{ t: 0, c: 20 }];
  Trend.append(points, 25, 30001);   // more than 3 * 10000ms after the last point
  assert.deepEqual(points, [{ t: 0, c: 20 }, { t: 10000, c: null }, { t: 30001, c: 25 }]);
});

test("append does not double up a gap when the previous point is already a gap", () => {
  const points = [{ t: 0, c: 20 }, { t: 10000, c: null }];
  Trend.append(points, 25, 40001);
  assert.deepEqual(points, [{ t: 0, c: 20 }, { t: 10000, c: null }, { t: 40001, c: 25 }]);
});

test("append with the grill's minute history step adds no gap for a 70s old point", () => {
  const points = [{ t: 0, c: 20 }];
  assert.equal(Trend.append(points, 25, 70000, 60000), true);
  assert.deepEqual(points, [{ t: 0, c: 20 }, { t: 70000, c: 25 }]);
});

test("append with a minute history step still inserts a gap past 3 append intervals beyond it", () => {
  const points = [{ t: 0, c: 20 }];
  assert.equal(Trend.append(points, 25, 100000, 60000), true);
  assert.deepEqual(points, [{ t: 0, c: 20 }, { t: 60000, c: null }, { t: 100000, c: 25 }]);
});

test("append keeps the default 10s gap behaviour when no step is given", () => {
  const points = [{ t: 0, c: 20 }];
  Trend.append(points, 25, 30001);
  assert.deepEqual(points, [{ t: 0, c: 20 }, { t: 10000, c: null }, { t: 30001, c: 25 }]);
});

test("thin keeps points that touch a gap in the older half even at an odd index", () => {
  const points = Array.from({ length: 10 }, (_, i) => ({ t: i, c: i }));
  points[3].c = null;   // index 3 falls in the older half (indices 0-4) and is odd
  Trend.thin(points, 8);
  assert.deepEqual(points, [
    { t: 0, c: 0 }, { t: 2, c: 2 }, { t: 3, c: null }, { t: 4, c: 4 },
    { t: 5, c: 5 }, { t: 6, c: 6 }, { t: 7, c: 7 }, { t: 8, c: 8 }, { t: 9, c: 9 },
  ]);
});

test("version starts at 0 and bumps when a status appends a point", () => {
  global.Api = { get: async () => ({ probes: [] }) };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");
  assert.equal(FreshTrend.version(7), 0);
  FreshTrend.onStatus({ temperature_unit: "celcius", probes: [{ probe_id: 7, connected: true, temperature: 20 }] });
  assert.equal(FreshTrend.version(7), 1);
  delete global.Api;
});

test("clear empties the series and bumps the version", () => {
  global.Api = { get: async () => ({ probes: [] }) };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");
  FreshTrend.onStatus({ temperature_unit: "celcius", probes: [{ probe_id: 9, connected: true, temperature: 20 }] });
  assert.ok(FreshTrend.points(9, "celcius").length > 0);
  const versionBefore = FreshTrend.version(9);
  FreshTrend.clear(9);
  assert.deepEqual(FreshTrend.points(9, "celcius"), []);
  assert.notEqual(FreshTrend.version(9), versionBefore);
  delete global.Api;
});

test("clear notifies onChange listeners", () => {
  global.Api = { get: async () => ({ probes: [] }) };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");
  FreshTrend.onStatus({ temperature_unit: "celcius", probes: [{ probe_id: 4, connected: true, temperature: 20 }] });
  let calls = 0;
  FreshTrend.onChange(() => { calls++; });
  FreshTrend.clear(4);
  assert.equal(calls, 1);
  delete global.Api;
});

test("onStatus loads immediately on the first status", async () => {
  let calls = 0;
  global.Api = { get: async () => { calls++; return { probes: [] }; } };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");
  FreshTrend.onStatus({ temperature_unit: "celcius", probes: [] });
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(calls, 1);
  delete global.Api;
});

test("onStatus debounces reconnect-triggered reloads to at most one per 5s", async (t) => {
  t.mock.timers.enable({ apis: ["setTimeout", "Date"] });
  let calls = 0;
  global.Api = { get: async () => { calls++; return { probes: [] }; } };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");
  const flush = () => Promise.resolve().then(() => Promise.resolve());

  const statusWith = (connected) => ({
    temperature_unit: "celcius",
    probes: [{ probe_id: 1, connected, temperature: 20 }],
  });

  FreshTrend.onStatus(statusWith(true));   // first status: loads immediately
  await flush();
  assert.equal(calls, 1);

  FreshTrend.onStatus(statusWith(false));
  FreshTrend.onStatus(statusWith(true));   // reconnect within 5s: scheduled, not dropped
  FreshTrend.onStatus(statusWith(false));
  FreshTrend.onStatus(statusWith(true));   // another reconnect within 5s: still only one scheduled
  await flush();
  assert.equal(calls, 1);

  t.mock.timers.tick(5000);
  await flush();
  assert.equal(calls, 2);

  global.Api = undefined;
  t.mock.timers.reset();
});

test("load retries once after a failure, but not a second time", (t) => {
  t.mock.timers.enable({ apis: ["setTimeout"] });
  let calls = 0;
  global.Api = { get: async () => { calls++; throw new Error("offline"); } };
  delete require.cache[require.resolve("../js/trend.js")];
  const FreshTrend = require("../js/trend.js");

  return FreshTrend.load()
    .then(() => Promise.resolve())
    .then(() => {
      assert.equal(calls, 1);
      t.mock.timers.tick(5000);
      return new Promise((resolve) => setImmediate(resolve));
    })
    .then(() => {
      assert.equal(calls, 2);   // the one retry
      t.mock.timers.tick(5000);
      return new Promise((resolve) => setImmediate(resolve));
    })
    .then(() => {
      assert.equal(calls, 2);   // no retry of the retry
      global.Api = undefined;
      t.mock.timers.reset();
    });
});

test("csv has a header with the name and unit, one decimal, and empty gaps", () => {
  const t = new Date(2026, 10, 22, 14, 5, 30).getTime();
  const text = Trend.csv([{ t, v: 71.44 }, { t: t + 10000, v: null }, { t: t + 20000, v: 72 }], "celcius", 'Mom\'s "roast"');
  assert.equal(text, '"Time","Mom\'s ""roast"" (°C)"\r\n2026-11-22 14:05:30,71.4\r\n2026-11-22 14:05:40,\r\n2026-11-22 14:05:50,72.0\r\n');
  assert.match(Trend.csv([], "fahrenheit", "Grill"), /^"Time","Grill \(°F\)"\r\n$/);
});

test("csvName is a safe file name with the date and time", () => {
  const t = new Date(2026, 10, 22, 9, 7).getTime();
  assert.equal(Trend.csvName("Pulled pork", t), "grilly-plus-pulled-pork-2026-11-22-0907.csv");
  assert.equal(Trend.csvName("Crème brûlée!", t), "grilly-plus-creme-brulee-2026-11-22-0907.csv");
  assert.equal(Trend.csvName("***", t), "grilly-plus-probe-2026-11-22-0907.csv");
});
