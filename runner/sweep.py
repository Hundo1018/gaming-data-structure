#!/usr/bin/env python3
"""Scaling experiments: measure how cost grows with population.

The main suite (`orchestrate.py`) compares candidates at one size. It cannot say
how anything grows, because no two of its workloads differ only in population.
This tool generates workloads that do, measures each candidate across them, and
fits a growth exponent to the result.

The exponent is the slope of log(cost) against log(population), fitted by least
squares. A structure whose cost per operation does not depend on the population
gives a slope near 0; one that looks at everything gives 1. The fit's r squared
says whether a power law describes the points at all — a low value means the
candidate is not following a single power law over this range, and the exponent
should not be quoted.

Every exponent is measured. Each manifest's `complexity:` field is a claim, and
`compare_declared()` puts the two side by side; nothing here rewrites the claim
to match, and a disagreement is a finding rather than an error.

A family that declares `vary: {key, values}` instead of regimes is a parameter
sweep: population and world come from its template, one workload key takes each
value in turn, and every candidate of the track is measured at each. Nothing is
fitted, because the axis is not a size. The curve goes into `curves` in the
results, apart from `sweeps`, so the declared-complexity comparison never sees
it. Every value is verified against the oracle before the family is measured,
not only the first: a history strategy can be correct at one rewind depth and
wrong at another, and verification at these sizes costs seconds. The candidates
then take turns at each value, because what the family reports is their order.
"""

import argparse
import json
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from archive import Archive  # noqa: E402
from orchestrate import (  # noqa: E402
    build,
    build_flags,
    compiler_version,
    cpu_model,
    discover_candidates,
    git_commit,
    load_yaml,
    parse_workload,
)

ROOT = Path(__file__).resolve().parent.parent
SWEEP_DIR = ROOT / "workloads" / "sweep"


def scale_value(raw, rule, ratio):
    """Applies a regime's scaling rule to one template value."""
    value = float(raw)
    if rule == "linear":
        return value * ratio
    if rule == "cbrt":
        return value * (ratio ** (1.0 / 3.0))
    if rule == "sqrt":
        return value * math.sqrt(ratio)
    raise SystemExit(f"unknown scaling rule: {rule}")


def render(template_text, overrides):
    """Rewrites the named keys of a workload file, leaving everything else alone."""
    out = []
    seen = set()
    for raw in template_text.splitlines():
        stripped = raw.split("#", 1)[0].strip()
        if stripped and ":" in stripped:
            key = stripped.split(":", 1)[0].strip()
            if key in overrides:
                seen.add(key)
                out.append(f"{key}: {overrides[key]}")
                continue
        out.append(raw)
    missing = set(overrides) - seen
    if missing:
        # A key the regime wants to scale that the template never sets would be
        # silently ignored, and the sweep would claim to vary something it did not.
        raise SystemExit(
            f"template does not set {sorted(missing)}, so the sweep cannot scale it"
        )
    return "\n".join(out) + "\n"


def vary_spec(fam, template_spec):
    """Checks a parameter sweep's declaration and returns its key and values."""
    if "regimes" in fam:
        raise SystemExit(f"{fam['id']}: declares both vary and regimes; a family "
                         "varies one thing, and which one would be ambiguous")
    key = (fam["vary"] or {}).get("key")
    values = (fam["vary"] or {}).get("values") or []
    if key in (None, "id", "track", "visibility"):
        raise SystemExit(f"{fam['id']}: vary needs a workload key, not {key!r}")
    if key not in template_spec:
        raise SystemExit(f"{fam['id']}: {fam['template']} does not set {key}, so "
                         "the sweep cannot vary it")
    numeric = all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in values)
    if len(values) < 2 or not numeric or any(b <= a for a, b in zip(values, values[1:])):
        # A crossover is reported as the interval between two neighbouring
        # values, which only means something on an ordered axis.
        raise SystemExit(f"{fam['id']}: vary.values must be at least two numbers in "
                         f"increasing order, got {values!r}")
    if re.fullmatch(r"\d+", template_spec[key]) and not all(
            isinstance(v, int) and v >= 0 for v in values):
        # The template is the statement of what type the key has. A fraction
        # written into a count is refused by the workload parser, and only
        # when the first binary reads it.
        raise SystemExit(f"{fam['id']}: {fam['template']} writes {key} as a whole "
                         f"number ({template_spec[key]}), so every value must be one; "
                         f"got {values!r}")
    return key, values


