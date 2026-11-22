// Food types: the icon a probe shows on its card and on the grill's screen. The keys are the ones the
// grill accepts as type_icon; the line icons are the i-food-* symbols in index.html.
const Food = (() => {
  const TYPES = [
    { key: "beef", label: "Beef", emoji: "🐄" },
    { key: "lamb", label: "Lamb", emoji: "🐑" },
    { key: "pork", label: "Pork", emoji: "🐖" },
    { key: "poultry", label: "Poultry", emoji: "🐔" },
    { key: "fish", label: "Fish", emoji: "🐟" },
    { key: "sausage", label: "Sausage", emoji: "🌭" },
    { key: "vegetables", label: "Vegetables", emoji: "🥦" },
    { key: "pit", label: "Pit or smoker", emoji: "🔥" },
    { key: "other", label: "Other", emoji: "🍴" },
  ];

  function find(key) {
    return TYPES.find((type) => type.key === key) || null;
  }

  function emoji(key) {
    const type = find(key);
    return type ? type.emoji : "";
  }

  function label(key) {
    const type = find(key);
    return type ? type.label : "None";
  }

  // An svg element for the type's line icon, null for no type
  function icon(key, className = "icon food-icon") {
    if (!find(key) || typeof document === "undefined") return null;
    const ns = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(ns, "svg");
    svg.setAttribute("class", className);
    svg.setAttribute("aria-hidden", "true");
    const use = document.createElementNS(ns, "use");
    use.setAttribute("href", "#i-food-" + key);
    svg.append(use);
    return svg;
  }

  return { TYPES, find, emoji, label, icon };
})();

if (typeof module === "object" && module.exports) module.exports = Food;
