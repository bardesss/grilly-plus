// Temperature history per probe for the graphs. The grill's history is loaded once, after that every
// status poll adds a point, so the graphs stay current without extra requests. Kept in °C like the
// grill's history, converted to the display unit when drawn.
const Trend = (() => {
  const APPEND_EVERY_MS = 10000;   // the grill's own sample interval
  const MAX_POINTS = 4320;         // 12h at 10s/point, older data thins out instead of growing forever
  const RELOAD_EVERY_MS = 5000;    // minimum time between reconnect-triggered reloads
  const RETRY_AFTER_MS = 5000;     // a failed load gets one retry, after this delay
  const series = {};               // probe_id -> [{t, c}], oldest first
  const versions = {};             // probe_id -> bumped whenever its series changes, for cheap redraw checks
  const connectedBefore = {};      // probe_id -> connected in the previous status
  const stepMs = {};               // probe_id -> the grill's coarse history interval, from the last load
  const listeners = [];
  let loading = false;
  let pending = false;             // a reload was requested while one was already in flight
  let started = false;
  let lastLoadAt = -Infinity;      // Date.now() of the last load(), for the reconnect debounce
  let reloadTimer = null;          // pending debounced reload, at most one at a time
  let retryScheduled = false;      // a failed load's single retry is already queued

  function bump(id) { versions[id] = (versions[id] || 0) + 1; }
  function version(id) { return versions[id] || 0; }

  const isNumber = (value) => typeof value === "number" && isFinite(value);

  // Value i of a tier was taken age + (n - 1 - i) * interval seconds before nowMs
  function tierPoints(tier, nowMs) {
    const count = tier.values.length;
    return tier.values.map((value, i) => ({
      t: nowMs - (tier.age + (count - 1 - i) * tier.interval) * 1000,
      c: value === null ? null : value / 10,
    }));
  }

  function fromResponse(response, nowMs) {
    const result = {};
    for (const probe of (response && response.probes) || []) {
      result[probe.probe_id] = tierPoints(probe.coarse, nowMs);
    }
    return result;
  }

  // Keeps the newest half untouched and drops every other point of the older half, in place, so a
  // long cook stays fully in view at a lower resolution for its older data instead of growing forever.
  // A point that borders a gap (null reading) survives even at an odd index, so gaps aren't smoothed
  // away by thinning.
  function thin(points, max) {
    if (points.length <= max) return;
    const half = Math.ceil(points.length / 2);
    const older = points.slice(0, half).filter((point, i, arr) => {
      if (i % 2 === 0) return true;
      const prev = arr[i - 1];
      const next = arr[i + 1];
      return point.c === null || (prev && prev.c === null) || (next && next.c === null);
    });
    points.splice(0, points.length, ...older, ...points.slice(half));
  }

  // stepMs is the caller's own sample spacing (APPEND_EVERY_MS for live polling, or a probe's coarser
  // history interval right after a load) - the gap threshold is relative to it so a history point
  // that is merely as old as one of its own coarse intervals isn't mistaken for an outage.
  function append(points, celsius, nowMs, stepMs = APPEND_EVERY_MS) {
    const last = points[points.length - 1];
    if (last && nowMs - last.t < APPEND_EVERY_MS) return false;
    // A probe that dropped off and came back leaves a hole wider than a normal sample gap; show it
    // as a gap in the graph instead of a line bridging the outage.
    if (last && last.c !== null && nowMs - last.t > stepMs + 2 * APPEND_EVERY_MS) {
      points.push({ t: last.t + stepMs, c: null });
    }
    points.push({ t: nowMs, c: celsius });
    thin(points, MAX_POINTS);
    return true;
  }

  function toCelsius(value, unit) {
    if (!isNumber(value)) return null;
    return unit === "fahrenheit" ? (value - 32) / 1.8 : value;
  }

  function display(points, unit) {
    return points.map((point) => ({
      t: point.t,
      v: point.c === null ? null : unit === "fahrenheit" ? point.c * 1.8 + 32 : point.c,
    }));
  }

  function points(probeId, unit) {
    return display(series[probeId] || [], unit);
  }

  // Local time as 2026-11-22 14:05:30, which spreadsheets read as a date and time
  function stamp(ms) {
    const d = new Date(ms);
    const two = (n) => String(n).padStart(2, "0");
    return d.getFullYear() + "-" + two(d.getMonth() + 1) + "-" + two(d.getDate()) + " " +
      two(d.getHours()) + ":" + two(d.getMinutes()) + ":" + two(d.getSeconds());
  }

  // A probe's points ({t, v} in the display unit) as CSV: a time column and a temperature column.
  // A gap (probe unplugged) is an empty temperature.
  function csv(points, unit, name) {
    const header = '"Time","' + String(name).replace(/"/g, '""') + " (" + (unit === "fahrenheit" ? "°F" : "°C") + ')"';
    const rows = points.map((p) => stamp(p.t) + "," + (p.v === null ? "" : p.v.toFixed(1)));
    return [header, ...rows].join("\r\n") + "\r\n";
  }

  // grilly-plus-pulled-pork-2026-11-22-1405.csv
  function csvName(name, ms) {
    const slug = String(name).toLowerCase().normalize("NFKD").replace(/[\u0300-\u036f]/g, "").replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "") || "probe";
    return "grilly-plus-" + slug + "-" + stamp(ms).slice(0, 16).replace(" ", "-").replace(":", "") + ".csv";
  }

  // Notifies listeners so the card resets immediately, without waiting for the next status poll or
  // history reload. editor.js's own onChange handler only redraws the chart, and onClear() only calls
  // Trend.load() afterwards (not clear() again), so this can't loop back into another clear.
  function clear(probeId) {
    if (series[probeId]) series[probeId].length = 0;
    bump(probeId);
    listeners.forEach((listener) => listener());
  }

  function onChange(listener) { listeners.push(listener); }

  // isRetry marks the single automatic retry of a failed load, so its own failure doesn't queue
  // another one - see the catch block below.
  async function load(isRetry) {
    if (loading) { pending = true; return; }
    loading = true;
    try {
      const response = await Api.get("/api/history", 8000);
      const fresh = fromResponse(response, Date.now());
      const touched = new Set([...Object.keys(series), ...Object.keys(fresh)]);
      Object.keys(series).forEach((id) => { delete series[id]; });
      Object.assign(series, fresh);
      for (const probe of (response && response.probes) || []) {
        stepMs[probe.probe_id] = probe.coarse.interval * 1000;
      }
      touched.forEach(bump);
      retryScheduled = false;
      listeners.forEach((listener) => listener());
    } catch (error) {
      // Keep what we have, the next reconnect or page show loads again; also queue one retry so a
      // single dropped request doesn't leave the graphs stale until the next status change.
      if (!isRetry && !retryScheduled) {
        retryScheduled = true;
        setTimeout(() => { retryScheduled = false; load(true); }, RETRY_AFTER_MS);
      }
    } finally {
      loading = false;
      // A probe connecting or the page becoming visible during this load must not be dropped
      if (pending) { pending = false; load(); }
    }
  }

  function loadNow() {
    lastLoadAt = Date.now();
    load();
  }

  // Reloads immediately if the last one was more than RELOAD_EVERY_MS ago, otherwise schedules
  // exactly one reload for when that window is up - so a flapping probe reconnecting repeatedly
  // doesn't spam /api/history, but the reload it asked for still happens.
  function scheduleReload() {
    const wait = RELOAD_EVERY_MS - (Date.now() - lastLoadAt);
    if (wait <= 0) { loadNow(); return; }
    if (reloadTimer) return;
    reloadTimer = setTimeout(() => { reloadTimer = null; loadNow(); }, wait);
  }

  function onStatus(status) {
    const first = !started;
    started = true;
    let reconnected = false;
    const now = Date.now();
    for (const probe of status.probes) {
      const id = probe.probe_id;
      if (probe.connected && !connectedBefore[id]) reconnected = true;
      connectedBefore[id] = probe.connected;
      if (!probe.connected) continue;
      if (!series[id]) series[id] = [];
      const step = stepMs[id] || APPEND_EVERY_MS;
      if (append(series[id], toCelsius(probe.temperature, status.temperature_unit), now, step)) bump(id);
    }
    // The very first status and a probe reconnecting both want a reload, but only the reconnect
    // case is debounced - the first load should never wait.
    if (first) loadNow();
    else if (reconnected) scheduleReload();
  }

  if (typeof App !== "undefined") {
    App.onStatus(onStatus);
    document.addEventListener("visibilitychange", () => { if (!document.hidden) loadNow(); });
  }

  return { csv, csvName, tierPoints, fromResponse, append, thin, toCelsius, display, points, clear, onChange, load, onStatus, version };
})();

if (typeof module === "object" && module.exports) module.exports = Trend;
