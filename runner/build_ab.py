#!/usr/bin/env python3
"""Paired timing of two builds of the suite.

`runner/equivalence.py` says whether two builds give the same answers. This
says how their speed differs once they do: for every binary present in both
build directories and every workload of its track, it runs the two binaries
alternately for a number of rounds, swapping which goes first each round, and
takes the ratio b/a within each round. Drift on a shared machine then lands on
both halves of a pair instead of on whichever build ran during a slow minute,
and the spread of the per-round ratios says how much a ratio can be trusted.

It was written to measure what a C11 port of the suite changed against the
C++ build (benchmarks/language_port.md; the port was reverted), and is kept
for the same question about any later change of toolchain or flags.

    python3 runner/build_ab.py --a build-old --b build --rounds 5 \\
        --json /tmp/timing.json --md /tmp/timing.md

Runs are serial: a timing comparison under parallel load measures the load.
A pair whose checksums differ is reported and excluded, because a speed ratio
between two builds that answer differently compares two different things.
"""

import argparse
import json
import math
import os
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def is_binary(path):
    """An executable file: a build directory also holds gds_build_info.txt."""
    return path.is_file() and os.access(path, os.X_OK)


def track_of(binary_name):
    parts = binary_name.split("_", 2)
    if len(parts) < 3 or parts[0] != "gds":
        return None
    return parts[1]


def workload_track(path):
    for raw in Path(path).read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if line.startswith("track:"):
            return line.split(":", 1)[1].strip()
    return "ecs"