def _whole(spec, key, where):
    raw = spec.get(key, "0") or "0"
    try:
        return int(raw)
    except ValueError:
        raise SystemExit(f"{where}: {key} is {raw!r}, and the workload parser reads "
                         "it as a whole number") from None


def check_rewind_schedule(spec, where):
    """Refuses a workload in which a scheduled rewind cannot happen, or in which
    a rewind parameter is not what the file says.

    The generator schedules a rewind at every multiple t of rewind_every with
    t > rewind_depth, back to t - rewind_depth - 1. When rewind_depth is not
    less than rewind_every, that target is a tick the previous rewind already
    discarded: no history holds it, the oracle and the candidate both report
    that they could not rewind, and verification passes because they agree. A
    sweep of rewind depth through that boundary would measure fewer rewinds at
    its deep end than it claims to, with nothing in the output to show it.

    The workload parser also raises history_ticks to rewind_depth without
    saying so. A point that sets less history than its depth therefore retains
    more than the report's list of fixed parameters says, and a sweep of
    history_ticks below the depth would give several identical points.

    A rewind of depth 0 goes back to the end of the tick just finished. With no
    history retained, the oracle keeps that one frame and the substrate's
    history wrappers keep none, so every candidate fails verification on a
    disagreement inside the substrate rather than in the candidate.
    """
    every = _whole(spec, "rewind_every", where)
    if every == 0:
        return
    if "rewind_depth" not in spec:
        raise SystemExit(f"{where}: rewinds but does not set rewind_depth; a sweep "
                         "template states what it holds fixed")
    depth = _whole(spec, "rewind_depth", where)
    if depth >= every:
        raise SystemExit(
            f"{where}: rewind_depth {depth} >= rewind_every {every}, so some of its "
            "rewinds target a tick the previous one discarded and do nothing on "
            "either side of the verifier (substrate/src/spatial_workload.c)")
    history = _whole(spec, "history_ticks", where)
    if "history_ticks" in spec and history < depth:
        raise SystemExit(
            f"{where}: history_ticks {history} < rewind_depth {depth}; the workload "
            f"parser raises it to {depth} without saying so, so this point would not "
            "retain the history it states")
    if max(history, depth) == 0:
        raise SystemExit(
            f"{where}: rewind_depth 0 with no history_ticks; the oracle and the "
            "substrate's history wrappers disagree on whether that rewind is "
            "possible, so every candidate would fail verification. Set "
            "history_ticks to at least 1")


def fit_power_law(sizes, values):
    """Least-squares slope of log(value) against log(size), with r squared."""
    pts = [(math.log(s), math.log(v)) for s, v in zip(sizes, values) if s > 0 and v > 0]
    n = len(pts)
    if n < 3:
        return None
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    sxx = sum((p[0] - mx) ** 2 for p in pts)
    sxy = sum((p[0] - mx) * (p[1] - my) for p in pts)
    if sxx == 0:
        return None
    slope = sxy / sxx
    intercept = my - slope * mx
    ss_tot = sum((p[1] - my) ** 2 for p in pts)
    ss_res = sum((p[1] - (slope * p[0] + intercept)) ** 2 for p in pts)
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else 1.0
    return {"exponent": round(slope, 3), "r_squared": round(r2, 4), "points": n}


def run_bench(binary, workload_path, repeats, warmup, timeout):
    cmd = [str(binary), "--workload", str(workload_path), "--mode", "bench",
           "--repeats", str(repeats), "--warmup", str(warmup)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "crashed",
                "failure": (proc.stderr or proc.stdout or "no output")[-300:]}


def run_verify(binary, workload_path, timeout):
    cmd = [str(binary), "--workload", str(workload_path), "--mode", "verify"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "crashed",
                "failure": (proc.stderr or proc.stdout or "no output")[-300:]}


