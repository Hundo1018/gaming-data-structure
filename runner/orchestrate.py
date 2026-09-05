#!/usr/bin/env python3
"""Build, verify, measure, archive, report.

Order is not negotiable: a candidate that fails verification on any workload is
never measured, and its absence from the results is recorded as a rejection
rather than as a gap.
"""

import argparse
import datetime
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import manifest as manifest_schema  # noqa: E402
import pareto  # noqa: E402
from archive import Archive  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent

# Objectives, all minimised. Latency is split into typical and tail because a
# structure can be good at one and bad at the other, and collapsing them would
# hide exactly the candidates worth looking at.
OBJECTIVES = [
    ("step_ns_p50", "median step ns"),
    ("step_ns_p99", "p99 step ns"),
    ("peak_bytes", "peak bytes"),
]


def load_yaml(path):
    try:
        import yaml
    except ImportError:
        sys.exit(
            "PyYAML is required to read candidate manifests. Install it with:\n"
            "    python3 -m pip install pyyaml"
        )
    with open(path) as f:
        return yaml.safe_load(f)


def parse_workload(path):
    """Reads the flat `key: value` workload format, same grammar as the C++ parser."""
    spec = {}
    for raw in Path(path).read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        key, _, value = line.partition(":")
        spec[key.strip()] = value.strip()
    return spec


def cpu_model():
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def git_commit():
    try:
        return subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def compiler_version(build_dir):
    cache = build_dir / "CMakeCache.txt"
    cxx = "unknown"
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith("CMAKE_CXX_COMPILER:"):
                cxx = line.split("=", 1)[1]
    try:
        out = subprocess.run([cxx, "--version"], capture_output=True, text=True).stdout
        return out.splitlines()[0] if out else cxx
    except OSError:
        return cxx


