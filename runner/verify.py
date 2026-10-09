#!/usr/bin/env python3
"""Verify candidate binaries against every workload of their track.

`orchestrate.py --verify-only` is the gate as the experiment runs it: it reads
every manifest in the tree and checks each candidate against its own
`expect_verify`. That is the wrong tool while a candidate is being written,
because one malformed manifest anywhere stops it. This one reads no manifests:
it takes binary names, infers the track from the name, and runs `--mode verify`
on every workload of that track, plus any extra workload files given.

    python3 runner/verify.py gds_spatial_uniform_grid --build-dir build
    python3 runner/verify.py gds_spatial_mine --extra /tmp/adversarial/*.workload

Exit status is non-zero if any run did not pass.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def track_of(binary_name):
    parts = binary_name.split("_", 2)
    if len(parts) < 3 or parts[0] != "gds":
        sys.exit(f"cannot infer a track from '{binary_name}': expected gds_<track>_<name>")
    return parts[1]


def workload_track(path):
    for raw in Path(path).read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if line.startswith("track:"):
            return line.split(":", 1)[1].strip()
    return "ecs"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binaries", nargs="+", help="e.g. gds_spatial_uniform_grid")
    ap.add_argument("--build-dir", default=str(ROOT / "build"))
    ap.add_argument("--extra", nargs="*", default=[], help="additional workload files")
    ap.add_argument("--timeout", type=int, default=900)
    args = ap.parse_args()

    failures = 0
    for name in args.binaries:
        binary = Path(args.build_dir) / name
        if not binary.exists():
            print(f"{name}: no binary at {binary}")
            failures += 1
            continue
        track = track_of(name)
        paths = [p for vis in ("public", "hidden")
                 for p in sorted((ROOT / "workloads" / vis).glob("*.workload"))
                 if workload_track(p) == track]
        paths += [Path(p) for p in args.extra]
        for p in paths:
            try:
                proc = subprocess.run([str(binary), "--workload", str(p), "--mode", "verify"],
                                      capture_output=True, text=True, timeout=args.timeout)
                r = json.loads(proc.stdout)
            except subprocess.TimeoutExpired:
                r = {"status": "timeout"}
            except json.JSONDecodeError:
                r = {"status": "crashed", "failure": (proc.stderr or proc.stdout)[-300:]}
            status = r.get("status")
            if status != "passed":
                failures += 1
            print(f"{name:32s} {p.name:36s} {status} {r.get('failure', '')}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
