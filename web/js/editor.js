// Probe editor: a bottom sheet on phones, a side panel on wide screens. Every change saves itself.
const Editor = (() => {
  const TYPES = {
    grilleye_iris: "Grilleye Iris",
    ikea_fantast: "Ikea Fantast",
    maverick_et733: "Maverick ET733",
    weber_igrill: "Weber iGrill",
    custom: "Custom",
  };

  let backdrop, sheet, form, title, live, note, nameInput, modeInputs;
  let targetGroup, targetLabel, minGroup, targetStepper, minStepper, restGroup, restStepper, stepsGroup, stepRows;
  let presetGroup, presetSelect, presetKey = null, savePresetButton, savePresetTimer = null;
  let typeSelect, typeName, customBox, offsetStepper;
  let foodGroup, foodInputs;
  const refInputs = {};
  let saver;
  let probeId = null;
  let returnFocus = null;
  let closeTimer = null;
  let swipeStart = null;
  let chart, chartBox, rangeInputs, clearButton;
  let range = "cook";
  let fine = null;
  let chartKey = "";
  let clearTimer = null;
  let clearNote;
  let generation = 0;
  let fineLoading = false;

  function unit() {
    const status = App.getStatus();
    return status ? status.temperature_unit : "celcius";
  }

  function defaults() {
    return unit() === "fahrenheit"
      ? { max: 572, target: 160, rangeMin: 212, rangeMax: 230 }
      : { max: 300, target: 70, rangeMin: 100, rangeMax: 110 };
  }

  function save(fields, options) {
    saver.change({ probe_id: probeId, ...fields }, options);
  }

  function mode() {
    return [...modeInputs].find((input) => input.checked).value;
  }

  function showMode(current) {
    targetGroup.hidden = current === "off";
    presetGroup.hidden = current === "off";
    savePresetButton.hidden = current === "off";
    minGroup.hidden = current !== "range";
    restGroup.hidden = current !== "target";
    stepsGroup.hidden = current !== "target";
    targetLabel.textContent = current === "range" ? "Maximum temperature" : "Target temperature";
    targetStepper.setLabel(current === "range" ? "Maximum temperature" : "Target temperature");
  }

  function onModeChange() {
    const current = mode();
    showMode(current);
    if (current === "off") {
      save({ target_temperature: 0, minimum_temperature: 0 });
    } else if (current === "target") {
      save({ target_temperature: targetStepper.get(), minimum_temperature: 0 });
    } else {
      if (minStepper.get() >= targetStepper.get()) minStepper.set(targetStepper.get() - 5);
      save({ target_temperature: targetStepper.get(), minimum_temperature: minStepper.get() });
    }
  }

  function onTargetChange(value) {
    stepRows.setTarget(value);
    if (mode() === "range") {
      if (minStepper.get() >= value) minStepper.set(value - 1);
      save({ target_temperature: value, minimum_temperature: minStepper.get() });
    } else {
      save({ target_temperature: value });
    }
  }

  function onRestChange(value) {
    save({ rest_minutes: value });
  }

  function onMinChange(value) {
    if (value >= targetStepper.get()) targetStepper.set(value + 1);
    save({ target_temperature: targetStepper.get(), minimum_temperature: value });
  }

  // Options read "Brisket · 95°C", so they're rebuilt when the unit or the presets change
  function fillPresets(u) {
    const key = u + "|" + Presets.version();
    if (presetKey === key) return;
    presetKey = key;
    presetSelect.querySelectorAll("optgroup").forEach((group) => group.remove());   // length = 1 leaves them
    Presets.groups().forEach((group) => {
      const optgroup = document.createElement("optgroup");
      optgroup.label = group.name;
      group.items.forEach((preset) => {
        optgroup.append(new Option(Presets.optionText(preset, u, Format.unitSymbol(u)), preset.value));
      });
      presetSelect.append(optgroup);
    });
  }

  function onPresetChange() {
    const preset = Presets.byValue(presetSelect.value);
    presetSelect.value = "";
    if (!preset) return;
    const min = Presets.minimum(preset, unit());
    const current = min > 0 ? "range" : "target";
    modeInputs.forEach((input) => { input.checked = input.value === current; });
    showMode(current);
    targetStepper.set(Presets.target(preset, unit()));
    if (min > 0) minStepper.set(min);
    const fields = { target_temperature: targetStepper.get(), minimum_temperature: min > 0 ? minStepper.get() : 0, type_icon: preset.type || "" };
    if (!(min > 0)) {
      restStepper.set(Presets.rest(preset));
      fields.rest_minutes = restStepper.get();
    }
    setFood(fields.type_icon);
    if (Presets.shouldRename(nameInput.value, probeId)) {
      nameInput.value = preset.name;
      fields.name = preset.name;
    }
    save(fields, { immediate: true });
  }

  // Saves what the editor holds as a preset of your own; one with the same name is replaced
  async function onSavePreset() {
    clearTimeout(savePresetTimer);
    const name = nameInput.value.trim() || "Probe " + probeId;
    const food = [...foodInputs].find((i) => i.checked);
    const preset = Presets.make({
      name, group: "", type: food ? food.value : "", unit: unit(),
      target: targetStepper.get(), minimum: mode() === "range" ? minStepper.get() : 0,
      rest: mode() === "target" ? restStepper.get() : 0,
    });
    const list = Presets.get().presets;
    const index = list.findIndex((p) => p.name === preset.name);
    if (index >= 0) preset.group = list[index].group;
    if (index < 0 && list.length >= Presets.MAX_OWN) {
      savePresetButton.textContent = "You have 30 presets already, remove one under Settings";
    } else {
      if (index >= 0) list[index] = preset; else list.push(preset);
      savePresetButton.disabled = true;
      try {
        await Presets.save({ presets: list });
        savePresetButton.textContent = (index >= 0 ? "Updated" : "Saved as") + " preset “" + preset.name + "”";
      } catch (error) {
        savePresetButton.textContent = error.message || "Couldn't save the preset";
      } finally {
        savePresetButton.disabled = false;
      }
    }
    savePresetTimer = setTimeout(() => { savePresetButton.textContent = "Save as preset"; }, 3000);
  }

  function setFood(key) {
    foodInputs.forEach((input) => { input.checked = input.value === key; });
  }

  function onFoodChange() {
    const input = [...foodInputs].find((i) => i.checked);
    if (input) save({ type_icon: input.value }, { immediate: true });
  }

  // A row of icon buttons: none plus every food type
  function buildFoodPicker() {
    const options = foodGroup.querySelector(".options");
    [{ key: "", label: "None" }, ...Food.TYPES].forEach((type) => {
      const label = document.createElement("label");
      label.title = type.label;
      const input = document.createElement("input");
      input.type = "radio";
      input.name = "editor-food";
      input.value = type.key;
      const span = document.createElement("span");
      const icon = type.key ? Food.icon(type.key) : null;
      if (icon) span.append(icon);
      else span.textContent = "–";
      const text = document.createElement("span");
      text.className = "visually-hidden";
      text.textContent = type.label;
      label.append(input, span, text);
      options.append(label);
    });
    foodInputs = options.querySelectorAll("input");
    foodInputs.forEach((input) => input.addEventListener("change", onFoodChange));
  }

  function references() {
    return {
      reference_kohm: Number(refInputs.reference_kohm.value) || 0,
      reference_celcius: Number(refInputs.reference_celcius.value) || 0,
      reference_beta: Number(refInputs.reference_beta.value) || 0,
    };
  }

  function onOffsetChange(value) {
    save({ offset_celcius: value });
  }

  function onTypeChange() {
    const type = typeSelect.value;
    customBox.hidden = type !== "custom";
    typeName.textContent = TYPES[type];
    if (type === "custom") save({ probe_type: "custom", ...references() }, { immediate: true });
    else save({ probe_type: type }, { immediate: true });
  }

  // Fills the controls from a probe, but leaves the ones the user is working in alone
  function fill(probe) {
    if (!probe) return;
    const d = defaults();
    targetStepper.setLimits(1, d.max);
    minStepper.setLimits(1, d.max - 1);
    targetStepper.setSuffix(Format.unitSymbol(unit()));
    fillPresets(unit());
    minStepper.setSuffix(Format.unitSymbol(unit()));

    if (document.activeElement !== nameInput) nameInput.value = probe.name;
    if (!Controls.isEditing(foodGroup)) setFood(probe.type_icon || "");
    const current = Format.alarmMode(probe);
    if (!Controls.isEditing(form.querySelector(".segmented"))) {
      modeInputs.forEach((input) => { input.checked = input.value === current; });
    }
    if (!Controls.isEditing(targetStepper.input.parentElement.parentElement)) {
      targetStepper.set(probe.target_temperature > 0 ? probe.target_temperature : (current === "range" ? d.rangeMax : d.target));
    }
    if (!Controls.isEditing(minStepper.input.parentElement.parentElement)) {
      minStepper.set(probe.minimum_temperature > 0 ? probe.minimum_temperature : Math.min(d.rangeMin, targetStepper.get() - 1));
    }
    if (!Controls.isEditing(restStepper.input.parentElement.parentElement)) restStepper.set(probe.rest_minutes || 0);
    // Only /api/probes has the full steps; /api/grill has just the next one
    if (Array.isArray(probe.steps)) stepRows.set(probe.steps, targetStepper.get());
    else stepRows.setTarget(targetStepper.get());
    showMode(mode());   // follows the radios, which keep the user's choice while they're editing it

    if (!Controls.isEditing(typeSelect)) typeSelect.value = TYPES[probe.probe_type] ? probe.probe_type : "custom";
    typeName.textContent = TYPES[typeSelect.value];
    customBox.hidden = typeSelect.value !== "custom";
    Object.keys(refInputs).forEach((key) => {
      if (document.activeElement !== refInputs[key]) refInputs[key].value = probe[key];
    });
    if (!Controls.isEditing(offsetStepper.input.parentElement.parentElement)) {
      offsetStepper.set(probe.offset_celcius || 0);
    }
    form.disabled = false;
  }

  function updateLive(status) {
    if (probeId === null || !status) return;
    const probe = status.probes.find((p) => p.probe_id === probeId);
    if (probe && probe.connected) {
      const eta = Format.progressNote(probe);
      live.textContent = "Now " + Format.temperature(probe.temperature, status.temperature_unit) + (eta ? " · " + eta : "");
    } else {
      live.textContent = "Not connected";
    }
  }

  // Last 30 minutes: the grill's 10 s history plus the points the page added since
  function recentPoints(id, u) {
    const base = fine ? Trend.display(fine, u) : [];
    const lastT = base.length ? base[base.length - 1].t : 0;
    const cutoff = Date.now() - 30 * 60 * 1000;
    return base.concat(Trend.points(id, u).filter((p) => p.t > lastT)).filter((p) => p.t >= cutoff);
  }

  function drawChart(status) {
    if (probeId === null || !status) return;
    const probe = status.probes.find((p) => p.probe_id === probeId);
    chartBox.hidden = !probe || !probe.connected;
    if (chartBox.hidden) return;
    const u = status.temperature_unit;
    const mode = Format.alarmMode(probe);
    const key = [range, Trend.version(probeId), fine ? fine.length : -1, u, mode,
                 probe.target_temperature, probe.minimum_temperature, probe.alarm,
                 Format.probeStatus(probe).kind].join("|");
    if (key === chartKey) return;
    chartKey = key;
    chart.update(range === "cook" ? Trend.points(probeId, u) : recentPoints(probeId, u), {
      mode, target: probe.target_temperature, minimum: probe.minimum_temperature,
      status: probe.alarm ? "alarm" : Format.probeStatus(probe).kind,
    });
  }

  // gen guards against a fetch that is still in flight when a clear (or a fresh open()) invalidates
  // it - without this, a late response could put the pre-clear fine data back on screen.
  async function loadFine(id) {
    const gen = generation;
    fineLoading = true;
    try {
      const response = await Api.get("/api/history?probe=" + id, 5000);
      if (probeId !== id || gen !== generation) return;
      const entry = response && response.probes && response.probes[0];
      fine = entry && entry.fine ? Trend.tierPoints(entry.fine, Date.now()) : [];
    } catch (error) {
      if (probeId === id && gen === generation) fine = [];   // the whole-cook points still fill the last 30 minutes
    } finally {
      fineLoading = false;
    }
    if (probeId !== id || gen !== generation) return;
    chartKey = "";
    drawChart(App.getStatus());
  }

  function onRangeChange() {
    range = [...rangeInputs].find((input) => input.checked).value;
    chartKey = "";
    if (range === "recent" && fine === null && !fineLoading) loadFine(probeId);
    drawChart(App.getStatus());
  }

  // The whole cook as the graph shows it, in the display unit
  function downloadCsv() {
    const status = App.getStatus();
    if (probeId === null || !status) return;
    const name = nameInput.value.trim() || "Probe " + probeId;
    const blob = new Blob(["\ufeff" + Trend.csv(Trend.points(probeId, status.temperature_unit), status.temperature_unit, name)],
                          { type: "text/csv;charset=utf-8" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = Trend.csvName(name, Date.now());
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
  }

  function resetClear() {
    clearTimeout(clearTimer);
    clearButton.textContent = "Clear history";
    clearButton.disabled = false;
    delete clearButton.dataset.confirm;
  }

  async function onClear() {
    if (!clearButton.dataset.confirm) {
      clearTimeout(clearTimer);   // a leftover "History cleared" reset must not cut the new confirm short
      clearButton.dataset.confirm = "1";
      clearButton.textContent = "Tap again to clear the history";
      clearTimer = setTimeout(resetClear, 4000);
      return;
    }
    clearTimeout(clearTimer);
    const id = probeId;
    clearButton.disabled = true;
    if (clearNote) clearNote.textContent = "";
    try {
      await Api.post("/api/history/clear", { probe_id: id });
      Trend.clear(id);
      generation++;   // invalidate any loadFine() still in flight for the pre-clear data
      if (probeId === id) {
        fine = range === "recent" ? [] : null;
        chartKey = "";
        drawChart(App.getStatus());
        clearButton.textContent = "History cleared";
        if (clearNote) clearNote.textContent = "History cleared";
      }
      Trend.load();
    } catch (error) {
      if (probeId === id) {
        clearButton.textContent = error.message || "Couldn't clear the history";
        if (clearNote) clearNote.textContent = clearButton.textContent;
      }
    } finally {
      if (probeId === id) {
        clearButton.disabled = false;
        delete clearButton.dataset.confirm;
        clearTimer = setTimeout(resetClear, 2500);
      }
    }
  }

  async function open(id, trigger) {
    if (probeId !== null && probeId !== id) saver.flush();
    probeId = id;
    returnFocus = trigger || null;
    Presets.load();     // presets of your own may have changed on another phone
    title.textContent = "Probe " + id;
    Controls.showNote(note, "idle");
    form.disabled = true;
    updateLive(App.getStatus());
    range = "cook";
    fine = null;
    chartKey = "";
    generation++;   // invalidate a loadFine() left over from a previous probe or clear
    rangeInputs.forEach((input) => { input.checked = input.value === "cook"; });
    resetClear();
    drawChart(App.getStatus());
    show();
    try {
      const probes = await Api.get("/api/probes", 5000);
      if (probeId === id) fill(probes.find((p) => p.probe_id === id));
    } catch (error) {
      Controls.showNote(note, "error", error);
    }
  }

  function show() {
    clearTimeout(closeTimer);
    backdrop.hidden = false;
    sheet.hidden = false;
    sheet.getBoundingClientRect();   // apply the hidden state first so the slide-in animates
    document.body.classList.add("sheet-open");
    sheet.querySelector("[data-close]").focus();
  }

  function close() {
    if (probeId === null) return;
    saver.flush();
    probeId = null;
    resetClear();
    document.body.classList.remove("sheet-open");
    closeTimer = setTimeout(() => { backdrop.hidden = true; sheet.hidden = true; }, 260);
    if (returnFocus && document.contains(returnFocus)) returnFocus.focus();
    else document.getElementById("view-grill").focus();   // the card was redrawn, e.g. the probe was unplugged
  }

  // Keeps Tab inside the sheet while it's open
  function trapFocus(event) {
    if (event.key === "Escape") { close(); return; }
    if (event.key !== "Tab") return;
    const focusable = [...sheet.querySelectorAll("button, input, select, summary")]
      .filter((el) => !el.disabled && el.offsetParent !== null);
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (event.shiftKey && document.activeElement === first) { last.focus(); event.preventDefault(); }
    else if (!event.shiftKey && document.activeElement === last) { first.focus(); event.preventDefault(); }
  }

  function mount() {
    backdrop = document.createElement("div");
    backdrop.className = "sheet-backdrop";
    backdrop.hidden = true;

    sheet = document.createElement("aside");
    sheet.className = "sheet";
    sheet.hidden = true;
    sheet.setAttribute("role", "dialog");
    sheet.setAttribute("aria-modal", "true");
    sheet.setAttribute("aria-labelledby", "editor-title");
    sheet.innerHTML =
      '<div class="sheet-grab" aria-hidden="true"></div>' +
      '<div class="sheet-head">' +
      '  <h2 id="editor-title"></h2><span class="save-note" role="status"></span>' +
      '  <button type="button" class="icon-button" data-close aria-label="Close"><svg class="icon"><use href="#i-close"/></svg></button>' +
      '</div>' +
      '<p class="sheet-live live"></p>' +
      '<div id="editor-chart-box">' +
      '  <div id="editor-chart"></div>' +
      '  <fieldset class="segmented chart-range"><legend class="visually-hidden">Graph</legend><div class="options">' +
      '    <label><input type="radio" name="editor-range" value="recent"><span>30 min</span></label>' +
      '    <label><input type="radio" name="editor-range" value="cook"><span>Whole cook</span></label>' +
      '  </div></fieldset>' +
      '</div>' +
      '<fieldset class="editor-form">' +
      '  <label class="field"><span>Name</span><input id="editor-name" maxlength="32" autocomplete="off"></label>' +
      '  <fieldset class="food-picker"><legend>Icon</legend><div class="options"></div></fieldset>' +
      '  <fieldset class="segmented"><legend>Alarm</legend><div class="options">' +
      '    <label><input type="radio" name="editor-mode" value="off"><span>Off</span></label>' +
      '    <label><input type="radio" name="editor-mode" value="target"><span>Target</span></label>' +
      '    <label><input type="radio" name="editor-mode" value="range"><span>Range</span></label>' +
      '  </div></fieldset>' +
      '  <label class="field" id="editor-preset-group"><span>Food</span><select id="editor-preset">' +
      '    <option value="">Pick a food to set the target</option></select></label>' +
      '  <button type="button" class="link-button" id="editor-save-preset">Save as preset</button>' +
      '  <div class="field" id="editor-min-group"><span class="field-label">Minimum temperature</span><div id="editor-min"></div></div>' +
      '  <div class="field" id="editor-target-group"><span class="field-label" id="editor-target-label"></span><div id="editor-target"></div></div>' +
      '  <div class="field" id="editor-steps-group"><span class="field-label">Steps</span><div id="editor-steps"></div></div>' +
      '  <div class="field" id="editor-rest-group"><span class="field-label">Rest timer</span><div id="editor-rest"></div>' +
      '    <p class="hint">Starts when the target is reached, so you know when to slice. 0 is off.</p></div>' +
      '  <details class="probe-type"><summary><span>Probe type</span><span class="summary-value"><span id="editor-type-name"></span> <svg class="icon"><use href="#i-chevron"/></svg></span></summary>' +
      '    <label class="field"><span>Type</span><select id="editor-type"></select></label>' +
      '    <div id="editor-custom">' +
      '      <label class="field"><span>Reference resistance (kOhm)</span><input type="number" inputmode="numeric" data-ref="reference_kohm"></label>' +
      '      <label class="field"><span>Reference temperature (°C)</span><input type="number" inputmode="numeric" data-ref="reference_celcius"></label>' +
      '      <label class="field"><span>Beta</span><input type="number" inputmode="numeric" data-ref="reference_beta"></label>' +
      '    </div>' +
      '    <div class="field"><span class="field-label">Calibration offset (°C)</span><div id="editor-offset"></div>' +
      '      <p class="hint">Saved for this socket, not for the probe. If you move the probe to another socket, set it again there.</p></div>' +
      '  </details>' +
      '</fieldset>' +
      '<div class="editor-actions">' +
      '  <button type="button" class="link-button" id="editor-csv">Download CSV</button>' +
      '  <button type="button" class="link-button danger" id="editor-clear">Clear history</button>' +
      '</div>' +
      '<span class="visually-hidden" id="editor-clear-note" aria-live="polite"></span>';
    document.body.append(backdrop, sheet);

    form = sheet.querySelector(".editor-form");
    title = sheet.querySelector("#editor-title");
    live = sheet.querySelector(".sheet-live");
    note = sheet.querySelector(".save-note");
    nameInput = sheet.querySelector("#editor-name");
    modeInputs = sheet.querySelectorAll('input[name="editor-mode"]');
    targetGroup = sheet.querySelector("#editor-target-group");
    targetLabel = sheet.querySelector("#editor-target-label");
    minGroup = sheet.querySelector("#editor-min-group");
    restGroup = sheet.querySelector("#editor-rest-group");
    stepsGroup = sheet.querySelector("#editor-steps-group");
    presetGroup = sheet.querySelector("#editor-preset-group");
    presetSelect = sheet.querySelector("#editor-preset");
    savePresetButton = sheet.querySelector("#editor-save-preset");
    foodGroup = sheet.querySelector(".food-picker");
    buildFoodPicker();
    typeSelect = sheet.querySelector("#editor-type");
    typeName = sheet.querySelector("#editor-type-name");
    customBox = sheet.querySelector("#editor-custom");
    sheet.querySelectorAll("[data-ref]").forEach((input) => { refInputs[input.dataset.ref] = input; });
    chart = Chart.create(sheet.querySelector("#editor-chart"));
    chartBox = sheet.querySelector("#editor-chart-box");
    rangeInputs = sheet.querySelectorAll('input[name="editor-range"]');
    clearButton = sheet.querySelector("#editor-clear");
    clearNote = sheet.querySelector("#editor-clear-note");

    Object.entries(TYPES).forEach(([value, label]) => typeSelect.add(new Option(label, value)));

    targetStepper = Controls.stepper(sheet.querySelector("#editor-target"), {
      min: 1, max: 300, label: "Target temperature", onChange: onTargetChange,
    });
    minStepper = Controls.stepper(sheet.querySelector("#editor-min"), {
      min: 1, max: 299, label: "Minimum temperature", onChange: onMinChange,
    });
    stepRows = StepRows.create(sheet.querySelector("#editor-steps"), {
      unit,
      onChange: (list) => save({ steps: list }, { immediate: true }),
    });
    restStepper = Controls.stepper(sheet.querySelector("#editor-rest"), {
      min: 0, max: 180, label: "Rest timer in minutes", suffix: (v) => (v ? " min" : " (off)"), onChange: onRestChange,
    });
    offsetStepper = Controls.stepper(sheet.querySelector("#editor-offset"), {
      min: -10, max: 10, step: 0.1, label: "Calibration offset", suffix: "°C", onChange: onOffsetChange,
    });

    saver = createSaver({
      send: (fields) => Api.post("/api/probes", [fields]),
      onStatus: (state, detail) => {
        Controls.showNote(note, state, detail);
        // While newer changes wait or are on their way, this answer is outdated: the next one refills
        if (state === "saved" && Array.isArray(detail) && probeId !== null && !saver.hasPending()) {
          fill(detail.find((p) => p.probe_id === probeId));
        }
      },
    });

    nameInput.addEventListener("change", () => save({ name: nameInput.value.trim() }, { immediate: true }));
    nameInput.addEventListener("keydown", (event) => { if (event.key === "Enter") nameInput.blur(); });
    modeInputs.forEach((input) => input.addEventListener("change", onModeChange));
    typeSelect.addEventListener("change", onTypeChange);
    presetSelect.addEventListener("change", onPresetChange);
    savePresetButton.addEventListener("click", onSavePreset);
    Presets.onChange(() => fillPresets(unit()));
    Object.entries(refInputs).forEach(([key, input]) => {
      input.addEventListener("change", () => save({ probe_type: "custom", ...references() }, { immediate: true }));
    });

    sheet.querySelector("[data-close]").addEventListener("click", close);
    backdrop.addEventListener("click", close);
    sheet.addEventListener("keydown", trapFocus);
    rangeInputs.forEach((input) => input.addEventListener("change", onRangeChange));
    clearButton.addEventListener("click", onClear);
    sheet.querySelector("#editor-csv").addEventListener("click", downloadCsv);

    // Swipe down on the grab handle or the title row to close
    sheet.addEventListener("pointerdown", (event) => {
      if (event.target.closest(".sheet-grab, .sheet-head")) swipeStart = event.clientY;
    });
    sheet.addEventListener("pointerup", (event) => {
      if (swipeStart !== null && event.clientY - swipeStart > 80) close();
      swipeStart = null;
    });

    App.onStatus(updateLive);
    App.onStatus(drawChart);
    Trend.onChange(() => { chartKey = ""; drawChart(App.getStatus()); });
  }

  // The editor has no view of its own, it mounts once when the app starts
  App.register("editor", { mount });

  return { open, close };
})();
