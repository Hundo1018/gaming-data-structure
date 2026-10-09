#!/usr/bin/env python3
"""Preregistered predictions, judged by code.

Every candidate's `hypothesis.md` states falsifiable predictions in prose, and
until now a person read the report and wrote in `notes.md` whether each one
held. That leaves the verdict to whoever writes the notes, and a reader cannot
tell a prediction that held by a wide margin from one that held by less than
the noise.

A manifest may now carry the same predictions as data, under `predictions:`.
This script evaluates each one against `benchmarks/results.json` (the main
suite) and `benchmarks/scaling.json` (the sweeps), and writes
`benchmarks/predictions.json` and `benchmarks/predictions.md`. Nothing in it
decides what a prediction should have been; it only says whether the numbers
satisfy the one that was written down.

A prediction:

    - id: P1
      claim: hs03_knn_heavy median tick at most 0.5x uniform_grid's
      metric: step_ns_p50            # step_ns_p50 | step_ns_p99 | peak_bytes
                                     # | rewind_step_ns_p50 (median of the ticks
                                     # that open with a rewind) | time_exponent
                                     # | memory_exponent
      workloads: [hs03_knn_heavy]    # main-suite workloads, or "*" for the track
      against: uniform_grid          # ratio of this candidate to another
      at_most: 0.5                   # at_most | at_least | below | above | within

Instead of `workloads`, `sweep: <family>` reads the scaling results, with
optional `regimes: [...]` (default: every regime measured), `size: <population>`
for a step or memory metric at one point of a population family, or
`values: [...]` (or "*") for a family that varies a workload key.

The comparison is one of:
    against: name or [names]      value / theirs, holding against each; for an
                                  exponent, value - theirs
    against_min: [names]          value / min(theirs)
    against_workload: name        value / this candidate's own value there
    gap_closed: {from: a, to: b}  (a - value) / (a - b), the share of the
                                  distance from a to b this candidate covered
    (none)                        the value itself, e.g. an exponent

A prediction naming several workloads, regimes or values must hold at every one.
Its verdict is HELD, FALSIFIED, or UNTESTED when a point it needs was not
measured. A point is marked `within noise` when it is closer to its bound than
the spread between repetitions of the measurements it was computed from, and
an exponent is marked when its fit's r2 is below 0.9. Neither mark changes the
verdict: it says how much weight the verdict can carry.
"""

import argparse
import json
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

METRICS = {"step_ns_p50", "step_ns_p99", "peak_bytes", "rewind_step_ns_p50",
           "time_exponent", "memory_exponent"}
BOUNDS = ("at_most", "at_least", "below", "above", "within")
COMPARISONS = ("against", "against_min", "against_workload", "gap_closed")
KEYS = {"id", "claim", "metric", "workloads", "sweep", "regimes", "size", "values",
        *BOUNDS, *COMPARISONS}


def validate_predictions(preds, where):
    """Problems with a manifest's `predictions:` list; empty means well formed."""
    problems = []
    if not isinstance(preds, list):
        return [f"{where}: predictions must be a list"]
    seen = set()
    for i, p in enumerate(preds):
        tag = f"{where}: predictions[{i}]"
        if not isinstance(p, dict):
            problems.append(f"{tag}: must be a mapping")
            continue
        for k in p:
            if k not in KEYS:
                problems.append(f"{tag}: unknown key '{k}'")
        for k in ("id", "claim", "metric"):
            if k not in p:
                problems.append(f"{tag}: missing '{k}'")
        if p.get("id") in seen:
            problems.append(f"{tag}: duplicate id {p.get('id')}")
        seen.add(p.get("id"))
        if p.get("metric") not in METRICS:
            problems.append(f"{tag}: metric must be one of {sorted(METRICS)}")
        if ("workloads" in p) == ("sweep" in p):
            problems.append(f"{tag}: give exactly one of 'workloads' or 'sweep'")
        if not any(b in p for b in BOUNDS):
            problems.append(f"{tag}: give a bound, one of {BOUNDS}")
        if sum(c in p for c in COMPARISONS) > 1:
            problems.append(f"{tag}: give at most one of {COMPARISONS}")
        if p.get("metric") in ("time_exponent", "memory_exponent") and "sweep" not in p:
            problems.append(f"{tag}: an exponent comes from a sweep")
    return problems


