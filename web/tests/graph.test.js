const test = require("node:test");
const assert = require("node:assert/strict");
const Graph = require("../js/graph.js");

const series = (values, stepMs = 10000) => values.map((v, i) => ({ t: i * stepMs, v }));
const box = { width: 240, height: 46, mode: "off", target: 0, minimum: 0 };

test("layout needs two readings", () => {
  assert.equal(Graph.layout([], box), null);
  assert.equal(Graph.layout(series([20]), box), null);
  assert.equal(Graph.layout(series([20, null]), box), null);
  assert.notEqual(Graph.layout(series([20, 21]), box), null);
});

test("layout spans the full width", () => {
  const result = Graph.layout(series([20, 30, 40]), box);
  assert.match(result.line, /^M0 /);
  assert.match(result.line, /L240 [\d.]+$/);
});

test("a flat line sits in the middle", () => {
  const result = Graph.layout(series([20, 20]), box);
  assert.equal(result.line, "M0 23L240 23");
});

test("higher temperatures are drawn higher up", () => {
  const result = Graph.layout(series([20, 60]), box);
  const ys = result.line.match(/[\d.]+(?=L|$)/g).map(Number);
  assert.ok(ys[1] < ys[0]);
});

test("the target line is included in the scale", () => {
  const result = Graph.layout(series([20, 40, 60]), { ...box, mode: "target", target: 95 });
  assert.equal(result.target, 3.2);
  assert.equal(result.minimum, null);
});

test("range mode has a band below the target", () => {
  const result = Graph.layout(series([85, 90, 91]), { ...box, mode: "range", target: 93, minimum: 88 });
  assert.ok(result.minimum > result.target);
});

test("no guides without an alarm", () => {
  const result = Graph.layout(series([20, 30]), box);
  assert.equal(result.target, null);
  assert.equal(result.minimum, null);
});

test("a gap splits the line", () => {
  const result = Graph.layout(series([20, 21, null, 22, 23]), box);
  assert.equal(result.line.split("M").length - 1, 2);
  assert.equal(result.area.split("Z").length - 1, 2);
});

test("area closes along the bottom", () => {
  const result = Graph.layout(series([20, 30]), box);
  assert.match(result.area, /^M0 46L0 [\d.]+L240 [\d.]+L240 46Z$/);
});

test("the target line stays on canvas even below all plotted values", () => {
  const result = Graph.layout(series([60, 70]), { ...box, mode: "target", target: 40 });
  assert.ok(result.target >= 0 && result.target <= 46);
});

test("a lone point after a gap is drawn as a dot, not dropped", () => {
  const result = Graph.layout(series([20, 21, null, 22]), box);
  assert.equal(result.line.split("M").length - 1, 2);
  assert.match(result.line, /L240 [\d.]+$/);
});

test("downsample averages equal time slices", () => {
  const out = Graph.downsample(series([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]), 5);
  assert.deepEqual(out.map((p) => p.v), [0.5, 2.5, 4.5, 6.5, 8.5]);
});

test("downsample keeps gaps and short series", () => {
  assert.deepEqual(Graph.downsample(series([1, 2]), 5), series([1, 2]));
  const out = Graph.downsample(series([1, 1, null, null, 3, 3]), 3);
  assert.deepEqual(out.map((p) => p.v), [1, null, 3]);
});

test("downsample caps long series", () => {
  const long = series(Array.from({ length: 5000 }, (_, i) => 20 + i / 100));
  assert.ok(Graph.downsample(long, 240).length <= 240);
});

test("layout reports the highest and lowest measured values", () => {
  const result = Graph.layout(series([20, 45, 30]), box);
  assert.equal(result.highest, 45);
  assert.equal(result.lowest, 20);
});

test("highest and lowest ignore gaps and the target/minimum guides", () => {
  const result = Graph.layout(series([20, null, 45, 30]), { ...box, mode: "range", target: 95, minimum: 5 });
  assert.equal(result.highest, 45);
  assert.equal(result.lowest, 20);
});

test("layout reports the first and last point times", () => {
  const result = Graph.layout(series([20, 30, 40], 10000), box);
  assert.equal(result.startT, 0);
  assert.equal(result.endT, 20000);
});

test("layout reports the padded scale", () => {
  const result = Graph.layout(series([20, 20]), box);
  assert.ok(Math.abs(result.scaleLow - 17.1) < 1e-9);
  assert.ok(Math.abs(result.scaleHigh - 22.9) < 1e-9);
});

test("ticks are nice values inside the range", () => {
  assert.deepEqual(Graph.ticks(17.1, 22.9), [18, 20, 22]);
  assert.deepEqual(Graph.ticks(14, 101), [50, 100]);
  assert.deepEqual(Graph.ticks(0, 300), [0, 100, 200, 300]);
  assert.deepEqual(Graph.ticks(20.2, 21.4), [20.5, 21]);
});

test("forecastShare follows the time to target within 20-45 % of the width", () => {
  assert.equal(Graph.forecastShare(600000, -1), 0);
  assert.equal(Graph.forecastShare(600000, 0), 0);
  assert.equal(Graph.forecastShare(3600000, 600), 0.2);      // 10 min left of a 1 h cook: at least 20 %
  assert.equal(Graph.forecastShare(600000, 2100), 0.45);     // 35 min left after 10 min: at most 45 %
  assert.equal(Graph.forecastShare(3000000, 1500), 1500 / 4500);
});

test("a forecast runs from the newest reading to the target at the right edge", () => {
  const result = Graph.layout(series([20, 22, 24, 26]), { ...box, mode: "target", target: 74, etaSeconds: 2100 });
  assert.equal(result.nowX, 132);                            // 240 * (1 - 0.45)
  assert.deepEqual(result.now, { x: 132, y: Number(result.line.match(/([\d.]+) ([\d.]+)$/)[2]) });
  assert.equal(result.forecast, "M132 " + result.now.y + "L240 " + result.target);
});

test("no forecast without a time to target, outside target mode, or after a gap", () => {
  const opts = { ...box, mode: "target", target: 74 };
  assert.equal(Graph.layout(series([20, 22]), opts).forecast, null);
  assert.equal(Graph.layout(series([20, 22]), opts).nowX, 240);
  assert.equal(Graph.layout(series([20, 22]), { ...opts, mode: "range", minimum: 60, etaSeconds: 600 }).forecast, null);
  assert.equal(Graph.layout(series([20, 22, null]), { ...opts, etaSeconds: 600 }).forecast, null);
  assert.equal(Graph.layout(series([20, 22, null]), { ...opts, etaSeconds: 600 }).now, null);
});