def build(build_dir, native_arch):
    if not shutil.which("cmake"):
        sys.exit("cmake not found")
    subprocess.run(
        ["cmake", "-S", str(ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release",
         "-DGDS_NATIVE_ARCH=" + ("ON" if native_arch else "OFF")],
        check=True, capture_output=True, text=True,
    )
    # Candidates are separate targets on purpose: one that does not compile is
    # reported as a compile failure and the rest of the population still runs.
    result = subprocess.run(
        ["cmake", "--build", str(build_dir), "--parallel"],
        capture_output=True, text=True,
    )
    return result.returncode == 0, result.stdout + result.stderr


def build_flags(build_dir):
    info = build_dir / "gds_build_info.txt"
    if not info.exists():
        return "unknown"
    for line in info.read_text().splitlines():
        if line.startswith("flags="):
            return line.split("=", 1)[1]
    return "unknown"


def discover_candidates(strict=True):
    """Loads every candidate manifest, checking it against the declared schema.

    A malformed manifest stops the run rather than being measured: a candidate
    whose claims are not readable is not a research object, and silently
    measuring it would put numbers in the archive with nothing to attach them to.
    """
    out = []
    problems = []
    for manifest_path in sorted(ROOT.glob("candidates/*/*/manifest.yaml")):
        m = load_yaml(manifest_path)
        problems.extend(manifest_schema.validate(m, manifest_path.relative_to(ROOT)))
        m["_dir"] = manifest_path.parent
        out.append(m)
    if problems and strict:
        for p in problems:
            print(p, file=sys.stderr)
        sys.exit("manifest schema violations; nothing was measured")
    return out


def discover_workloads():
    out = []
    for visibility in ("public", "hidden"):
        for path in sorted((ROOT / "workloads" / visibility).glob("*.workload")):
            spec = parse_workload(path)
            out.append(
                {
                    "name": spec.get("id", path.stem),
                    "visibility": spec.get("visibility", visibility),
                    "track": spec.get("track", "ecs"),
                    "path": path,
                    "spec": spec,
                }
            )
    return out


def workloads_for(candidate, workloads):
    """A candidate only ever meets workloads of its own track.

    Tracks ask different questions of different structures. Running one track's
    workload against the other's candidate would not fail, it would simply
    refuse to parse, and the refusal would show up as a gap rather than as the
    category error it is.
    """
    track = candidate.get("track", "ecs")
    return [w for w in workloads if w["track"] == track]


def run_binary(binary, workload_path, mode, repeats, warmup, timeout):
    cmd = [str(binary), "--workload", str(workload_path), "--mode", mode]
    if mode == "bench":
        cmd += ["--repeats", str(repeats), "--warmup", str(warmup)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {"status": "timeout", "failure": f"exceeded {timeout}s"}
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {
            "status": "crashed",
            "failure": (proc.stderr or proc.stdout or "no output")[-400:],
            "returncode": proc.returncode,
        }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(ROOT / "build"))
    ap.add_argument("--archive", default=str(ROOT / "archive" / "archive.db"))
    ap.add_argument("--results", default=str(ROOT / "benchmarks" / "results.json"))
    ap.add_argument("--report", default=str(ROOT / "benchmarks" / "report.md"))
    ap.add_argument("--repeats", type=int, default=5)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--timeout", type=int, default=900)
    ap.add_argument("--no-native-arch", action="store_true")
    ap.add_argument("--only", default="", help="comma-separated candidate names")
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    if not args.skip_build:
        ok, log = build(build_dir, not args.no_native_arch)
        if not ok:
            print(log[-4000:], file=sys.stderr)
            print("build reported failures; candidates that did compile will still run.",
                  file=sys.stderr)

    run_id = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    archive = Archive(args.archive)
    archive.add_run(
        {
            "run_id": run_id,
            "started_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "git_commit": git_commit(),
            "host": platform.node(),
            "cpu": cpu_model(),
            "compiler": compiler_version(build_dir),
            "build_flags": build_flags(build_dir),
            "repeats": args.repeats,
            "warmup": args.warmup,
        }
    )

    candidates = discover_candidates()
    if args.only:
        wanted = {s.strip() for s in args.only.split(",")}
        candidates = [c for c in candidates if c["name"] in wanted]
    workloads = discover_workloads()

    for w in workloads:
        archive.add_workload(run_id, w["name"], w["visibility"], w["track"], w["path"],
                             Path(w["path"]).read_text())

    results = {
        "run_id": run_id,
        "git_commit": git_commit(),
        "cpu": cpu_model(),
        "compiler": compiler_version(build_dir),
        "build_flags": build_flags(build_dir),
        "repeats": args.repeats,
        "warmup": args.warmup,
        "objectives": [o[0] for o in OBJECTIVES],
        "candidates": {},
        "verification": {},
        "measurements": {},
        "pareto": {},
        "notes": [],
    }

    # ---- gate 1: compile -----------------------------------------------
    runnable = []
    for c in candidates:
        binary = build_dir / c["binary"]
        archive.add_candidate(run_id, c, c["_dir"].relative_to(ROOT))
        entry = {
            "track": c.get("track"),
            "island": c.get("island", -1),
            "origin": c.get("origin"),
            "parents": c.get("parents", []),
            "novelty_status": c.get("novelty_status"),
            "expect_verify": c.get("expect_verify", "pass"),
            "complexity": c.get("complexity"),
            "compiled": binary.exists(),
        }
        results["candidates"][c["name"]] = entry
        if binary.exists():
            runnable.append((c, binary))
        else:
            print(f"[compile] {c['name']}: FAILED (no binary at {binary})")

    # ---- gate 2: correctness -------------------------------------------
    verified = []
    for c, binary in runnable:
        expected = c.get("expect_verify", "pass")
        per_workload = {}
        all_agree = True
        my_workloads = workloads_for(c, workloads)
        for w in my_workloads:
            r = run_binary(binary, w["path"], "verify", 0, 0, args.timeout)
            archive.add_verification(run_id, c["name"], w["name"], r, expected)
            status = r.get("status", "error")
            per_workload[w["name"]] = {
                "status": status,
                "failure": r.get("failure", ""),
                "checksum": str(r.get("checksum", "")),
                "ops_checked": r.get("ops_checked", 0),
            }
            marker = ""
            if expected == "pass" and status != "passed":
                marker = "  <-- REJECTED"
            print(f"[verify] {c['name']:16s} {w['name']:22s} {status}{marker}")

        passed_all = all(v["status"] == "passed" for v in per_workload.values())
        failed_any = any(v["status"] != "passed" for v in per_workload.values())
        # A candidate claiming `expect_verify: fail` has to be rejected by at
        # least one workload. Passing the rest is not a disagreement: a workload
        # that never destroys an entity cannot expose a handle-recycling bug.
        all_agree = passed_all if expected == "pass" else failed_any
        for name_w, v in per_workload.items():
            v["agreed_with_expectation"] = (
                (v["status"] == "passed") if expected == "pass" else all_agree
            )
        results["verification"][c["name"]] = {
            "expected": expected,
            "workloads": per_workload,
            "all_agree_with_expectation": all_agree,
        }
        if not all_agree:
            results["notes"].append(
                f"{c['name']} did not behave as its manifest claims "
                f"(expect_verify: {expected})."
            )
        if expected == "pass" and passed_all:
            verified.append((c, binary))
        elif expected == "fail":
            results["notes"].append(
                f"{c['name']} is a negative control; it is excluded from measurement."
            )

    # Cross-candidate agreement: every verified candidate must produce the same
    # observation checksum on the same workload. A disagreement between two
    # candidates that both passed would mean the oracle comparison has a hole.
    for w in workloads:
        seen = {}
        for c, _ in verified:
            entry = results["verification"][c["name"]]["workloads"].get(w["name"])
            if entry is None:
                continue
            seen.setdefault(entry["checksum"], []).append(c["name"])
        if len(seen) > 1:
            results["notes"].append(
                f"checksum disagreement on {w['name']}: " + json.dumps(seen)
            )

    # ---- gate 3: measurement -------------------------------------------
    # The oracle goes first so that the benchmark-mode checksum every other
    # candidate is compared against is the oracle's, not whichever candidate
    # happened to be measured first.
    verified.sort(key=lambda cb: 0 if cb[0].get("origin") == "oracle" else 1)
    anchor_checksums = {}
    anchor_name = verified[0][0]["name"] if verified else ""
    results["checksum_anchor"] = anchor_name
    for c, binary in verified:
        results["measurements"][c["name"]] = {}
        for w in workloads_for(c, workloads):
            m = run_binary(binary, w["path"], "bench", args.repeats, args.warmup, args.timeout)
            if m.get("status") != "ok":
                print(f"[bench]  {c['name']:16s} {w['name']:22s} {m.get('status')}")
                results["measurements"][c["name"]][w["name"]] = m
                continue
            key = w["name"]
            expected_ck = anchor_checksums.setdefault(key, m["checksum"])
            matches = m["checksum"] == expected_ck
            archive.add_measurement(run_id, c["name"], key, m, matches)
            m["checksum_matches_anchor"] = matches
            results["measurements"][c["name"]][key] = m
            print(
                f"[bench]  {c['name']:16s} {key:22s} "
                f"p50={m['step_ns_p50']/1000:9.1f}us  p99={m['step_ns_p99']/1000:9.1f}us  "
                f"peak={m['peak_bytes']/1048576:7.2f}MB"
                + ("" if matches else "  <-- CHECKSUM MISMATCH")
            )
            if not matches:
                results["notes"].append(
                    f"{c['name']} produced a different observation checksum than "
                    f"{anchor_name} on {key}; its numbers are not comparable."
                )

    # ---- Pareto fronts --------------------------------------------------
    for w in workloads:
        points = {}
        for name, per_w in results["measurements"].items():
            m = per_w.get(w["name"])
            if not m or m.get("status") != "ok":
                continue
            if not m.get("checksum_matches_anchor", True):
                continue
            points[name] = tuple(m[k] for k, _ in OBJECTIVES)
        if not points:
            continue
        f = pareto.front(points)
        results["pareto"][w["name"]] = {
            "front": f,
            "points": {k: list(v) for k, v in points.items()},
        }
        for name in f:
            archive.add_pareto(run_id, "workload", w["name"], name, list(points[name]))

    # Overfitting check: a candidate whose standing on the held-out workloads is
    # worse than on the public ones was tuned, knowingly or not, to what it could
    # see.
    gap = {}
    for name, per_w in results["measurements"].items():
        track = results["candidates"][name]["track"]
        public = [w["name"] for w in workloads
                  if w["visibility"] == "public" and w["track"] == track]
        hidden = [w["name"] for w in workloads
                  if w["visibility"] == "hidden" and w["track"] == track]
        def mean_rank(names):
            rs = []
            for wn in names:
                pts = {
                    n: tuple(mm[k] for k, _ in OBJECTIVES)
                    for n, pw in results["measurements"].items()
                    for mm in [pw.get(wn)]
                    if mm and mm.get("status") == "ok"
                }
                if name not in pts:
                    continue
                rs.append(pareto.ranks(pts, 1)[name])  # rank on p99
            return sum(rs) / len(rs) if rs else None

        pr, hr = mean_rank(public), mean_rank(hidden)
        if pr is not None and hr is not None:
            gap[name] = {
                "track": track,
                "mean_p99_rank_public": round(pr, 2),
                "mean_p99_rank_hidden": round(hr, 2),
                "generalization_gap": round(hr - pr, 2),
            }
    results["generalization"] = gap

    archive.close()

    Path(args.results).parent.mkdir(parents=True, exist_ok=True)
    Path(args.results).write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")

    from report import write_report  # noqa: E402
    write_report(results, workloads, Path(args.report))
    print(f"\narchive: {args.archive}\nresults: {args.results}\nreport:  {args.report}")


if __name__ == "__main__":
    main()