def spread(xs):
    """Relative spread of repeated measurements: (max - min) / median."""
    xs = [x for x in (xs or []) if x is not None]
    if len(xs) < 2:
        return 0.0
    med = statistics.median(xs)
    return (max(xs) - min(xs)) / med if med else 0.0


class Data:
    """Measurements addressed by (candidate, point), whatever file they came from."""

    def __init__(self, results, scaling):
        self.results = results or {}
        self.scaling = scaling or {}

    def track_workloads(self, candidate):
        track = self.results.get("candidates", {}).get(candidate, {}).get("track")
        per_w = self.results.get("measurements", {}).get(candidate, {})
        return [w for w in per_w if per_w[w].get("status") == "ok"] if track else []

    def suite(self, candidate, workload, metric):
        """(value, spread) from the main suite, or None if not measured."""
        m = self.results.get("measurements", {}).get(candidate, {}).get(workload)
        if not m or m.get("status") != "ok" or metric not in m:
            return None
        if not m.get("checksum_matches_anchor", True):
            return None
        reps = {"step_ns_p50": m.get("repetition_step_ns_p50"),
                "step_ns_p99": m.get("repetition_step_ns_p99")}.get(metric)
        return float(m[metric]), spread(reps)

    def sweep_regimes(self, family):
        return sorted(k.split("::", 1)[1] for k in self.scaling.get("sweeps", {})
                      if k.split("::", 1)[0] == family)

    def sweep_row(self, family, regime, candidate):
        return self.scaling.get("sweeps", {}).get(f"{family}::{regime}", {}).get(candidate)

    def sweep(self, candidate, family, regime, metric, size=None, value=None):
        row = self.sweep_row(family, regime, candidate)
        if not row:
            return None
        if metric in ("time_exponent", "memory_exponent"):
            fit = row.get("time_fit" if metric == "time_exponent" else "memory_fit")
            if not fit:
                return None
            return float(fit["exponent"]), fit.get("r_squared")
        key = metric
        axis = "size" if size is not None else "value"
        want = size if size is not None else value
        xs = row.get(axis) or []
        if want not in xs or key not in row:
            return None
        return float(row[key][xs.index(want)]), None


def check_bound(x, p):
    """Whether x satisfies the prediction's bound, and the bound's reference value."""
    if "within" in p:
        return abs(x - 1.0) <= float(p["within"]), 1.0
    for b in ("at_most", "at_least", "below", "above"):
        if b in p:
            v = float(p[b])
            ok = {"at_most": x <= v, "at_least": x >= v, "below": x < v, "above": x > v}[b]
            return ok, v
    return False, None


def bound_text(p):
    for b in BOUNDS:
        if b in p:
            return f"{b.replace('_', ' ')} {p[b]}"
    return "?"


def points(p, data, candidate):
    """The points a prediction is evaluated at, as (label, fetch) pairs."""
    metric = p["metric"]
    out = []
    if "workloads" in p:
        ws = p["workloads"]
        names = data.track_workloads(candidate) if ws == "*" else list(ws)
        for w in names:
            out.append((w, lambda c, w=w: data.suite(c, w, metric), "suite", w))
        return out
    family = p["sweep"]
    regimes = p.get("regimes") or data.sweep_regimes(family)
    for r in regimes:
        if "values" in p:
            vals = p["values"]
            if vals == "*":
                row = data.sweep_row(family, r, candidate) or {}
                vals = row.get("value") or []
            for v in vals:
                out.append((f"{family}·{r}·{v}",
                            lambda c, r=r, v=v: data.sweep(c, family, r, metric, value=v),
                            "sweep", (r, v)))
        else:
            size = p.get("size")
            label = f"{family}·{r}" + (f"·n={size}" if size is not None else "")
            out.append((label, lambda c, r=r: data.sweep(c, family, r, metric, size=size),
                        "sweep", r))
    return out