def prepare_vary_family(fam, out_dir):
    """Checks a parameter sweep and writes its workloads, measuring nothing.

    Every check that can refuse a family is static, so all of them run before
    the first measurement of the run. The parameter sweeps come after the
    population families in sweeps.yaml and results are written only at the end,
    so a mistake found when its family is reached would discard every curve
    measured before it.
    """
    template_path = SWEEP_DIR / fam["template"]
    template_text = template_path.read_text()
    template_spec = parse_workload(template_path)
    key, values = vary_spec(fam, template_spec)
    meta = {
        "track": fam["track"],
        "measures": fam.get("measures", "").strip(),
        "checks": fam.get("checks", ""),
        "vary": {"key": key, "values": values},
        "template": fam["template"],
        # Pairs rather than a mapping, so the template's order survives the
        # results file, which is written with sorted keys.
        "fixed": [[k, v] for k, v in template_spec.items()
                  if k not in ("id", "visibility", "track", key)],
    }
    paths = {}
    for v in values:
        path = out_dir / f"{fam['id']}_{v}.workload"
        path.write_text(render(template_text, {"id": f"{fam['id']}_{v}", key: v}))
        check_rewind_schedule(parse_workload(path), path.name)
        paths[v] = path
    return {"key": key, "values": values, "paths": paths, "meta": meta}


