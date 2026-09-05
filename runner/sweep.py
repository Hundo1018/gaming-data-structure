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
"""

import argparse
import json
import math
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

    for fam in families:
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
    archive.close()

    Path(args.results).parent.mkdir(parents=True, exist_ok=True)
    Path(args.results).write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")

    from scaling_report import write_scaling_report  # noqa: E402
    write_scaling_report(results, Path(args.report))
    print(f"\nresults: {args.results}\nreport:  {args.report}")


if __name__ == "__main__":
    main()
