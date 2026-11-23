// Food presets for the probe editor: pick a food and the target temperature fills in. Each preset
// has its own °C and °F value, so both units get round numbers instead of a converted 62.8°.
// Safe temperatures follow the USDA; doneness levels follow common practice. The type sets the
// probe's food icon (Food.TYPES). rest is the rest timer in minutes that starts at the target.
// steps are moments before the target, like wrapping a brisket at 74 °C.
const Presets = (() => {
  const WRAP = [{ action: "Wrap", c: 74, f: 165 }];
  const LIST = [
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, rare", c: 52, f: 125, rest: 5 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium-rare", c: 57, f: 135, rest: 5 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium", c: 63, f: 145, rest: 5 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, medium-well", c: 68, f: 155, rest: 5 },
    { type: "beef", group: "Beef", name: "Steak", label: "Steak, well done", c: 71, f: 160, rest: 5 },
    { type: "beef", group: "Beef", name: "Roast beef", label: "Roast beef, medium-rare", c: 55, f: 130, rest: 15 },
    { type: "beef", group: "Beef", name: "Brisket", label: "Brisket", c: 95, f: 203, rest: 60, steps: WRAP },
    { type: "beef", group: "Beef", name: "Short ribs", label: "Short ribs", c: 95, f: 203, rest: 30, steps: WRAP },
    { type: "beef", group: "Beef", name: "Burger", label: "Burger, minced meat", c: 71, f: 160, rest: 0 },
    { type: "lamb", group: "Lamb", name: "Lamb", label: "Leg of lamb, medium", c: 60, f: 140, rest: 15 },
    { type: "lamb", group: "Lamb", name: "Lamb shoulder", label: "Pulled lamb shoulder", c: 90, f: 195, rest: 30, steps: WRAP },
    { type: "pork", group: "Pork", name: "Pork chop", label: "Pork chop or tenderloin", c: 63, f: 145, rest: 5 },
    { type: "pork", group: "Pork", name: "Pulled pork", label: "Pulled pork", c: 95, f: 203, rest: 30, steps: WRAP },
    { type: "pork", group: "Pork", name: "Ribs", label: "Spare ribs", c: 93, f: 200, rest: 10, steps: WRAP },
    { type: "sausage", group: "Pork", name: "Sausage", label: "Sausage", c: 71, f: 160, rest: 0 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Chicken breast", c: 74, f: 165, rest: 5 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Chicken thighs or wings", c: 80, f: 175, rest: 0 },
    { type: "poultry", group: "Poultry", name: "Chicken", label: "Whole chicken", c: 74, f: 165, rest: 15 },
    { type: "poultry", group: "Poultry", name: "Turkey", label: "Turkey", c: 74, f: 165, rest: 30 },
    { type: "poultry", group: "Poultry", name: "Duck", label: "Duck breast, pink", c: 57, f: 135, rest: 5 },
    { type: "fish", group: "Fish", name: "Salmon", label: "Salmon, glassy", c: 50, f: 122, rest: 0 },
    { type: "fish", group: "Fish", name: "Fish", label: "Fish, cooked through", c: 63, f: 145, rest: 0 },
  ];

  // Ids for hiding a built-in preset: its label in lower case with dashes ("steak-medium-rare")
  LIST.forEach((preset) => { preset.id = preset.label.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, ""); });

  const OWN_GROUP = "My presets";
  const MAX_OWN = 30;

  // Your own presets and the hidden built-in ids, as the grill keeps them (GET /api/presets)
  let own = [];
  let hidden = [];
  let version = 0;
  const listeners = [];

  function target(preset, unit) {
    return unit === "fahrenheit" ? preset.f : preset.c;
  }

  // The range minimum, 0 for a target preset
  function minimum(preset, unit) {
    return (unit === "fahrenheit" ? preset.min_f : preset.min_c) || 0;
  }

  // Minutes of rest after the target, 0 for none and for a range
  function rest(preset) {
    return preset.min_c ? 0 : preset.rest || 0;
  }

  // The preset's steps as [{action, temperature}] in the unit, [] for none and for a range
  function steps(preset, unit) {
    if (preset.min_c || !Array.isArray(preset.steps)) return [];
    return preset.steps.map((step) => ({ action: step.action, temperature: unit === "fahrenheit" ? step.f : step.c }));
  }

  function label(preset) {
    return preset.label || preset.name;
  }

  // "Brisket · 95°C" or "Pit · 107–121°C", with the type's emoji in front
  function optionText(preset, unit, symbol) {
    const min = minimum(preset, unit);
    const temps = (min ? min + "–" : "") + target(preset, unit) + symbol;
    const emoji = typeof Food !== "undefined" ? Food.emoji(preset.type) : "";
    return (emoji ? emoji + " " : "") + label(preset) + " · " + temps;
  }

  // The groups the Food list shows: your own first (in their group, or "My presets"), then the
  // built-in ones that aren't hidden. value finds the preset again with byValue().
  function groups() {
    const result = [];
    const add = (name, item) => {
      let group = result.find((g) => g.name === name);
      if (!group) { group = { name, items: [] }; result.push(group); }
      group.items.push(item);
    };
    own.forEach((preset, index) => add(preset.group || OWN_GROUP, { value: "own:" + index, ...preset }));
    LIST.forEach((preset, index) => {
      if (!hidden.includes(preset.id)) add(preset.group, { value: "builtin:" + index, ...preset });
    });
    return result;
  }

  function byValue(value) {
    const [kind, index] = String(value).split(":");
    if (kind === "own") return own[Number(index)] || null;
    if (kind === "builtin") return LIST[Number(index)] || null;
    return null;
  }

  // Both units from the one typed in, rounded to whole degrees
  function convert(value, unit) {
    return unit === "fahrenheit"
      ? { c: Math.round((value - 32) / 1.8), f: Math.round(value) }
      : { c: Math.round(value), f: Math.round(value * 1.8 + 32) };
  }

  // A preset of your own from what the editor or the form holds, temperatures in `unit`
  function make({ name, group, type, target: max, minimum: min, rest: minutes, steps: list, unit }) {
    const top = convert(max, unit);
    const preset = { name: name.trim().slice(0, 24), c: top.c, f: top.f };
    if (group && group.trim() && group.trim() !== OWN_GROUP) preset.group = group.trim().slice(0, 20);
    if (type) preset.type = type;
    if (min > 0) {
      const low = convert(min, unit);
      preset.min_c = Math.min(low.c, top.c - 1);
      preset.min_f = Math.min(low.f, top.f - 1);
    } else if (minutes > 0) {
      preset.rest = Math.min(180, Math.round(minutes));
    }
    // Steps in both units, kept below the target in each (rounding may land one on it)
    if (!(min > 0) && Array.isArray(list) && list.length) {
      preset.steps = list.map((step) => {
        const both = convert(step.temperature, unit);
        return { action: step.action, c: Math.max(1, Math.min(both.c, top.c - 1)), f: Math.max(1, Math.min(both.f, top.f - 1)) };
      }).sort((a, b) => a.c - b.c).slice(0, 3);
    }
    return preset;
  }

  function set(data) {
    own = data && Array.isArray(data.presets) ? data.presets : [];
    hidden = data && Array.isArray(data.hidden) ? data.hidden : [];
    version++;
    listeners.forEach((listener) => listener());
  }

  function get() {
    return { presets: own.slice(), hidden: hidden.slice() };
  }

  function onChange(listener) { listeners.push(listener); }
  function getVersion() { return version; }

  async function load() {
    try { set(await Api.get("/api/presets", 5000)); } catch (error) { /* the built-in ones still work */ }
  }

  // change: { presets: [...] } and/or { hidden: [...] }, as the grill should store them
  async function save(change) {
    set(await Api.post("/api/presets", change));
  }

  // A preset may rename the probe when the user hasn't named it: the default "Probe 3", empty,
  // or a name an earlier preset put there
  function shouldRename(name, probeId) {
    const current = (name || "").trim();
    return current === "" || current === "Probe " + probeId
      || LIST.some((preset) => preset.name === current) || own.some((preset) => preset.name === current);
  }

  return {
    LIST, OWN_GROUP, MAX_OWN, target, minimum, rest, steps, label, optionText, groups, byValue, convert, make,
    set, get, onChange, version: getVersion, load, save, shouldRename,
  };
})();

if (typeof module === "object" && module.exports) module.exports = Presets;
