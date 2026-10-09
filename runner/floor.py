#!/usr/bin/env python3
"""Headroom: how far each spatial candidate is from the cost nothing can avoid.

`gds_floor_spatial` measures, per workload, the floor of a tick — applying the
tick's mutations to flat arrays, plus digesting exactly the entities in every
answer from packed buffers — and how many entities the reference grid's query
box admits per entity actually in the answer.

This script runs it over every spatial workload, writes `benchmarks/floor.json`,
and, when `benchmarks/results.json` exists, writes `benchmarks/floor.md` with
each candidate's median tick as a multiple of the floor. A workload where the
best candidate sits near 1x has nothing left for a new structure to win; one
where it sits at 10x is where to look.

The floor and the candidates are measured in different processes and possibly
at different times on a shared machine, so a ratio is an estimate of headroom,
not a measurement of it. Run this right after `orchestrate.py` to keep the two
close together.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from orchestrate import build, discover_workloads  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def run_floor(binary, workload, reps, timeout):
    proc = subprocess.run([str(binary), "--workload", str(workload), "--reps", str(reps)],
                          capture_output=True, text=True, timeout=timeout)
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "crashed", "failure": (proc.stderr or proc.stdout)[-300:]}


def write_markdown(floor, results, path):
    lines = ["# Headroom", ""]
    lines.append(
        "The floor of a tick is what it would cost if finding every answer were free: "
        "applying the tick's inserts, removes and moves to flat arrays, plus folding the "
        "digest of exactly the entities in every answer from packed buffers. Everything a "
        "candidate spends above it is search. It is measured by `gds_floor_spatial` and "
        "every answer it digests is checked against the oracle's.")
    lines.append("")
    lines.append(
        "`over-admission` is how many entities the reference grid's query box hands to the "
        "exact distance test per entity actually in the answer: what a tighter broad phase "
        "could remove. `hits/query` is the answer size, which nothing can remove.")
    lines.append("")
    lines.append(
        "Ratios divide a median tick from `benchmarks/results.json` by a floor measured in a "
        "separate process. They estimate headroom; they are not a measurement of it.")
    lines.append("")
    if results:
        lines.append(f"- candidates: run `{results.get('run_id', '?')}`")
    lines.append(f"- floor: {floor['reps']} repetitions per tick, minimum taken")
    lines.append("")
    lines.append("| workload | floor p50 (us) | of which state | of which answers | hits/query "
                 "| over-admission | best candidate | its p50 (us) | x floor |")
    lines.append("|---|---:|---:|---:|---:|---:|---|---:|---:|")
    per_candidate = {}
    for name, f in floor["workloads"].items():
        if f.get("status") != "ok":
            lines.append(f"| `{name}` | {f.get('status')} | | | | | | | |")
            continue
        best_name, best = "", None
        if results:
            for cand, per_w in results.get("measurements", {}).items():
                m = per_w.get(name)
                if not m or m.get("status") != "ok":
                    continue
                ratio = m["step_ns_p50"] / f["floor_ns_p50"] if f["floor_ns_p50"] else None
                per_candidate.setdefault(cand, {})[name] = ratio
                if best is None or m["step_ns_p50"] < best:
                    best, best_name = m["step_ns_p50"], cand
        ratio = f"{best / f['floor_ns_p50']:.1f}" if best and f["floor_ns_p50"] else ""
        lines.append(
            f"| `{name}` | {f['floor_ns_p50']/1000:.1f} | {f['state_ns_p50']/1000:.1f} | "
            f"{f['answer_ns_p50']/1000:.1f} | {f['hits_per_radius_query']:.1f} | "
            f"{f['grid_over_admission']:.2f} | {('`' + best_name + '`') if best_name else ''} | "
            f"{(best/1000) if best else 0:.1f} | {ratio} |")
    if per_candidate:
        names = list(floor["workloads"].keys())
        lines.append("")
        lines.append("## Every candidate, as a multiple of the floor")
        lines.append("")
        lines.append("| candidate | " + " | ".join(f"`{n}`" for n in names) + " |")
        lines.append("|---|" + "---:|" * len(names))
        for cand in sorted(per_candidate):
            row = per_candidate[cand]
            cells = [f"{row[n]:.1f}" if row.get(n) else "" for n in names]
            lines.append(f"| `{cand}` | " + " | ".join(cells) + " |")
    path.write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(ROOT / "build"))
    ap.add_argument("--results", default=str(ROOT / "benchmarks" / "results.json"))
    ap.add_argument("--out", default=str(ROOT / "benchmarks" / "floor.json"))
    ap.add_argument("--report", default=str(ROOT / "benchmarks" / "floor.md"))
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    if not args.skip_build:
        ok, log = build(build_dir, True)
        if not ok:
            print(log[-3000:], file=sys.stderr)
    binary = build_dir / "gds_floor_spatial"
    if not binary.exists():
        sys.exit(f"no floor binary at {binary}")

    floor = {"reps": args.reps, "workloads": {}}
    for w in discover_workloads():
        if w["track"] != "spatial":
            continue
        f = run_floor(binary, w["path"], args.reps, args.timeout)
        floor["workloads"][w["name"]] = f
        if f.get("status") == "ok":
            print(f"[floor]  {w['name']:24s} p50={f['floor_ns_p50']/1000:8.1f}us  "
                  f"hits/q={f['hits_per_radius_query']:7.1f}  "
                  f"over-admission={f['grid_over_admission']:.2f}")
        else:
            print(f"[floor]  {w['name']:24s} {f.get('status')}")

    Path(args.out).write_text(json.dumps(floor, indent=2, sort_keys=True) + "\n")
    results = None
    if Path(args.results).exists():
        results = json.loads(Path(args.results).read_text())
    write_markdown(floor, results, Path(args.report))
    print(f"\nfloor:  {args.out}\nreport: {args.report}")


if __name__ == "__main__":
    main()