def evaluate(candidate, p, data):
    instances = []
    for label, fetch, kind, where in points(p, data, candidate):
        mine = fetch(candidate)
        if mine is None:
            instances.append({"point": label, "status": "untested",
                              "reason": f"{candidate} not measured here"})
            continue
        value, mine_aux = mine
        noise = mine_aux if isinstance(mine_aux, float) and p["metric"] not in (
            "time_exponent", "memory_exponent") else 0.0
        poor_fit = (p["metric"] in ("time_exponent", "memory_exponent")
                    and mine_aux is not None and mine_aux < 0.9)

        ratios = []  # (x, description, extra noise)
        missing = None
        if "against" in p:
            others = p["against"] if isinstance(p["against"], list) else [p["against"]]
            for o in others:
                t = fetch(o)
                if t is None or not t[0]:
                    missing = o
                    break
                if p["metric"] in ("time_exponent", "memory_exponent"):
                    # An exponent near zero makes a ratio meaningless, so two
                    # exponents are compared by their difference.
                    ratios.append((value - t[0], f"- {o}", 0.0))
                else:
                    ratios.append((value / t[0], f"/ {o}",
                                   t[1] if isinstance(t[1], float) else 0.0))
        elif "against_min" in p:
            ts = [(o, fetch(o)) for o in p["against_min"]]
            lacking = [o for o, t in ts if t is None]
            if lacking:
                missing = lacking[0]
            else:
                o, t = min(ts, key=lambda ot: ot[1][0])
                ratios.append((value / t[0], f"/ min = {o}",
                               t[1] if isinstance(t[1], float) else 0.0))
        elif "against_workload" in p:
            t = data.suite(candidate, p["against_workload"], p["metric"])
            if t is None:
                missing = f"{candidate} on {p['against_workload']}"
            else:
                ratios.append((value / t[0], f"/ own {p['against_workload']}", t[1]))
        elif "gap_closed" in p:
            a, b = p["gap_closed"]["from"], p["gap_closed"]["to"]
            ta, tb = fetch(a), fetch(b)
            if ta is None or tb is None:
                missing = a if ta is None else b
            elif ta[0] == tb[0]:
                missing = f"no gap between {a} and {b}"
            else:
                ratios.append(((ta[0] - value) / (ta[0] - tb[0]), f"gap {a} to {b} closed",
                               max(ta[1] or 0.0, tb[1] or 0.0)))
        else:
            ratios.append((value, "", 0.0))

        if missing is not None:
            instances.append({"point": label, "status": "untested",
                              "reason": f"{missing} not measured here"})
            continue
        for x, desc, other_noise in ratios:
            ok, ref = check_bound(x, p)
            band = noise + other_noise
            near = ref is not None and ref != 0 and band > 0 and abs(x - ref) / abs(ref) <= band
            instances.append({
                "point": label, "status": "held" if ok else "falsified",
                "value": round(x, 4), "compared": desc, "bound": bound_text(p),
                "within_noise": bool(near), "noise_band": round(band, 4),
                "poor_fit": bool(poor_fit),
            })

    statuses = {i["status"] for i in instances}
    if not instances:
        verdict = "UNTESTED"
    elif "falsified" in statuses:
        verdict = "FALSIFIED"
    elif "untested" in statuses:
        verdict = "UNTESTED"
    else:
        verdict = "HELD"
    return {"id": p["id"], "claim": p["claim"], "verdict": verdict, "instances": instances,
            "decided_within_noise": any(i.get("within_noise") for i in instances
                                        if i["status"] == ("falsified" if verdict == "FALSIFIED"
                                                           else "held"))}


def load_yaml(path):
    import yaml
    with open(path) as f:
        return yaml.safe_load(f)


