const test = require("node:test");
const assert = require("node:assert/strict");
const StepRows = require("../js/steprows.js");

test("a new step goes 20 °C (35 °F) below the target, or 5 below the last step", () => {
  assert.equal(StepRows.defaultTemperature([], 95, "celcius"), 75);
  assert.equal(StepRows.defaultTemperature([{ temperature: 74 }], 95, "celcius"), 69);
  assert.equal(StepRows.defaultTemperature([{ temperature: 80 }], 95, "celcius"), 75);
  assert.equal(StepRows.defaultTemperature([], 203, "fahrenheit"), 168);
  assert.equal(StepRows.defaultTemperature([], 10, "celcius"), 1);
});

test("steps are sorted by temperature", () => {
  assert.deepEqual(StepRows.sorted([{ action: "Spritz", temperature: 82 }, { action: "Wrap", temperature: 74 }]).map((s) => s.action),
    ["Wrap", "Spritz"]);
});

test("a third step goes 5 below the lowest one, not on top of it", () => {
  assert.equal(StepRows.defaultTemperature([{ temperature: 69 }, { temperature: 74 }], 95, "celcius"), 64);
});

// The widget in a fake DOM
const FakeDom = require("./fakedom.js");
global.Format = require("../js/format.js");

function widget(steps, target) {
  FakeDom.install();
  const container = document.createElement("div");
  document.body.append(container);
  const saved = [];
  const errors = [];
  const rows = StepRows.create(container, {
    unit: () => "celcius",
    onChange: (list) => saved.push(list),
    onError: (message) => errors.push(message),
  });
  rows.set(steps, target);
  const rowEls = () => container.querySelectorAll(".step-row");
  const row = (action) => rowEls().find((el) => el.querySelector(".step-action").value === action);
  return { rows, saved, errors, rowEls, row, container };
}

function type(input, value) {
  input.focus();
  input.value = String(value);
  input.dispatch("change");
}

test("the cross removes the step's row, also though the cross has focus", () => {
  const w = widget([{ action: "Wrap", temperature: 74 }, { action: "Spritz", temperature: 82 }], 95);
  const cross = w.row("Spritz").querySelector(".icon-button");
  cross.focus();
  cross.dispatch("click");
  assert.deepEqual(w.saved.at(-1), [{ action: "Wrap", temperature: 74 }]);
  assert.equal(w.rowEls().length, 1);
});

test("after an edit re-sorts the steps, each cross still removes its own step", () => {
  const w = widget([{ action: "Wrap", temperature: 74 }, { action: "Spritz", temperature: 82 }], 95);
  type(w.row("Wrap").querySelectorAll("input")[1], 90);
  assert.deepEqual(w.rowEls().map((el) => el.querySelector(".step-action").value), ["Spritz", "Wrap"]);
  w.row("Wrap").querySelector(".icon-button").dispatch("click");
  assert.deepEqual(w.saved.at(-1), [{ action: "Spritz", temperature: 82 }]);
});

test("an answer from the grill while a step is being typed in keeps the edit", () => {
  const w = widget([{ action: "Wrap", temperature: 74 }], 95);
  const temperature = w.row("Wrap").querySelectorAll("input")[1];
  temperature.focus();
  temperature.value = "70";
  w.rows.set([{ action: "Wrap", temperature: 74 }], 96);    // the answer to a target change
  temperature.dispatch("change");
  assert.deepEqual(w.saved.at(-1), [{ action: "Wrap", temperature: 70 }]);
  assert.equal(w.rowEls().length, 1);
});

test("a step typed at or above the target is refused right away", () => {
  const w = widget([{ action: "Wrap", temperature: 74 }], 80);
  const temperature = w.row("Wrap").querySelectorAll("input")[1];
  type(temperature, 85);
  assert.equal(w.saved.length, 0);
  assert.equal(Number(temperature.value), 74);
  assert.deepEqual(w.errors, ["A step should be below the target"]);
});

test("a step the target dropped below shows as skipped, and can still be removed next to others", () => {
  const w = widget([{ action: "Wrap", temperature: 74 }, { action: "Spritz", temperature: 82 }], 95);
  w.rows.setTarget(80);
  assert.ok(w.row("Spritz").classList.contains("ignored"));
  assert.ok(!w.row("Wrap").classList.contains("ignored"));
  w.rows.setTarget(95);
  assert.ok(!w.row("Spritz").classList.contains("ignored"));
});
