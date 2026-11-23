// Steps of a probe or a preset ("Wrap at 74°"): rows of an action and a temperature, an ✕ per row
// and "+ Add a step". Nothing but that link shows until a step is added.
const StepRows = (() => {
  const MAX = 3;
  const ACTIONS = ["Wrap", "Spritz", "Sauce", "Flip", "Check", "Baste"];

  function sorted(list) {
    return list.slice().sort((a, b) => a.temperature - b.temperature);
  }

  // A new step: 20 °C (35 °F) below the target, or 5 below the lowest step if that is lower
  function defaultTemperature(list, target, unit) {
    let t = target - (unit === "fahrenheit" ? 35 : 20);
    if (list.length) t = Math.min(t, Math.min(...list.map((s) => s.temperature)) - 5);
    return Math.max(1, Math.round(t));
  }

  const plain = (list) => list.map(({ action, temperature }) => ({ action, temperature }));

  // options: { unit: () => "celcius"|"fahrenheit", onChange(list), onError(message) }
  function create(container, options) {
    let list = [];
    let target = 0;
    // One row per step object. Rows are updated in place and moved, never rebuilt for an edit, so a
    // click on a cross right after an edit still lands and the field being typed in keeps its text.
    const rowOf = new Map();
    const rows = document.createElement("div");
    rows.className = "step-rows";
    const add = document.createElement("button");
    add.type = "button";
    add.className = "link-button step-add";
    container.append(rows, add);

    if (!document.getElementById("step-actions")) {
      const datalist = document.createElement("datalist");
      datalist.id = "step-actions";
      ACTIONS.forEach((action) => datalist.append(new Option(action)));
      document.body.append(datalist);
    }

    // Typing in one of the rows: an answer from the grill mustn't swap the steps under it
    function typing() {
      const active = document.activeElement;
      return Boolean(active) && active.tagName === "INPUT" && rows.contains(active);
    }

    function changed() {
      list = sorted(list);
      render();
      options.onChange(plain(list));
    }

    function makeRow(step) {
      const el = document.createElement("div");
      el.className = "step-row";
      const action = document.createElement("input");
      action.className = "step-action";
      action.setAttribute("list", "step-actions");
      action.maxLength = 16;
      action.addEventListener("change", () => {
        const text = action.value.trim();
        if (text && text !== step.action) { step.action = text; changed(); } else { action.value = step.action; }
      });
      const at = document.createElement("span");
      at.className = "step-at";
      at.textContent = "at";
      // A plain number field: a stepper doesn't fit next to the action and the cross on a phone
      const box = document.createElement("label");
      box.className = "step-temperature";
      const temperature = document.createElement("input");
      temperature.type = "number";
      temperature.inputMode = "numeric";
      temperature.min = "1";
      temperature.addEventListener("change", () => {
        const value = Math.round(Number(temperature.value));
        const valid = value >= 1 && value <= 999;
        // A step the target dropped below may stay where it is, but a new value has to be below it
        const tooHigh = valid && target > 0 && value >= target && value !== step.temperature;
        if (valid && !tooHigh && value !== step.temperature) {
          step.temperature = value;
          changed();
          return;
        }
        temperature.value = step.temperature;
        if (tooHigh && options.onError) options.onError("A step should be below the target");
      });
      box.append(temperature, Format.unitSymbol(options.unit()));
      const remove = document.createElement("button");
      remove.type = "button";
      remove.className = "icon-button";
      remove.innerHTML = '<svg class="icon"><use href="#i-close"/></svg>';
      remove.addEventListener("click", () => {
        const index = list.indexOf(step);
        if (index >= 0) { list.splice(index, 1); changed(); }
      });
      const hint = document.createElement("p");
      hint.className = "hint step-hint";
      hint.textContent = "Above the target, so it's skipped";
      el.append(action, at, box, remove, hint);
      el.parts = { action, temperature, remove, hint };
      return el;
    }

    function update(el, step, index) {
      const { action, temperature, remove, hint } = el.parts;
      if (document.activeElement !== action) action.value = step.action;
      if (document.activeElement !== temperature) temperature.value = step.temperature;
      action.setAttribute("aria-label", "Step " + (index + 1) + " action");
      temperature.setAttribute("aria-label", "Step " + (index + 1) + " temperature");
      remove.setAttribute("aria-label", "Remove step " + (index + 1));
      const ignored = target > 0 && step.temperature >= target;
      el.classList.toggle("ignored", ignored);
      hint.hidden = !ignored;
    }

    function render() {
      for (const [step, el] of rowOf) {
        if (!list.includes(step)) { el.remove(); rowOf.delete(step); }
      }
      const active = document.activeElement;
      list.forEach((step, index) => {
        if (!rowOf.has(step)) rowOf.set(step, makeRow(step));
        const el = rowOf.get(step);
        update(el, step, index);
        if (rows.children[index] !== el) rows.insertBefore(el, rows.children[index] || null);
      });
      // Moving a row can drop the focus; put it back where it was
      if (active && active !== document.activeElement && rows.contains(active)) active.focus();
      add.hidden = list.length >= MAX;
      add.textContent = list.length ? "+ Add a step" : "+ Add a step (like Wrap at " + defaultTemperature([], target || (options.unit() === "fahrenheit" ? 203 : 95), options.unit()) + "°)";
    }

    add.addEventListener("click", () => {
      // Without a target yet (a new preset) start from the same 95 °C (203 °F) as the link's text
      const top = target || (options.unit() === "fahrenheit" ? 203 : 95);
      list.push({ action: "Wrap", temperature: defaultTemperature(list, top, options.unit()) });
      changed();
    });

    return {
      // The steps from the grill. While a row is being typed in only the target is taken: the edit
      // is sent when it's done, and the answer to that refills.
      set(steps, newTarget) {
        target = newTarget;
        const next = sorted(plain(steps || []));
        if (!typing() && JSON.stringify(next) !== JSON.stringify(plain(list))) list = next;
        render();
      },
      setTarget(newTarget) { target = newTarget; render(); },
      get: () => plain(list),
      element: container,
    };
  }

  return { MAX, ACTIONS, sorted, defaultTemperature, create };
})();

if (typeof module === "object" && module.exports) module.exports = StepRows;