def run_vary_family(fam, prepared, cfg, fam_candidates, build_dir, timeout, results):
    """Measures every candidate at each value of a parameter sweep. Fits nothing."""
    key, values, paths = prepared["key"], prepared["values"], prepared["paths"]
    results["families"][fam["id"]] = prepared["meta"]

    passed = []
    for c in fam_candidates:
        binary = build_dir / c["binary"]
        if not binary.exists():
            continue
        failed = None
        for v in values:
            r = run_verify(binary, paths[v], timeout)
            if r.get("status") != "passed":
                failed = (v, r)
                break
        if failed:
            # Wrong at one value means the curve is of a structure known to be
            # incorrect, so none of it is measured.
            v, r = failed
            results["notes"].append(
                f"{c['name']} failed verification on {fam['id']} at {key} {v}: "
                f"{r.get('failure') or r.get('error') or r.get('status')}")
            print(f"[verify] {c['name']:16s} {fam['id']:24s} {key} {v} "
                  f"{r.get('status')}  <-- REJECTED")
            continue
        passed.append(c)

    # What this family reports is the order of two candidates at each value, so
    # the candidates take turns at each value rather than each running its whole
    # curve in a window of its own: on a shared machine a slow minute then lands
    # on both sides of a comparison instead of on one candidate's curve. The
    # order rotates from one value to the next, so no candidate always runs
    # first.
    rows = {c["name"]: {"value": [], "step_ns_p50": [], "step_ns_p99": [],
                        "peak_bytes": [], "checksum": [], "rewind_strategy": [],
                        "rewinds": [], "ticks": [], "rewind_step_ns_p50": []}
            for c in passed}
    for i, v in enumerate(values):
        shift = i % len(passed) if passed else 0
        for c in passed[shift:] + passed[:shift]:
            m = run_bench(build_dir / c["binary"], paths[v], cfg["repeats"],
                          cfg["warmup"], timeout)
            if m.get("status") != "ok":
                results["notes"].append(
                    f"{c['name']} {fam['id']} {key} {v}: {m.get('status')}")
                continue
            row = rows[c["name"]]
            row["value"].append(v)
            row["step_ns_p50"].append(m["step_ns_p50"])
            row["step_ns_p99"].append(m["step_ns_p99"])
            row["peak_bytes"].append(m["peak_bytes"])
            row["checksum"].append(m["checksum"])
            row["rewind_strategy"].append(m.get("rewind_strategy"))
            row["rewinds"].append(m.get("rewinds"))
            row["ticks"].append(m.get("ticks"))
            row["rewind_step_ns_p50"].append(m.get("rewind_step_ns_p50", 0))
            print(f"[vary]   {c['name']:16s} {fam['id']:24s} {key} {v}: p50 "
                  f"{m['step_ns_p50'] / 1000.0:.1f} us, p99 "
                  f"{m['step_ns_p99'] / 1000.0:.1f} us, history "
                  f"{m.get('rewind_strategy')}")
    per_candidate = {}
    for c in passed:
        row = rows[c["name"]]
        per_candidate[c["name"]] = row
        print(f"[vary]   {c['name']:16s} {fam['id']:24s} verified at all {len(values)} "
              f"values of {key}, measured at {len(row['value'])}")

    # The same checks a population family gets, per value: one op stream per
    # value, so every candidate's checksum at that value must be the same.
    for v in values:
        seen = {}
        for name, row in per_candidate.items():
            if v in row["value"]:
                seen.setdefault(row["checksum"][row["value"].index(v)], []).append(name)
        if len(seen) > 1:
            results["notes"].append(
                f"checksum disagreement on {fam['id']} at {key} {v}: " + json.dumps(seen))
    counts = {}
    for row in per_candidate.values():
        for v, n in zip(row["value"], row["rewinds"]):
            counts.setdefault(v, n)
    if len(set(counts.values())) > 1:
        results["notes"].append(
            f"{fam['id']}: the number of rewinds differs across values of {key} "
            f"({json.dumps({str(v): n for v, n in counts.items()})}), so the points "
            "do not rewind equally often")
    results["curves"][fam["id"]] = {"key": key, "values": values,
                                    "candidates": per_candidate}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(ROOT / "build"))
    ap.add_argument("--out-dir", default=str(ROOT / "build" / "sweep"))
    ap.add_argument("--archive", default=str(ROOT / "archive" / "archive.db"))
    ap.add_argument("--results", default=str(ROOT / "benchmarks" / "scaling.json"))
    ap.add_argument("--report", default=str(ROOT / "benchmarks" / "scaling.md"))
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--only", default="", help="comma-separated candidate names")
    ap.add_argument("--families", default="", help="comma-separated family ids")
    ap.add_argument("--verify-smallest", action="store_true", default=True,
                    help="verify each generated workload at the smallest size")
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    if not args.skip_build:
        ok, log = build(build_dir, True)
        if not ok:
            print(log[-3000:], file=sys.stderr)

    cfg = load_yaml(SWEEP_DIR / "sweeps.yaml")
    regimes = {r["id"]: r for r in cfg["regimes"]}
    families = cfg["families"]
    if args.families:
        wanted = {s.strip() for s in args.families.split(",")}
        families = [f for f in families if f["id"] in wanted]

    candidates = discover_candidates()
    if args.only:
        wanted = {s.strip() for s in args.only.split(",")}
        candidates = [c for c in candidates if c["name"] in wanted]
    # A negative control is expected to be rejected, so it has nothing to scale.
    candidates = [c for c in candidates if c.get("origin") != "negative_control"]

    out_dir = Path(args.out_dir)
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    sizes = cfg["sizes"]
    base = cfg["base_size"]
    run_id = "sweep-" + subprocess.run(
        ["date", "-u", "+%Y%m%dT%H%M%SZ"], capture_output=True, text=True
    ).stdout.strip()

    results = {
        "run_id": run_id,
        "git_commit": git_commit(),
        "cpu": cpu_model(),
        "compiler": compiler_version(build_dir),
        "build_flags": build_flags(build_dir),
        "sizes": sizes,
        "base_size": base,
        "repeats": cfg["repeats"],
        "warmup": cfg["warmup"],
        "regimes": {r["id"]: r.get("describes", "").strip() for r in cfg["regimes"]},
        "families": {},
        "sweeps": {},
        "declared": {},
        "notes": [],
    }
    for c in candidates:
        if c.get("complexity"):
            results["declared"][c["name"]] = c["complexity"]

    prepared = {fam["id"]: prepare_vary_family(fam, out_dir)
                for fam in families if "vary" in fam}
    if prepared:
        # Present only when the run has a parameter sweep, so that a run of
        # population families writes the same results file as before they existed.
        results["curves"] = {}

    for fam in families:
        if "vary" in fam:
            fam_candidates = [c for c in candidates if c.get("track") == fam["track"]]
            run_vary_family(fam, prepared[fam["id"]], cfg, fam_candidates, build_dir,
                            args.timeout, results)
            continue
        results["families"][fam["id"]] = {
            "track": fam["track"],
            "measures": fam.get("measures", "").strip(),
            "checks": fam.get("checks", ""),
            "regimes": fam["regimes"],
        }
        template_text = (SWEEP_DIR / fam["template"]).read_text()
        template_spec = parse_workload(SWEEP_DIR / fam["template"])
        fam_candidates = [c for c in candidates if c.get("track") == fam["track"]]

        for regime_id in fam["regimes"]:
            regime = regimes[regime_id]
            key = f"{fam['id']}::{regime_id}"
            paths = {}
            for n in sizes:
                ratio = n / base
                overrides = {"id": f"{fam['id']}_{regime_id}_{n}",
                             "initial_entities": n}
                for k, rule in (regime.get("scale") or {}).items():
                    scaled = scale_value(template_spec[k], rule, ratio)
                    overrides[k] = int(round(scaled))
                path = out_dir / f"{key.replace('::', '_')}_{n}.workload"
                path.write_text(render(template_text, overrides))
                paths[n] = path

            per_candidate = {}
            for c in fam_candidates:
                binary = build_dir / c["binary"]
                if not binary.exists():
                    continue
                if args.verify_smallest:
                    v = run_verify(binary, paths[sizes[0]], args.timeout)
                    if v.get("status") != "passed":
                        results["notes"].append(
                            f"{c['name']} failed verification on {key} at size "
                            f"{sizes[0]}: {v.get('failure', v.get('status'))}"
                        )
                        print(f"[verify] {c['name']:16s} {key:38s} "
                              f"{v.get('status')}  <-- REJECTED")
                        continue
                row = {"size": [], "step_ns_p50": [], "step_ns_p99": [],
                       "peak_bytes": [], "checksum": []}
                for n in sizes:
                    m = run_bench(binary, paths[n], cfg["repeats"], cfg["warmup"],
                                  args.timeout)
                    if m.get("status") != "ok":
                        results["notes"].append(
                            f"{c['name']} {key} size {n}: {m.get('status')}")
                        continue
                    row["size"].append(n)
                    row["step_ns_p50"].append(m["step_ns_p50"])
                    row["step_ns_p99"].append(m["step_ns_p99"])
                    row["peak_bytes"].append(m["peak_bytes"])
                    row["checksum"].append(m["checksum"])
                row["time_fit"] = fit_power_law(row["size"], row["step_ns_p50"])
                row["memory_fit"] = fit_power_law(row["size"], row["peak_bytes"])
                per_candidate[c["name"]] = row
                tf = row["time_fit"]
                mf = row["memory_fit"]
                print(f"[sweep]  {c['name']:16s} {key:38s} "
                      f"time n^{tf['exponent'] if tf else '?'} (r2 "
                      f"{tf['r_squared'] if tf else '?'})  "
                      f"memory n^{mf['exponent'] if mf else '?'}")

            # Every candidate replays the same op stream at a given size, so a
            # checksum that differs between two of them means they are not
            # answering the same question and neither exponent is comparable.
            for i, n in enumerate(sizes):
                seen = {}
                for name, row in per_candidate.items():
                    if n in row["size"]:
                        seen.setdefault(row["checksum"][row["size"].index(n)],
                                        []).append(name)
                if len(seen) > 1:
                    results["notes"].append(
                        f"checksum disagreement on {key} at size {n}: "
                        + json.dumps(seen))
            results["sweeps"][key] = per_candidate

    archive = Archive(args.archive)
    for key, per_candidate in results["sweeps"].items():
        family, regime = key.split("::")
        for name, row in per_candidate.items():
            archive.add_scaling(run_id, name, family, regime, row)
    # The scaling table has no column for a varied key, so a parameter sweep is
    # stored with its key in the regime column and its values where a
    # population family keeps its sizes. No exponent is written for it.
    for family, curve in results.get("curves", {}).items():
        for name, row in curve["candidates"].items():
            archive.add_scaling(run_id, name, family, f"vary:{curve['key']}",
                                {"size": row["value"], "step_ns_p50": row["step_ns_p50"],
                                 "step_ns_p99": row["step_ns_p99"],
                                 "peak_bytes": row["peak_bytes"]})
    archive.close()

    Path(args.results).parent.mkdir(parents=True, exist_ok=True)
    Path(args.results).write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")

    from scaling_report import write_scaling_report  # noqa: E402
    write_scaling_report(results, Path(args.report))
    print(f"\nresults: {args.results}\nreport:  {args.report}")


if __name__ == "__main__":
    main()
