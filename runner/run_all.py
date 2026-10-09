#!/usr/bin/env python3
"""The whole experiment, in the order its parts depend on each other.

    1. orchestrate.py   build, verify every candidate, measure the survivors
    2. floor.py         the irreducible cost of each spatial workload, measured
                        right after the suite so the two are close in time
    3. sweep.py         growth with population, and the families that vary a
                        workload key
    4. predictions.py   every preregistered prediction judged against 1 and 3

Each step is its own script and can be run alone; this one exists so that a
complete result is one command, run serially, with nothing else measuring on
the machine at the same time. Steps after a failed one are not run, because
each reads what the one before wrote.
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "runner"


def step(name, cmd):
    print(f"\n==== {name}: {' '.join(cmd)}", flush=True)
    t0 = time.monotonic()
    rc = subprocess.run(cmd, cwd=ROOT).returncode
    print(f"==== {name}: exit {rc} after {time.monotonic() - t0:.0f}s", flush=True)
    if rc != 0:
        sys.exit(f"{name} failed; later steps read its output and were not run")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repeats", type=int, default=5, help="main-suite repetitions")
    ap.add_argument("--skip-sweep", action="store_true")
    ap.add_argument("--families", default="", help="passed to sweep.py")
    args = ap.parse_args()

    py = sys.executable
    step("suite", [py, str(RUNNER / "orchestrate.py"), "--repeats", str(args.repeats)])
    step("floor", [py, str(RUNNER / "floor.py"), "--skip-build"])
    if not args.skip_sweep:
        cmd = [py, str(RUNNER / "sweep.py"), "--skip-build"]
        if args.families:
            cmd += ["--families", args.families]
        step("sweep", cmd)
    step("predictions", [py, str(RUNNER / "predictions.py")])


if __name__ == "__main__":
    main()