def bench(binary, workload, repeats, warmup, timeout):
    cmd = [str(binary), "--workload", str(workload), "--mode", "bench",
           "--repeats", str(repeats), "--warmup", str(warmup)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return json.loads(proc.stdout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}
    except json.JSONDecodeError:
        return {"status": "crashed"}


def geomean(xs):
    xs = [x for x in xs if x and x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else None


def measure_pair(a, b, workload, args):
    """Per-round metrics for both builds and the per-round ratios b/a."""
    rows = {"a": [], "b": [], "ratio": [], "a_peak": None, "b_peak": None,
            "checksums_equal": True, "status": "ok",
            "also": {m: {"a": [], "b": []} for m in args.also}}
    for r in range(args.rounds):
        order = [("a", a), ("b", b)] if r % 2 == 0 else [("b", b), ("a", a)]
        got = {}
        for side, binary in order:
            m = bench(binary, workload, args.repeats, args.warmup, args.timeout)
            if m.get("status") != "ok":
                rows["status"] = f"{side}: {m.get('status')}"
                return rows
            got[side] = m
        if got["a"]["checksum"] != got["b"]["checksum"]:
            rows["checksums_equal"] = False
            rows["status"] = "checksums differ"
            return rows
        rows["a"].append(got["a"][args.metric])
        rows["b"].append(got["b"][args.metric])
        for m in args.also:
            rows["also"][m]["a"].append(got["a"].get(m))
            rows["also"][m]["b"].append(got["b"].get(m))
        rows["ratio"].append(got["b"][args.metric] / got["a"][args.metric]
                             if got["a"][args.metric] else None)
        rows["a_peak"] = got["a"]["peak_bytes"]
        rows["b_peak"] = got["b"]["peak_bytes"]
    return rows


def write_markdown(result, path, a_label, b_label):
    lines = [f"# Paired timing: {b_label} against {a_label}", "",
             f"Metric `{result['metric']}`, {result['rounds']} rounds per pair alternating "
             f"which build runs first, {result['repeats']} repetition(s) inside each run, "
             f"{result['warmup']} warmup. Ratio is {b_label}/{a_label} within a round; "
             "below 1 means the second build is faster. The range is the lowest and "
             "highest per-round ratio.", ""]
    lines.append("| binary | geomean ratio | workloads | lowest median | highest median |")
    lines.append("|---|---:|---:|---:|---:|")
    for name, s in sorted(result["summary"].items()):
        if not s["workloads"]:
            continue
        if s["geomean_ratio"] is None:
            lines.append(f"| `{name}` | — | {s['workloads']} | — | — |")
            continue
        lines.append(f"| `{name}` | {s['geomean_ratio']:.3f} | {s['workloads']} | "
                     f"{s['lowest'][1]:.3f} ({s['lowest'][0]}) | "
                     f"{s['highest'][1]:.3f} ({s['highest'][0]}) |")
    if result.get("overall_geomean") is not None:
        lines += ["", f"Geometric mean over every pair: {result['overall_geomean']:.3f}."]
    lines += ["", "## Every pair", "",
              "| binary | workload | a median | b median | median ratio | range | peak a → b |",
              "|---|---|---:|---:|---:|---|---|"]
    for p in result["pairs"]:
        if p["status"] != "ok":
            lines.append(f"| `{p['binary']}` | {p['workload']} | — | — | — | {p['status']} | — |")
            continue
        rs = [r for r in p["ratio"] if r]
        lines.append(
            f"| `{p['binary']}` | {p['workload']} | {statistics.median(p['a'])/1e6:.2f} ms | "
            f"{statistics.median(p['b'])/1e6:.2f} ms | {statistics.median(rs):.3f} | "
            f"{min(rs):.3f}–{max(rs):.3f} | {p['a_peak']} → {p['b_peak']} |")
    path.write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binaries", nargs="*", help="binary names; default: every gds_* in both")
    ap.add_argument("--a", required=True, help="first build directory (the reference)")
    ap.add_argument("--b", required=True, help="second build directory")
    ap.add_argument("--a-label", default="a")
    ap.add_argument("--b-label", default="b")
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--metric", default="total_ns")
    ap.add_argument("--also", nargs="*", default=["step_ns_p50"],
                    help="further metrics recorded per round in the JSON, not ratioed")
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--workloads", nargs="*", default=[],
                    help="workload names (file stems) to restrict to; default: all")
    ap.add_argument("--json", default="")
    ap.add_argument("--md", default="")
    ap.add_argument("--from-json", default="",
                    help="re-render --md from a JSON this tool wrote, measuring nothing")
    args = ap.parse_args()

    if args.from_json:
        result = json.loads(Path(args.from_json).read_text())
        if not args.md:
            sys.exit("--from-json needs --md")
        write_markdown(result, Path(args.md), args.a_label, args.b_label)
        return

    a_dir, b_dir = Path(args.a), Path(args.b)
    names = args.binaries or sorted(
        p.name for p in a_dir.glob("gds_*")
        if is_binary(p) and is_binary(b_dir / p.name) and track_of(p.name)
        and not p.name.startswith("gds_floor"))
    workloads = [p for vis in ("public", "hidden")
                 for p in sorted((ROOT / "workloads" / vis).glob("*.workload"))
                 if not args.workloads or p.stem in args.workloads]

    pairs = []
    for name in names:
        for w in workloads:
            if workload_track(w) != track_of(name):
                continue
            rows = measure_pair(a_dir / name, b_dir / name, w, args)
            rows.update({"binary": name, "workload": w.stem})
            pairs.append(rows)
            if rows["status"] == "ok":
                rs = [r for r in rows["ratio"] if r]
                print(f"{name:32s} {w.stem:26s} ratio {statistics.median(rs):6.3f} "
                      f"[{min(rs):.3f}, {max(rs):.3f}]", flush=True)
            else:
                print(f"{name:32s} {w.stem:26s} {rows['status']}", flush=True)

    summary = {}
    for name in names:
        mine = [p for p in pairs if p["binary"] == name and p["status"] == "ok"]
        med = [(p["workload"], statistics.median([r for r in p["ratio"] if r])) for p in mine]
        summary[name] = {
            "workloads": len(med),
            "geomean_ratio": geomean([m for _, m in med]),
            "lowest": min(med, key=lambda x: x[1]) if med else None,
            "highest": max(med, key=lambda x: x[1]) if med else None,
        }
    every = [statistics.median([r for r in p["ratio"] if r])
             for p in pairs if p["status"] == "ok"]
    result = {"a": str(a_dir), "b": str(b_dir), "metric": args.metric, "rounds": args.rounds,
              "repeats": args.repeats, "warmup": args.warmup, "pairs": pairs,
              "summary": summary, "overall_geomean": geomean(every)}
    print(f"\noverall geomean ratio b/a: {result['overall_geomean']}")
    if args.json:
        Path(args.json).write_text(json.dumps(result, indent=1) + "\n")
    if args.md:
        write_markdown(result, Path(args.md), args.a_label, args.b_label)
    bad = [p for p in pairs if p["status"] != "ok"]
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
