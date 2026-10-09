#!/usr/bin/env python3
"""Check that two builds of the suite give the same answers.

Two builds are equivalent when every candidate binary present in both returns
the same checksums on every workload: the verify checksum (every observation
folded, checked against the oracle) and the bench checksum (the same fold
taken while timing). A checksum is a 64-bit fold of every query result and
every read, so two builds that agree on it agree on every answer, and a
workload generator that drifted by one random draw would change it.

It was written to accept a C11 port of the suite against the C++ build
(benchmarks/language_port.md; the port was accepted, then reverted), and is
kept because the same question comes back whenever the toolchain changes: a
new compiler, new flags, or a substrate refactor must not change any answer.

    python3 runner/equivalence.py --a build-old --b build
    python3 runner/equivalence.py --a build-cpp --b build gds_spatial_uniform_grid \\
        --modes verify --extra /tmp/more/*.workload --json out.json

Also reported, as differences rather than failures: reported_bytes, the
allocator's peak, allocation count and final entity count from one bench
repetition, and the ratio of the two builds' total times. Those are expected
to move with the toolchain and are what the comparison is for once the
answers are known to match.

Exit status is non-zero if any checksum, status or op count differs, or a run
of either build did not complete.
"""

import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Must be identical for the builds to be equivalent. Header fields describe
# the generated workload; the rest are the answers.
STRICT_VERIFY = ["status", "checksum", "ops_checked", "sweeps", "rewinds", "total_ops",
                 "frames", "ticks", "slots", "max_entity_id", "rewind_strategy", "failure"]
STRICT_BENCH = ["checksum", "final_entities", "total_ops", "rewind_strategy", "rewind_steps"]
# Expected to differ between toolchains; reported, never failed on.
SOFT_BENCH = ["reported_bytes", "peak_bytes", "alloc_count", "bytes_per_entity"]


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


def run(binary, workload, mode, timeout):
    cmd = [str(binary), "--workload", str(workload), "--mode", mode]
    if mode == "bench":
        cmd += ["--repeats", "1", "--warmup", "0"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}
    try:
        out = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "crashed", "failure": (proc.stderr or proc.stdout)[-300:],
                "exit": proc.returncode}
    if mode == "bench" and "checksum" in out:
        out.setdefault("status", "completed")
    return out


def compare(name, workload, mode, a, b):
    """One row: the fields that differ, split into strict and soft."""
    strict = STRICT_VERIFY if mode == "verify" else STRICT_BENCH
    row = {"binary": name, "workload": Path(workload).name, "mode": mode,
           "a_status": a.get("status"), "b_status": b.get("status"),
           "strict_diffs": {}, "soft_diffs": {}}
    incomplete = [s for s in (a.get("status"), b.get("status"))
                  if s in ("timeout", "crashed", None)]
    if incomplete:
        row["strict_diffs"]["run"] = [a.get("status"), b.get("status")]
    for key in strict:
        if key in a or key in b:
            if a.get(key) != b.get(key):
                row["strict_diffs"][key] = [a.get(key), b.get(key)]
    if mode == "bench":
        for key in SOFT_BENCH:
            if a.get(key) != b.get(key):
                row["soft_diffs"][key] = [a.get(key), b.get(key)]
        ta, tb = a.get("total_ns"), b.get("total_ns")
        if ta and tb:
            row["time_ratio_b_over_a"] = round(tb / ta, 3)
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binaries", nargs="*", help="binary names; default: every gds_* in both")
    ap.add_argument("--a", required=True, help="first build directory (the reference)")
    ap.add_argument("--b", required=True, help="second build directory")
    ap.add_argument("--modes", nargs="+", default=["verify", "bench"],
                    choices=["verify", "bench"])
    ap.add_argument("--extra", nargs="*", default=[], help="additional workload files")
    ap.add_argument("--only-extra", action="store_true",
                    help="skip the workloads under workloads/ and run only --extra")
    ap.add_argument("--jobs", type=int, default=2,
                    help="parallel runs; timings are reported but not the point here")
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--json", default="", help="write every row to this file")
    args = ap.parse_args()

    a_dir, b_dir = Path(args.a), Path(args.b)
    names = args.binaries or sorted(
        p.name for p in a_dir.glob("gds_*")
        if is_binary(p) and is_binary(b_dir / p.name) and track_of(p.name)
        and not p.name.startswith("gds_floor"))
    if not names:
        sys.exit("no binaries present in both build directories")

    workloads = []
    if not args.only_extra:
        workloads += [p for vis in ("public", "hidden")
                      for p in sorted((ROOT / "workloads" / vis).glob("*.workload"))]
    workloads += [Path(p) for p in args.extra]

    jobs = []
    for name in names:
        track = track_of(name)
        for w in workloads:
            if workload_track(w) != track:
                continue
            for mode in args.modes:
                jobs.append((name, w, mode))

    def one(job):
        name, w, mode = job
        a = run(a_dir / name, w, mode, args.timeout)
        b = run(b_dir / name, w, mode, args.timeout)
        return compare(name, w, mode, a, b)

    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for row in pool.map(one, jobs):
            rows.append(row)
            mark = "DIFF" if row["strict_diffs"] else "same"
            soft = ""
            if row["soft_diffs"]:
                soft = " soft:" + ",".join(f"{k}={v[0]}->{v[1]}"
                                           for k, v in row["soft_diffs"].items())
            ratio = f" time b/a={row['time_ratio_b_over_a']}" if "time_ratio_b_over_a" in row else ""
            strict = f" {row['strict_diffs']}" if row["strict_diffs"] else ""
            print(f"{mark} {row['binary']:32s} {row['workload']:34s} {row['mode']:6s}"
                  f"{strict}{soft}{ratio}", flush=True)

    differing = [r for r in rows if r["strict_diffs"]]
    print(f"\n{len(rows)} comparisons, {len(differing)} with differing answers or runs")
    # Two builds can agree by both failing; the statuses say whether they did.
    tally = {}
    for r in rows:
        key = f"{r['mode']} {r['a_status']}/{r['b_status']}"
        tally[key] = tally.get(key, 0) + 1
    for key in sorted(tally):
        print(f"  {key}: {tally[key]}")
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1) + "\n")
    sys.exit(1 if differing else 0)


if __name__ == "__main__":
    main()
