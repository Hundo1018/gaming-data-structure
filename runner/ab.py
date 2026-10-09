#!/usr/bin/env python3
"""Paired comparison of candidates on one workload.

`orchestrate.py` measures each candidate in turn, so on a shared machine a
slow minute lands on whichever candidate happened to be running. This tool
interleaves them instead: every round runs each candidate once, in an order
that rotates between rounds, so drift in the machine is spread over all of them
rather than charged to one.

It reports the median of each candidate's per-round medians, the spread across
rounds, and each candidate's ratio to the first one named. It does not write to
the archive: it is for deciding whether a difference is worth a full run, not a
replacement for one.

    python3 runner/ab.py --workload workloads/public/w04_random_access.workload \
        soa query_memo --rounds 5
"""

import argparse
import json
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def binary_for(name, build_dir):
    """Finds the executable for a candidate name, whichever track it is in."""
    hits = sorted(build_dir.glob(f"gds_*_{name}"))
    hits = [h for h in hits if h.name.split("_", 2)[-1] == name]
    if len(hits) != 1:
        sys.exit(f"expected one binary for '{name}' in {build_dir}, found {[h.name for h in hits]}")
    return hits[0]


def run(binary, workload, mode, repeats, warmup, timeout):
    cmd = [str(binary), "--workload", str(workload), "--mode", mode]
    if mode == "bench":
        cmd += ["--repeats", str(repeats), "--warmup", str(warmup)]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"status": "crashed", "failure": (proc.stderr or proc.stdout)[-400:]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("candidates", nargs="+")
    ap.add_argument("--workload", required=True)
    ap.add_argument("--build-dir", default=str(ROOT / "build"))
    ap.add_argument("--rounds", type=int, default=5)
    ap.add_argument("--repeats", type=int, default=1,
                    help="repetitions inside each binary invocation")
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--metric", default="step_ns_p50")
    ap.add_argument("--no-verify", action="store_true",
                    help="skip the correctness check that precedes measurement")
    ap.add_argument("--timeout", type=int, default=900)
    ap.add_argument("--json", default="", help="write the raw per-round numbers here")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    binaries = {c: binary_for(c, build_dir) for c in args.candidates}

    # Measuring something that answers wrongly is not a comparison, so each
    # candidate is verified first unless told otherwise.
    checksums = {}
    for name, b in binaries.items():
        if not args.no_verify:
            v = run(b, args.workload, "verify", 0, 0, args.timeout)
            if v.get("status") != "passed":
                sys.exit(f"{name} failed verification: {v.get('failure', v.get('status'))}")

    per_round = {c: [] for c in args.candidates}
    peak = {}
    order = list(args.candidates)
    for r in range(args.rounds):
        for name in order:
            m = run(binaries[name], args.workload, "bench", args.repeats, args.warmup,
                    args.timeout)
            if m.get("status") != "ok":
                sys.exit(f"{name} round {r}: {m.get('status')} {m.get('failure', '')}")
            per_round[name].append(m[args.metric])
            peak[name] = m["peak_bytes"]
            checksums.setdefault(m["checksum"], []).append(name)
        order = order[1:] + order[:1]

    if len(checksums) > 1:
        print("WARNING: candidates disagree on the bench checksum:",
              {k: sorted(set(v)) for k, v in checksums.items()}, file=sys.stderr)

    base = statistics.median(per_round[args.candidates[0]])
    print(f"workload: {args.workload}")
    print(f"metric:   {args.metric}, {args.rounds} interleaved rounds, "
          f"{args.repeats} repetition(s) per round")
    print(f"{'candidate':20s} {'median':>12s} {'min':>12s} {'max':>12s} "
          f"{'spread':>8s} {'ratio':>7s} {'peak MB':>8s}")
    for name in args.candidates:
        xs = per_round[name]
        med = statistics.median(xs)
        spread = (max(xs) - min(xs)) / med if med else 0.0
        print(f"{name:20s} {med/1000:10.1f}us {min(xs)/1000:10.1f}us {max(xs)/1000:10.1f}us "
              f"{spread*100:7.1f}% {med/base:7.3f} {peak[name]/1048576:8.2f}")

    if args.json:
        Path(args.json).write_text(json.dumps(
            {"workload": args.workload, "metric": args.metric, "rounds": per_round,
             "peak_bytes": peak}, indent=2) + "\n")


if __name__ == "__main__":
    main()
