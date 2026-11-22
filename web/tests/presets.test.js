const test = require("node:test");
const assert = require("node:assert/strict");
const Presets = require("../js/presets.js");

test("every preset has a sensible temperature in both units", () => {
  for (const preset of Presets.LIST) {
    assert.ok(preset.c >= 45 && preset.c <= 100, preset.label);
    // The °F value is the °C value converted, give or take rounding to a round number
    assert.ok(Math.abs(preset.c * 1.8 + 32 - preset.f) <= 3, preset.label);
  }
});

test("target follows the unit", () => {
  const brisket = Presets.LIST.find((p) => p.label === "Brisket");
  assert.equal(Presets.target(brisket, "celcius"), 95);
  assert.equal(Presets.target(brisket, "fahrenheit"), 203);
});

test("poultry is at least the USDA safe temperature", () => {
  for (const preset of Presets.LIST.filter((p) => p.group === "Poultry" && p.name !== "Duck")) {
    assert.ok(preset.c >= 74 && preset.f >= 165, preset.label);
  }
});

test("groups keep the list order and the index into the list", () => {
  const groups = Presets.groups();
  assert.deepEqual(groups.map((g) => g.name), ["Beef and lamb", "Pork", "Poultry", "Fish"]);
  const flat = groups.flatMap((g) => g.items);
  assert.equal(flat.length, Presets.LIST.length);
  flat.forEach((item, i) => assert.equal(item.index, i));
});

test("a preset renames only a probe without a name of its own", () => {
  assert.equal(Presets.shouldRename("Probe 3", 3), true);
  assert.equal(Presets.shouldRename("", 1), true);
  assert.equal(Presets.shouldRename("  ", 1), true);
  assert.equal(Presets.shouldRename("Brisket", 2), true);      // an earlier preset's name
  assert.equal(Presets.shouldRename("Probe 3", 4), false);     // someone named probe 4 "Probe 3"
  assert.equal(Presets.shouldRename("Grandma's roast", 1), false);
});
