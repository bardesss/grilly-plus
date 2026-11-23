// Grill view: one card per connected probe in socket order. Tapping a card opens the probe editor.
(() => {
  const GRAPH_WIDTH = 240;   // viewBox units, the svg stretches to the card width
  const GRAPH_HEIGHT = 46;

  let list;
  let emptyLine;
  let noProbes;
  const cards = {};   // probe_id -> card element

  function createCard(id) {
    const card = document.createElement("button");
    card.type = "button";
    card.className = "card probe-card live";
    card.dataset.probe = String(id);
    card.innerHTML =
      '<div class="card-head"><span class="probe-name"></span><span class="probe-alarm"></span></div>' +
      '<div class="probe-temp"><span class="value"></span><span class="unit"></span></div>' +
      '<div class="graph-wrap">' +
      '<svg class="graph" viewBox="0 0 ' + GRAPH_WIDTH + ' ' + GRAPH_HEIGHT + '" preserveAspectRatio="none" aria-hidden="true" hidden>' +
      '<rect class="graph-band" x="0" width="' + GRAPH_WIDTH + '"/><path class="graph-area"/><path class="graph-line"/>' +
      '<line class="graph-target" x1="0" x2="' + GRAPH_WIDTH + '"/><path class="graph-forecast"/></svg>' +
      // HTML on top of the stretched svg, so the dot stays round and the text unscaled
      '<span class="graph-dot" hidden></span><span class="graph-target-label" hidden></span>' +
      '</div>' +
      // Under the strip instead of on it, where they collided with the line
      '<div class="graph-labels" hidden><span class="graph-start"></span><span class="graph-range"></span>' +
      '<span class="graph-now">now</span><span class="graph-ready" hidden></span></div>' +
      '<div class="graph-empty">Collecting readings…</div>' +
      '<div class="card-meta"><span class="probe-status"></span><span class="probe-time"></span></div>' +
      '<p class="probe-step" hidden></p>';
    card.addEventListener("click", () => {
      if (typeof Editor !== "undefined") Editor.open(id, card);
    });
    return card;
  }

  // Draws the whole cook into the card's svg, or shows the placeholder until there are 2 readings.
  // Skipped when nothing that feeds the drawing changed since the last poll, so a card with a
  // steady reading isn't rebuilt every second.
  function drawGraph(card, probe, unit) {
    const mode = Format.alarmMode(probe);
    // The time to target counts down every second; a minute is plenty for where the forecast ends
    const eta = mode === "target" && !probe.stalled && probe.eta_seconds > 0 ? probe.eta_seconds : -1;
    const key = [Trend.version(probe.probe_id), unit, mode, probe.target_temperature, probe.minimum_temperature,
                 eta > 0 ? Math.round(eta / 60) : -1].join("|");
    if (card.dataset.graphKey === key) return;
    card.dataset.graphKey = key;
    const result = Graph.layout(Trend.points(probe.probe_id, unit), {
      width: GRAPH_WIDTH, height: GRAPH_HEIGHT, mode,
      target: probe.target_temperature, minimum: probe.minimum_temperature, etaSeconds: eta,
    });
    const svg = card.querySelector(".graph");
    const labels = card.querySelector(".graph-labels");
    // `hidden` is an HTMLElement IDL property, not defined on SVGElement, so Chromium has no
    // reflecting setter for it here — toggle the attribute directly instead of svg.hidden = ...
    svg.toggleAttribute("hidden", result === null);
    labels.hidden = result === null;
    card.querySelector(".graph-empty").hidden = result !== null;
    if (result === null) return;

    svg.querySelector(".graph-line").setAttribute("d", result.line);
    svg.querySelector(".graph-area").setAttribute("d", result.area);
    const target = svg.querySelector(".graph-target");
    target.style.display = mode === "target" ? "" : "none";
    if (mode === "target") { target.setAttribute("y1", result.target); target.setAttribute("y2", result.target); }
    const band = svg.querySelector(".graph-band");
    band.style.display = mode === "range" ? "" : "none";
    if (mode === "range") {
      band.setAttribute("y", result.target);
      band.setAttribute("height", Math.max(0, result.minimum - result.target));
    }

    const forecast = svg.querySelector(".graph-forecast");
    forecast.style.display = result.forecast ? "" : "none";
    if (result.forecast) forecast.setAttribute("d", result.forecast);

    const pct = (value, of) => (value / of * 100) + "%";
    const dot = card.querySelector(".graph-dot");
    dot.hidden = result.now === null;
    if (result.now) { dot.style.left = pct(result.now.x, GRAPH_WIDTH); dot.style.top = pct(result.now.y, GRAPH_HEIGHT); }
    const targetLabel = card.querySelector(".graph-target-label");
    targetLabel.hidden = mode !== "target";
    if (mode === "target") {
      targetLabel.textContent = Math.round(probe.target_temperature) + "°";
      targetLabel.style.top = pct(result.target, GRAPH_HEIGHT);
    }

    // Off mode has no guide lines, so it keeps the lowest and highest reading as a hint of the scale
    const high = Math.round(result.highest);
    const low = Math.round(result.lowest);
    card.querySelector(".graph-start").textContent = Format.ago((result.endT - result.startT) / 1000);
    card.querySelector(".graph-range").textContent = mode !== "off" ? "" : low === high ? high + "°" : low + "–" + high + "°";
    const nowLabel = card.querySelector(".graph-now");
    const ready = card.querySelector(".graph-ready");
    ready.hidden = !result.forecast;
    labels.classList.toggle("has-forecast", !!result.forecast);
    if (result.forecast) {
      ready.textContent = Format.readyAt(eta);   // the card says "ready in …" right below
      nowLabel.style.left = pct(result.nowX, GRAPH_WIDTH);
    } else {
      nowLabel.style.left = "";
    }
  }

  function fill(card, probe, unit) {
    const status = Format.probeStatus(probe);
    card.dataset.status = status.kind;
    card.classList.toggle("alarming", !!probe.alarm);
    const nameEl = card.querySelector(".probe-name");
    if (nameEl.dataset.key !== probe.probe_id + "|" + probe.name + "|" + (probe.type_icon || "")) {
      nameEl.dataset.key = probe.probe_id + "|" + probe.name + "|" + (probe.type_icon || "");
      nameEl.textContent = probe.probe_id + " · ";
      const icon = Food.icon(probe.type_icon);
      if (icon) nameEl.append(icon, " ");
      nameEl.append(probe.name);
    }
    card.querySelector(".probe-alarm").textContent = Format.alarmLabel(probe);
    card.querySelector(".value").textContent = Format.number(probe.temperature);
    card.querySelector(".unit").textContent = Format.unitSymbol(unit);
    const statusText = probe.alarm ? ("Alarm" + (status.text ? " · " + status.text : "")) : status.text;
    card.querySelector(".probe-status").textContent = statusText;
    const eta = Format.progressNote(probe);
    const time = card.querySelector(".probe-time");
    time.textContent = eta || Format.duration(probe.connected_seconds);
    time.classList.toggle("eta", eta !== "");
    const stepText = Format.stepNote(probe);
    const stepEl = card.querySelector(".probe-step");
    stepEl.textContent = stepText;
    stepEl.hidden = stepText === "";
    stepEl.classList.toggle("now", probe.step_alert >= 0);
    drawGraph(card, probe, unit);
    card.setAttribute("aria-label",
      probe.name + ", " + Format.temperature(probe.temperature, unit) + (statusText ? ", " + statusText : "") + (eta ? ", " + eta : "") + (stepText ? ", " + stepText : "") + ". Edit probe");
  }

  function render(status) {
    const connected = status.probes.filter((probe) => probe.connected);
    const empty = status.probes.filter((probe) => !probe.connected).map((probe) => probe.probe_id);

    Object.keys(cards).forEach((id) => {
      if (!connected.some((probe) => String(probe.probe_id) === id)) {
        cards[id].remove();
        delete cards[id];
      }
    });

    connected.forEach((probe, index) => {
      let card = cards[probe.probe_id];
      if (!card) card = cards[probe.probe_id] = createCard(probe.probe_id);
      // Only move a card when its position changed, moving a focused element would drop focus
      if (list.children[index] !== card) list.insertBefore(card, list.children[index] || null);
      fill(card, probe, status.temperature_unit);
    });

    noProbes.hidden = connected.length > 0;
    emptyLine.hidden = connected.length === 0 || empty.length === 0;
    emptyLine.textContent = Format.emptySockets(empty);
  }

  function mount(el) {
    el.innerHTML =
      '<div class="timer-row"></div>' +
      '<div class="probe-list"></div>' +
      '<p class="empty-sockets"></p>' +
      '<div class="card no-probes" hidden><p>Plug in a probe</p></div>';
    list = el.querySelector(".probe-list");
    emptyLine = el.querySelector(".empty-sockets");
    noProbes = el.querySelector(".no-probes");
    if (typeof Timers !== "undefined") Timers.mount(el.querySelector(".timer-row"));
    App.onStatus(render);
    Trend.onChange(() => { const status = App.getStatus(); if (status) render(status); });
  }

  App.register("grill", { mount });
})();
