#!/usr/bin/env python3
"""Where does the run-to-run spread of the timings come from?

The paired timing of the C port (benchmarks/language_port.md) saw a median
per-round spread of 26-29%, and runner/predictions.py marks about a third of
its verdicts *within noise*. This runs the same binary on the same workload
many times under several conditions and splits the spread into two parts:

- within a process: the spread of the repetitions of one invocation, which
  share an address layout, a heap and a CPU unless the scheduler moves them;
- between processes: the spread of the per-invocation medians, which also
  includes everything that changes from one process to the next (ASLR, page
  placement, which CPU, what the host was doing that second).

Conditions are interleaved round by round, in a rotating order, so drift of
the machine lands on every condition alike instead of on whichever ran last.
For every invocation the steal time of the guest (/proc/stat) is recorded, so
a slow invocation can be checked against the host taking the CPU away.

    python3 runner/noise_probe.py --binaries gds_ecs_aos gds_spatial_uniform_grid \\
        --workloads w01_steady_uniform s01_steady_uniform --invocations 12 \\
        --conditions plain pinned noaslr pinned_noaslr --json /tmp/noise.json

Spread is (max - min) / median, the measure build_ab.py and rank_stability.py
report, and also the interquartile range over the median, which one outlier
cannot move.
"""

import argparse
import json
import os
import resource
import statistics
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CONDITIONS = {
    "plain": [],
    "pinned": ["taskset", "-c", "{cpu}"],
    "noaslr": ["setarch", "-R"],
    "pinned_noaslr": ["taskset", "-c", "{cpu}", "setarch", "-R"],
    # glibc keeps freed memory instead of returning it to the kernel, so a
    # repetition reuses pages an earlier one faulted in instead of faulting in
    # fresh zeroed ones.
    "retain": ["env", "GLIBC_TUNABLES=glibc.malloc.mmap_threshold=4294967295:"
               "glibc.malloc.trim_threshold=4294967295:glibc.malloc.top_pad=268435456"],
}


def workload_path(name):
    for sub in ("public", "hidden", "port_review"):
        p = ROOT / "workloads" / sub / f"{name}.workload"
        if p.exists():
            return p
    p = Path(name)
    if p.exists():
        return p
    raise SystemExit(f"no workload {name}")


def cpu_ticks():
    """(busy, steal) jiffies summed over all CPUs."""
    f = open("/proc/stat").readline().split()[1:]
    v = [int(x) for x in f]
    user, nice, system, idle, iowait, irq, softirq, steal = v[:8]
    return user + nice + system + irq + softirq, steal


def spread(xs):
    xs = sorted(xs)
    med = statistics.median(xs)
    return (xs[-1] - xs[0]) / med if med else 0.0


def iqr(xs):
    if len(xs) < 4:
        return spread(xs)
    q = statistics.quantiles(xs, n=4)
    med = statistics.median(xs)
    return (q[2] - q[0]) / med if med else 0.0


def run(binary, workload, cond, args):
    prefix = [p.format(cpu=args.cpu) for p in CONDITIONS[cond]]
    cmd = prefix + [str(binary), "--workload", str(workload), "--mode", "bench",
                    "--repeats", str(args.repeats), "--warmup", str(args.warmup)]
    b0, s0 = cpu_ticks()
    r0 = resource.getrusage(resource.RUSAGE_CHILDREN)
    t0 = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=args.timeout)
    wall = time.perf_counter() - t0
    r1 = resource.getrusage(resource.RUSAGE_CHILDREN)
    b1, s1 = cpu_ticks()
    out = json.loads(proc.stdout)
    return {"total": out["repetition_total_ns"], "p50": out["repetition_step_ns_p50"],
            "wall_s": wall, "steal_jiffies": s1 - s0, "busy_jiffies": b1 - b0,
            "minor_faults": r1.ru_minflt - r0.ru_minflt,
            "sys_s": r1.ru_stime - r0.ru_stime, "user_s": r1.ru_utime - r0.ru_utime}


def summarise(invocations, key):
    within = [spread(i[key]) for i in invocations if len(i[key]) > 1]
    medians = [statistics.median(i[key]) for i in invocations]
    return {
        "within_median": statistics.median(within) if within else None,
        "between_range": spread(medians),
        "between_iqr": iqr(medians),
        "invocation_medians": medians,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=str(ROOT / "build"))
    ap.add_argument("--binaries", nargs="+", required=True)
    ap.add_argument("--workloads", nargs="+", required=True,
                    help="names; each binary runs only the workloads of its track")
    ap.add_argument("--conditions", nargs="+", default=list(CONDITIONS), choices=list(CONDITIONS))
    ap.add_argument("--invocations", type=int, default=10)
    ap.add_argument("--repeats", type=int, default=5)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--cpu", type=int, default=os.cpu_count() - 1, help="CPU for the pinned conditions")
    ap.add_argument("--timeout", type=float, default=600)
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    cells = []
    for b in args.binaries:
        if b == "noise_control":
            # runner/noise_control.cpp: the same timing with no candidate in it.
            cells += [(b, w, w) for w in ("compute", "cache", "memory")]
            continue
        track = b.split("_", 2)[1]
        for w in args.workloads:
            wp = workload_path(w)
            if f"track: {track}" not in wp.read_text() and not (track == "ecs" and "track:" not in wp.read_text()):
                continue
            cells.append((b, w, wp))
    results = {(b, w, c): [] for b, w, _ in cells for c in args.conditions}
    for r in range(args.invocations):
        conds = args.conditions[r % len(args.conditions):] + args.conditions[:r % len(args.conditions)]
        for b, w, wp in cells:
            for c in conds:
                results[(b, w, c)].append(run(Path(args.build) / b, wp, c, args))
        print(f"round {r + 1}/{args.invocations} done", flush=True)

    out = {"repeats": args.repeats, "warmup": args.warmup, "invocations": args.invocations,
           "cpu": args.cpu, "cells": []}
    print(f"{'binary':28s} {'workload':22s} {'condition':14s} "
          f"{'total within':>12s} {'between':>8s} {'iqr':>6s}   "
          f"{'p50 within':>10s} {'between':>8s} {'iqr':>6s}  steal    faults    sys")
    for b, w, _ in cells:
        for c in args.conditions:
            inv = results[(b, w, c)]
            t, p = summarise(inv, "total"), summarise(inv, "p50")
            steal = sum(i["steal_jiffies"] for i in inv)
            faults = statistics.median(i["minor_faults"] for i in inv)
            sys_share = statistics.median(i["sys_s"] / max(i["sys_s"] + i["user_s"], 1e-9) for i in inv)
            out["cells"].append({"binary": b, "workload": w, "condition": c,
                                 "total_ns": t, "step_ns_p50": p, "steal_jiffies": steal,
                                 "minor_faults": faults, "sys_share": sys_share,
                                 "raw": inv})
            print(f"{b:28s} {w:22s} {c:14s} {t['within_median']:12.1%} {t['between_range']:8.1%} "
                  f"{t['between_iqr']:6.1%}   {p['within_median']:10.1%} {p['between_range']:8.1%} "
                  f"{p['between_iqr']:6.1%}  {steal:5d} {faults:9.0f} {sys_share:6.1%}")
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1) + "\n")


if __name__ == "__main__":
    main()
