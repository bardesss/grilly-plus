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
