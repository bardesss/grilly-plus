// Settings > Food presets: your own presets (add, edit, delete) and which built-in ones show in the
// probe editor's Food list. Kept on the grill, so every phone and browser sees the same list.
(() => {
  let note, ownList, builtinList, form, addButton, formTitle, stepsGroup, stepRows;
  let inputs = {};
  let editing = -1;          // index of the preset in the form, -1 for a new one
  let deleteArmed = -1;
  let deleteTimer = null;

  function unit() {
    const status = typeof App !== "undefined" ? App.getStatus() : null;
    const settings = Settings.current();
    return (status && status.temperature_unit) || (settings && settings.temperature_unit) || "celcius";
  }

  function symbol() { return Format.unitSymbol(unit()); }

  // ", wrap at 74°" for the first step, "" without steps
  function stepText(preset) {
    const first = Presets.steps(preset, unit())[0];
    return first ? ", " + first.action.toLowerCase() + " at " + first.temperature + "°" : "";
  }

  async function save(change) {
    Controls.showNote(note, "saving");
    try {
      await Presets.save(change);
      Controls.showNote(note, "saved");
      return true;
    } catch (error) {
      Controls.showNote(note, "error", error);
      return false;
    }
  }

  function button(text, className, onClick) {
    const el = document.createElement("button");
    el.type = "button";
    el.className = className;
    el.textContent = text;
    el.addEventListener("click", onClick);
    return el;
  }

  function row(text, ...controls) {
    const li = document.createElement("li");
    li.className = "preset-row";
    const span = document.createElement("span");
    span.className = "preset-text";
    span.textContent = text;
    li.append(span, ...controls);
    return li;
  }

  function renderOwn() {
    const own = Presets.get().presets;
    ownList.replaceChildren(...own.map((preset, index) => {
      const remove = button(deleteArmed === index ? "Tap again" : "Delete", "link-button danger", () => onDelete(index));
      const rest = Presets.rest(preset);
      return row(Presets.optionText(preset, unit(), symbol()) + (rest ? ", rest " + rest + " min" : "") + stepText(preset) + (preset.group ? " (" + preset.group + ")" : ""),
                 button("Edit", "link-button", () => openForm(index)), remove);
    }));
    if (!own.length) {
      const empty = document.createElement("li");
      empty.className = "hint";
      empty.textContent = "None yet. Add one here, or use Save as preset in a probe's editor.";
      ownList.append(empty);
    }
    addButton.disabled = own.length >= Presets.MAX_OWN;
    addButton.textContent = own.length >= Presets.MAX_OWN ? "30 presets is the maximum" : "Add a preset";
  }

  function renderBuiltin() {
    const hidden = Presets.get().hidden;
    builtinList.replaceChildren(...Presets.LIST.map((preset) => {
      const show = document.createElement("input");
      show.type = "checkbox";
      show.className = "switch";
      show.setAttribute("role", "switch");
      show.setAttribute("aria-label", "Show " + preset.label);
      show.checked = !hidden.includes(preset.id);
      show.addEventListener("change", () => onShowChange(preset.id, show.checked));
      return row(Presets.optionText(preset, unit(), symbol()) + stepText(preset), button("Copy", "link-button", () => copy(preset)), show);
    }));
  }

  function render() {
    if (!ownList) return;
    renderOwn();
    renderBuiltin();
  }

  async function onShowChange(id, shown) {
    const hidden = Presets.get().hidden.filter((h) => h !== id);
    if (!shown) hidden.push(id);
    if (!(await save({ hidden }))) render();
  }

  async function onDelete(index) {
    clearTimeout(deleteTimer);
    if (deleteArmed !== index) {
      deleteArmed = index;
      renderOwn();
      deleteTimer = setTimeout(() => { deleteArmed = -1; renderOwn(); }, 4000);
      return;
    }
    deleteArmed = -1;
    const list = Presets.get().presets;
    list.splice(index, 1);
    if (editing === index) closeForm();
    await save({ presets: list });
    render();
  }

  function fillForm(preset) {
    const u = unit();
    inputs.name.value = preset ? preset.name : "";
    inputs.group.value = preset && preset.group ? preset.group : "";
    inputs.type.value = preset && preset.type ? preset.type : "";
    inputs.target.value = preset ? Presets.target(preset, u) : "";
    const min = preset ? Presets.minimum(preset, u) : 0;
    inputs.minimum.value = min > 0 ? min : "";
    inputs.rest.value = preset && Presets.rest(preset) ? Presets.rest(preset) : "";
    stepRows.set(preset ? Presets.steps(preset, u) : [], Number(inputs.target.value) || 0);
    stepsGroup.hidden = min > 0;
    form.querySelectorAll(".unit-symbol").forEach((el) => { el.textContent = Format.unitSymbol(u); });
  }

  // Ends typing in the form: a tap on a button doesn't always move the focus (iPhone), so the
  // field's change, which the step rows wait for, would never come
  function endTyping() {
    if (form.contains(document.activeElement)) document.activeElement.blur();
  }

  function openForm(index) {
    endTyping();
    editing = index;
    formTitle.textContent = index >= 0 ? "Edit preset" : "New preset";
    fillForm(index >= 0 ? Presets.get().presets[index] : null);
    form.hidden = false;
    addButton.hidden = true;
    inputs.name.focus();
  }

  // A built-in preset as the start of one of your own
  function copy(preset) {
    openForm(-1);
    fillForm({ ...preset, name: preset.label.slice(0, 24), group: "" });
    inputs.name.select();
  }

  function closeForm() {
    editing = -1;
    form.hidden = true;
    addButton.hidden = false;
  }

  async function onSubmit(event) {
    event.preventDefault();
    endTyping();
    const u = unit();
    const max = u === "fahrenheit" ? 572 : 300;
    const target = Number(inputs.target.value);
    const minimum = inputs.minimum.value === "" ? 0 : Number(inputs.minimum.value);
    const rest = inputs.rest.value === "" ? 0 : Number(inputs.rest.value);
    let problem = "";
    if (!inputs.name.value.trim()) problem = "Give the preset a name";
    else if (!(target >= 1 && target <= max)) problem = "The target should be 1 to " + max + Format.unitSymbol(u);
    else if (minimum && !(minimum >= 1 && minimum < target)) problem = "The minimum should be below the target";
    else if (!(Number.isInteger(rest) && rest >= 0 && rest <= 180)) problem = "The rest time should be 0 to 180 minutes";
    else if (!minimum && stepRows.get().some((step) => step.temperature >= target)) problem = "Steps should be below the target";
    if (problem) {
      Controls.showNote(note, "error", new Error(problem));
      return;
    }
    const preset = Presets.make({ name: inputs.name.value, group: inputs.group.value, type: inputs.type.value, target, minimum, rest, steps: stepRows.get(), unit: u });
    const list = Presets.get().presets;
    if (editing >= 0) list[editing] = preset; else list.push(preset);
    if (await save({ presets: list })) closeForm();
    render();
  }

  function build() {
    const card = document.createElement("section");
    card.className = "card settings-card";
    card.innerHTML =
      '<h2 class="card-title"><span>Food presets</span><span class="save-note" role="status"></span></h2>' +
      '<p class="hint">Your own presets show at the top of the Food list in the probe editor, on every phone and browser.</p>' +
      '<ul class="preset-list" data-own></ul>' +
      '<button type="button" class="button" data-add>Add a preset</button>' +
      '<form class="preset-form" hidden novalidate>' +
      '  <h3 class="subheading" data-form-title></h3>' +
      '  <label class="field"><span>Name</span><input data-input="name" maxlength="24" autocomplete="off"></label>' +
      '  <label class="field"><span>Group</span><input data-input="group" maxlength="20" autocomplete="off" placeholder="My presets"></label>' +
      '  <label class="field"><span>Icon</span><select data-input="type"></select></label>' +
      '  <label class="field"><span>Target or maximum (<span class="unit-symbol"></span>)</span><input data-input="target" type="number" inputmode="numeric"></label>' +
      '  <label class="field"><span>Minimum, for a range (<span class="unit-symbol"></span>, optional)</span><input data-input="minimum" type="number" inputmode="numeric"></label>' +
      '  <label class="field"><span>Rest timer in minutes (optional, not for a range)</span><input data-input="rest" type="number" inputmode="numeric" min="0" max="180"></label>' +
      '  <div class="field" data-steps-group><span class="field-label">Steps</span><div data-steps></div></div>' +
      '  <div class="form-buttons"><button type="submit" class="button primary">Save</button>' +
      '  <button type="button" class="button" data-cancel>Cancel</button></div>' +
      '</form>' +
      '<details class="advanced"><summary>Built-in presets</summary>' +
      '  <p class="hint">Switch off the ones you never use. Copy one to make it your own.</p>' +
      '  <ul class="preset-list" data-builtin></ul></details>';

    note = card.querySelector(".save-note");
    ownList = card.querySelector("[data-own]");
    builtinList = card.querySelector("[data-builtin]");
    form = card.querySelector(".preset-form");
    formTitle = card.querySelector("[data-form-title]");
    addButton = card.querySelector("[data-add]");
    inputs = {};
    card.querySelectorAll("[data-input]").forEach((input) => { inputs[input.dataset.input] = input; });
    inputs.type.add(new Option("None", ""));
    Food.TYPES.forEach((type) => inputs.type.add(new Option(type.emoji + " " + type.label, type.key)));
    stepsGroup = card.querySelector("[data-steps-group]");
    stepRows = StepRows.create(card.querySelector("[data-steps]"), {
      unit,
      onChange: () => {},     // read when the form is saved
      onError: (message) => Controls.showNote(note, "error", new Error(message)),
    });
    inputs.target.addEventListener("input", () => stepRows.setTarget(Number(inputs.target.value) || 0));
    inputs.minimum.addEventListener("input", () => { stepsGroup.hidden = inputs.minimum.value !== "" && Number(inputs.minimum.value) > 0; });

    addButton.addEventListener("click", () => openForm(-1));
    card.querySelector("[data-cancel]").addEventListener("click", closeForm);
    form.addEventListener("submit", onSubmit);
    Presets.onChange(render);
    Presets.load();
    render();
    return card;
  }

  // Settings refill on every visit: fetch the presets again too, another phone may have changed them
  function fill() {
    Presets.load();
  }

  Settings.addCard({ id: "presets", build, fill });
})();
