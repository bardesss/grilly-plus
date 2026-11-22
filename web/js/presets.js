// Food presets for the probe editor: pick a food and the target temperature fills in. Each preset
// has its own °C and °F value, so both units get round numbers instead of a converted 62.8°.
// Safe temperatures follow the USDA; doneness levels follow common practice. The type sets the
// probe's food icon (Food.TYPES).
const Presets = (() => {
  const LIST = [
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, rare", c: 52, f: 125 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium-rare", c: 57, f: 135 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium", c: 63, f: 145 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium-well", c: 68, f: 155 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, well done", c: 71, f: 160 },
    { type: "beef", group: "Beef", name: "Roast beef", label: "Roast beef, medium-rare", c: 55, f: 130 },
    { type: "beef", group: "Beef", name: "Brisket", label: "Brisket", c: 95, f: 203 },
    { type: "beef", group: "Beef", name: "Short ribs", label: "Short ribs", c: 95, f: 203 },
    { type: "beef", group: "Beef", name: "Burger", label: "Burger, minced meat", c: 71, f: 160 },
    { type: "lamb", group: "Lamb", name: "Lamb", label: "Leg of lamb, medium", c: 60, f: 140 },
    { type: "lamb", group: "Lamb", name: "Lamb shoulder", label: "Pulled lamb shoulder", c: 90, f: 195 },
    { type: "pork", group: "Pork", name: "Pork chop", label: "Pork chop or tenderloin", c: 63, f: 145 },
    { type: "pork", group: "Pork", name: "Pulled pork", label: "Pulled pork", c: 95, f: 203 },
    { type: "pork", group: "Pork", name: "Ribs", label: "Spare ribs", c: 93, f: 200 },
    { type: "sausage", group: "Pork", name: "Sausage", label: "Sausage", c: 71, f: 160 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Chicken breast", c: 74, f: 165 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Chicken thighs or wings", c: 80, f: 175 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Whole chicken", c: 74, f: 165 },
    { type: "poultry", group: "Poultry", name: "Turkey", label: "Turkey", c: 74, f: 165 },
    { type: "poultry", group: "Poultry", name: "Duck", label: "Duck breast, pink", c: 57, f: 135 },
    { type: "fish", group: "Fish", name: "Salmon", label: "Salmon, glassy", c: 50, f: 122 },
    { type: "fish", group: "Fish", name: "Fish", label: "Fish, cooked through", c: 63, f: 145 },
  ];

  function target(preset, unit) {
    return unit === "fahrenheit" ? preset.f : preset.c;
  }

  // The groups in order, each with its presets
  function groups() {
    const result = [];
    LIST.forEach((preset, index) => {
      let group = result.find((g) => g.name === preset.group);
      if (!group) { group = { name: preset.group, items: [] }; result.push(group); }
      group.items.push({ index, ...preset });
    });
    return result;
  }

  // A preset may rename the probe when the user hasn't named it: the default "Probe 3", empty,
  // or a name an earlier preset put there
  function shouldRename(name, probeId) {
    const current = (name || "").trim();
    return current === "" || current === "Probe " + probeId || LIST.some((preset) => preset.name === current);
  }

  return { LIST, target, groups, shouldRename };
})();

if (typeof module === "object" && module.exports) module.exports = Presets;
