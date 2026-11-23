// Steps of a probe or a preset ("Wrap at 74°"): rows of an action and a temperature, an ✕ per row
// and "+ Add a step". Nothing but that link shows until a step is added.
const StepRows = (() => {
  const MAX = 3;
  const ACTIONS = ["Wrap", "Spritz", "Sauce", "Flip", "Check", "Baste"];

  function sorted(list) {
    return list.slice().sort((a, b) => a.temperature - b.temperature);
  }

  // A new step: 20 °C (35 °F) below the target, or 5 below the last step if that is lower
  function defaultTemperature(list, target, unit) {
    let t = target - (unit === "fahrenheit" ? 35 : 20);
    if (list.length) t = Math.min(t, list[list.length - 1].temperature - 5);
    return Math.max(1, Math.round(t));
  }

  // options: { unit: () => "celcius"|"fahrenheit", onChange(list) }
  function create(container, options) {
    let list = [];
    let target = 0;
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

    function changed() {
      list = sorted(list);
      render();
      options.onChange(list.map(({ action, temperature }) => ({ action, temperature })));
    }

    function row(step, index) {
      const el = document.createElement("div");
      el.className = "step-row";
      const action = document.createElement("input");
      action.className = "step-action";
      action.setAttribute("list", "step-actions");
      action.maxLength = 16;
      action.value = step.action;
      action.setAttribute("aria-label", "Step " + (index + 1) + " action");
      action.addEventListener("change", () => {
        const text = action.value.trim();
        if (text) { step.action = text; changed(); } else { action.value = step.action; }
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
      temperature.value = step.temperature;
      temperature.setAttribute("aria-label", "Step " + (index + 1) + " temperature");
      temperature.addEventListener("change", () => {
        const value = Math.round(Number(temperature.value));
        if (value >= 1 && value <= 999) { step.temperature = value; changed(); } else { temperature.value = step.temperature; }
      });
      box.append(temperature, Format.unitSymbol(options.unit()));
      const remove = document.createElement("button");
      remove.type = "button";
      remove.className = "icon-button";
      remove.setAttribute("aria-label", "Remove step " + (index + 1));
      remove.innerHTML = '<svg class="icon"><use href="#i-close"/></svg>';
      remove.addEventListener("click", () => { list.splice(index, 1); changed(); });
      el.append(action, at, box, remove);
      if (target > 0 && step.temperature >= target) {
        el.classList.add("ignored");
        const hint = document.createElement("p");
        hint.className = "hint step-hint";
        hint.textContent = "Above the target, so it's skipped";
        el.append(hint);
      }
      return el;
    }

    function render() {
      if (Controls.isEditing(rows)) return;     // keep the row the user is typing in
      rows.replaceChildren(...list.map(row));
      add.hidden = list.length >= MAX;
      add.textContent = list.length ? "+ Add a step" : "+ Add a step (like Wrap at " + defaultTemperature([], target || 95, options.unit()) + "°)";
    }

    add.addEventListener("click", () => {
      list.push({ action: "Wrap", temperature: defaultTemperature(list, target, options.unit()) });
      changed();
    });

    return {
      set(steps, newTarget) {
        target = newTarget;
        list = sorted((steps || []).map(({ action, temperature }) => ({ action, temperature })));
        render();
      },
      setTarget(newTarget) { target = newTarget; render(); },
      get: () => list.map(({ action, temperature }) => ({ action, temperature })),
      element: container,
    };
  }

  return { MAX, ACTIONS, sorted, defaultTemperature, create };
})();

if (typeof module === "object" && module.exports) module.exports = StepRows;
