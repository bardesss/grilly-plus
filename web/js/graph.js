// Geometry for the temperature graphs: points in, SVG path data out. No DOM access, so the unit
// tests can run it in Node. Points are {t: milliseconds, v: degrees or null for a gap}, oldest first.
const Graph = (() => {
  const MIN_SPAN = 5;      // degrees, so a flat line doesn't turn noise into mountains
  const PADDING = 0.08;    // share of the range kept free above and below
  const FORECAST_MIN = 0.2;    // share of the width for the forecast, whatever the time to target
  const FORECAST_MAX = 0.45;   // so a long cook or a far target don't squeeze the readings

  const round = (value) => Math.round(value * 10) / 10;

  // Averages the points into at most `buckets` equal time slices. A slice with only gaps stays a gap.
  function downsample(points, buckets) {
    if (points.length <= buckets) return points;
    const start = points[0].t;
    const width = (points[points.length - 1].t - start) / buckets || 1;
    const result = [];
    let index = 0;
    for (let bucket = 0; bucket < buckets; bucket++) {
      const end = bucket === buckets - 1 ? Infinity : start + (bucket + 1) * width;
      let sum = 0, valid = 0, seen = 0, t = 0;
      while (index < points.length && points[index].t < end) {
        const point = points[index++];
        seen++;
        t = point.t;
        if (point.v !== null) { sum += point.v; valid++; }
      }
      if (seen > 0) result.push({ t, v: valid > 0 ? sum / valid : null });
    }
    return result;
  }

  // Share of the width for the forecast: the time to target in proportion to the readings shown,
  // within FORECAST_MIN..FORECAST_MAX. 0 without a forecast.
  function forecastShare(spanMs, etaSeconds) {
    if (!(etaSeconds > 0)) return 0;
    const share = etaSeconds * 1000 / (spanMs + etaSeconds * 1000);
    return Math.min(FORECAST_MAX, Math.max(FORECAST_MIN, share));
  }

  // Path data for a width x height box, or null with fewer than 2 readings. The target (and the
  // range minimum) are part of the scale, so their guide lines are always visible. In target mode
  // with opts.etaSeconds > 0 the readings take the left part and a dotted forecast runs from the
  // newest reading to the target at the right edge.
  function layout(points, opts) {
    const values = points.filter((point) => point.v !== null).map((point) => point.v);
    if (values.length < 2) return null;

    let low = values[0];
    let high = values[0];
    for (const value of values) {
      if (value < low) low = value;
      if (value > high) high = value;
    }
    const lowest = low;
    const highest = high;
    if (opts.mode !== "off") {
      high = Math.max(high, opts.target);
      low = Math.min(low, opts.target);
    }
    if (opts.mode === "range") low = Math.min(low, opts.minimum);
    if (high - low < MIN_SPAN) {
      const middle = (high + low) / 2;
      low = middle - MIN_SPAN / 2;
      high = middle + MIN_SPAN / 2;
    }
    const padding = (high - low) * PADDING;
    low -= padding;
    high += padding;

    const shown = downsample(points, opts.width);
    const first = shown[0].t;
    const span = shown[shown.length - 1].t - first || 1;
    const share = opts.mode === "target" ? forecastShare(points[points.length - 1].t - points[0].t, opts.etaSeconds) : 0;
    const nowX = round(opts.width * (1 - share));
    const x = (t) => round((t - first) / span * nowX);
    const y = (v) => round((high - v) / (high - low) * opts.height);

    // Split at gaps, each run of readings is its own line and area
    const runs = [];
    let run = [];
    for (const point of shown) {
      if (point.v === null) {
        if (run.length) runs.push(run);
        run = [];
      } else {
        run.push([x(point.t), y(point.v)]);
      }
    }
    if (run.length) runs.push(run);

    // A run of a single point has no line to draw ("Mx y" alone is invisible); repeat it with an L so
    // it shows up as a dot instead of silently vanishing (the round line caps and non-scaling stroke
    // turn that zero-length segment into a visible point).
    const line = runs.map((r) => {
      const path = r.length === 1 ? [r[0], r[0]] : r;
      return "M" + path.map(([px, py]) => px + " " + py).join("L");
    }).join("");
    const area = runs.filter((r) => r.length > 1).map((r) =>
      "M" + r[0][0] + " " + opts.height + "L" + r.map(([px, py]) => px + " " + py).join("L") +
      "L" + r[r.length - 1][0] + " " + opts.height + "Z").join("");

    // The newest reading, for the "now" dot and the start of the forecast
    const last = runs.length ? runs[runs.length - 1][runs[runs.length - 1].length - 1] : null;
    const lastIsNow = last !== null && shown[shown.length - 1].v !== null;
    const forecast = share > 0 && lastIsNow
      ? "M" + last[0] + " " + last[1] + "L" + opts.width + " " + y(opts.target) : null;

    return {
      line,
      area,
      forecast,
      nowX,
      now: lastIsNow ? { x: last[0], y: last[1] } : null,
      target: opts.mode === "off" ? null : y(opts.target),
      minimum: opts.mode === "range" ? y(opts.minimum) : null,
      highest,
      lowest,
      startT: points[0].t,
      endT: points[points.length - 1].t,
      scaleLow: low,
      scaleHigh: high,
    };
  }

  // Round values for axis labels: at most about `count` steps between low and high
  function ticks(low, high, count = 3) {
    const steps = [0.5, 1, 2, 5, 10, 20, 25, 50, 100, 200];
    const step = steps.find((s) => s >= (high - low) / count) || 200;
    const result = [];
    for (let v = Math.ceil(low / step) * step; v <= high + 1e-9; v += step) result.push(Math.round(v * 10) / 10);
    return result;
  }

  return { downsample, forecastShare, layout, ticks };
})();

if (typeof module === "object" && module.exports) module.exports = Graph;
