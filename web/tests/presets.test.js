const test = require("node:test");
const assert = require("node:assert/strict");
global.Food = require("../js/food.js");   // the browser has it as a global too
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

test("without presets of your own, the groups are the built-in ones in order", () => {
  Presets.set(null);
  const groups = Presets.groups();
  assert.deepEqual(groups.map((g) => g.name), ["Beef", "Lamb", "Pork", "Poultry", "Fish"]);
  const flat = groups.flatMap((g) => g.items);
  assert.equal(flat.length, Presets.LIST.length);
  flat.forEach((item) => assert.equal(Presets.byValue(item.value).label, item.label));
});

test("your own presets come first, and hidden built-in ones are left out", () => {
  Presets.set({
    presets: [{ name: "Pit", type: "pit", c: 121, f: 250, min_c: 107, min_f: 225 },
              { name: "Flat", group: "BBQ", type: "beef", c: 93, f: 200 }],
    hidden: ["brisket", "salmon-glassy"],
  });
  const groups = Presets.groups();
  assert.deepEqual(groups.map((g) => g.name).slice(0, 3), ["My presets", "BBQ", "Beef"]);
  const labels = groups.flatMap((g) => g.items).map((i) => i.label || i.name);
  assert.ok(!labels.includes("Brisket") && !labels.includes("Salmon, glassy"));
  assert.equal(Presets.byValue("own:0").name, "Pit");
  assert.equal(Presets.optionText(Presets.byValue("own:0"), "celcius", "°C"), "🔥 Pit · 107–121°C");
  assert.equal(Presets.shouldRename("Flat", 1), true);
  Presets.set(null);
});

test("make converts to both units and keeps the range below the target", () => {
  assert.deepEqual(Presets.make({ name: " Flat ", group: "", type: "beef", target: 93, minimum: 0, unit: "celcius" }),
    { name: "Flat", type: "beef", c: 93, f: 199 });
  assert.deepEqual(Presets.make({ name: "Pit", group: "My presets", type: "", target: 250, minimum: 225, unit: "fahrenheit" }),
    { name: "Pit", c: 121, f: 250, min_c: 107, min_f: 225 });
});

test("built-in ids are short, unique and only letters, digits and dashes", () => {
  const ids = Presets.LIST.map((p) => p.id);
  assert.equal(new Set(ids).size, ids.length);
  ids.forEach((id) => assert.match(id, /^[a-z0-9-]{1,24}$/));
});

test("a preset renames only a probe without a name of its own", () => {
  assert.equal(Presets.shouldRename("Probe 3", 3), true);
  assert.equal(Presets.shouldRename("", 1), true);
  assert.equal(Presets.shouldRename("  ", 1), true);
  assert.equal(Presets.shouldRename("Brisket", 2), true);      // an earlier preset's name
  assert.equal(Presets.shouldRename("Probe 3", 4), false);     // someone named probe 4 "Probe 3"
  assert.equal(Presets.shouldRename("Grandma's roast", 1), false);
});

test("every preset has a food type the grill knows", () => {
  const Food = require("../js/food.js");
  for (const preset of Presets.LIST) assert.ok(Food.find(preset.type), preset.label);
  assert.equal(Presets.LIST.find((p) => p.name === "Sausage").type, "sausage");
});