def write_markdown(out, path):
    lines = ["# Predictions", ""]
    lines.append(
        "Each candidate's falsifiable predictions, as written in its manifest before it "
        "was measured, judged against the measurements by `runner/predictions.py`. A "
        "verdict is computed, not written: HELD means every point the prediction names "
        "satisfied its bound, FALSIFIED means at least one did not, UNTESTED means a "
        "point it needs was not measured.")
    lines.append("")
    lines.append(
        "*within noise* marks a point closer to its bound than the spread between "
        "repetitions of the measurements it came from; a verdict resting on such a point "
        "is weak whichever way it went. *poor fit* marks an exponent whose power-law fit "
        "has r2 below 0.9.")
    lines.append("")
    lines.append(f"- main suite: run `{out.get('results_run', '?')}`")
    lines.append(f"- sweeps: run `{out.get('scaling_run', '?')}`")
    lines.append("")
    total = {"HELD": 0, "FALSIFIED": 0, "UNTESTED": 0}
    for c in out["candidates"].values():
        for p in c:
            total[p["verdict"]] += 1
    lines.append(f"**{total['HELD']} held, {total['FALSIFIED']} falsified, "
                 f"{total['UNTESTED']} untested.**")
    lines.append("")
    lines.append("| candidate | prediction | verdict | weak |")
    lines.append("|---|---|---|---|")
    for cand, preds in sorted(out["candidates"].items()):
        for p in preds:
            weak = "within noise" if p["decided_within_noise"] else ""
            lines.append(f"| `{cand}` | {p['id']}: {p['claim']} | **{p['verdict']}** | {weak} |")
    for cand, preds in sorted(out["candidates"].items()):
        lines.append("")
        lines.append(f"## `{cand}`")
        for p in preds:
            lines.append("")
            lines.append(f"**{p['id']} — {p['verdict']}.** {p['claim']}")
            lines.append("")
            lines.append("| point | value | compared | bound | result | note |")
            lines.append("|---|---:|---|---|---|---|")
            for i in p["instances"]:
                if i["status"] == "untested":
                    lines.append(f"| {i['point']} | | | | untested | {i['reason']} |")
                    continue
                note = []
                if i.get("within_noise"):
                    note.append(f"within noise (band {i['noise_band']:.1%})")
                if i.get("poor_fit"):
                    note.append("poor fit")
                lines.append(f"| {i['point']} | {i['value']} | {i['compared']} | {i['bound']} "
                             f"| {i['status']} | {', '.join(note)} |")
    path.write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=str(ROOT / "benchmarks" / "results.json"))
    ap.add_argument("--scaling", default=str(ROOT / "benchmarks" / "scaling.json"))
    ap.add_argument("--out", default=str(ROOT / "benchmarks" / "predictions.json"))
    ap.add_argument("--report", default=str(ROOT / "benchmarks" / "predictions.md"))
    args = ap.parse_args()

    results = json.loads(Path(args.results).read_text()) if Path(args.results).exists() else {}
    scaling = json.loads(Path(args.scaling).read_text()) if Path(args.scaling).exists() else {}
    data = Data(results, scaling)

    out = {"results_run": results.get("run_id"), "scaling_run": scaling.get("run_id"),
           "candidates": {}}
    problems = []
    for mpath in sorted(ROOT.glob("candidates/*/*/manifest.yaml")):
        m = load_yaml(mpath)
        preds = m.get("predictions")
        if not preds:
            continue
        problems += validate_predictions(preds, mpath.relative_to(ROOT))
        if problems:
            continue
        out["candidates"][m["name"]] = [evaluate(m["name"], p, data) for p in preds]
    if problems:
        for p in problems:
            print(p, file=sys.stderr)
        sys.exit("malformed predictions; nothing was judged")

    Path(args.out).write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
    write_markdown(out, Path(args.report))
    for cand, preds in sorted(out["candidates"].items()):
        for p in preds:
            print(f"[judge]  {cand:20s} {p['id']:4s} {p['verdict']:9s} "
                  f"{'(within noise) ' if p['decided_within_noise'] else ''}{p['claim']}")
    print(f"\npredictions: {args.out}\nreport:      {args.report}")


if __name__ == "__main__":
    main()
