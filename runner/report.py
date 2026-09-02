"""Turns a results file into something a person can read and argue with."""

from pathlib import Path


def us(ns):
    return f"{ns / 1000.0:.1f}"


def mb(b):
    return f"{b / 1048576.0:.2f}"


def write_report(results, workloads, path):
    L = []
    a = L.append

    a("# Benchmark report")
    a("")
    a(f"- run: `{results['run_id']}`")
    a(f"- commit: `{results['git_commit']}`")
    a(f"- cpu: {results['cpu']}")
    a(f"- compiler: {results['compiler']}")
    a(f"- build flags: `{results['build_flags']}`")
    a(f"- repetitions per measurement: {results['repeats']} (plus {results['warmup']} warmup), "
      "the median repetition is reported")
    a("")

    pmu_seen = False
    pmu_reason = ""
    for per_w in results["measurements"].values():
        for m in per_w.values():
            if m.get("status") == "ok":
                pmu_seen = pmu_seen or bool(m.get("pmu_available"))
                pmu_reason = pmu_reason or m.get("pmu_unavailable_reason", "")
    if not pmu_seen:
        a("> Hardware counters (cycles, instructions, cache references and misses, branch "
          "instructions and misses) are **not** in this report. `perf_event_open` failed on "
          f"this machine: `{pmu_reason}`. No counter has been estimated or modelled; the "
          "fields are simply absent.")
        a("")

    a("## Correctness")
    a("")
    a("Every candidate replays the identical op stream beside the oracle. A candidate is "
      "measured only after it passes every workload.")
    a("")
    a("| candidate | expectation | result | note |")
    a("|---|---|---|---|")
    for name, v in sorted(results["verification"].items()):
        statuses = {w: d["status"] for w, d in v["workloads"].items()}
        failed = sorted(w for w, s in statuses.items() if s != "passed")
        if not failed:
            outcome = f"passed all {len(statuses)}"
        else:
            outcome = f"failed {len(failed)} of {len(statuses)}"
        note = ""
        if v["expected"] == "fail":
            passed = sorted(w for w, s in statuses.items() if s == "passed")
            if failed:
                note = f"negative control: rejected by {len(failed)} of {len(statuses)} workloads"
                if passed:
                    note += "; passed " + ", ".join(f"`{p}`" for p in passed)
            else:
                note = "**the gate did not reject it**"
        elif failed:
            first = v["workloads"][failed[0]]["failure"]
            note = f"`{first[:110]}`"
        a(f"| `{name}` | {v['expected']} | {outcome} | {note} |")
    a("")

    ckmsg = [n for n in results.get("notes", []) if "checksum disagreement" in n]
    if ckmsg:
        a("Checksum disagreement between candidates that both passed verification:")
        for n in ckmsg:
            a(f"- {n}")
    else:
        a("All verified candidates produced identical observation checksums on every "
          "workload, so they are answering the same questions the same way.")
    a("")

    a("## Measurements")
    a("")
    a("Timing is per frame. Frame 0 carries the initial population load, which is why the "
      "`max` column sits far above `p99` on the larger workloads: that column is almost "
      "always the load frame, not steady state.")
    a("")
    a("`bytes/entity` is the allocated footprint standing at the end of the run divided by "
      "the live population at the end of the run. `peak` is the high-water mark of live "
      "allocated bytes during the run, which on a bursty workload occurs at a different "
      "moment and a different population.")
    a("")
    for w in workloads:
        name = w["name"]
        rows = []
        for cand, per_w in results["measurements"].items():
            m = per_w.get(name)
            if m and m.get("status") == "ok":
                rows.append((cand, m))
        if not rows:
            continue
        rows.sort(key=lambda r: r[1]["frame_ns_p50"])
        front = set(results.get("pareto", {}).get(name, {}).get("front", []))
        a(f"### `{name}` ({w['visibility']})")
        a("")
        note = w["spec"].get("note")
        if note:
            a(f"{note}")
            a("")
        a(f"entities {w['spec'].get('initial_entities')} initial / "
          f"{w['spec'].get('max_entities')} cap, {w['spec'].get('frames')} frames, "
          f"{w['spec'].get('ops_per_frame')} ops per frame, "
          f"access {w['spec'].get('access', 'uniform')}")
        a("")
        a("| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | "
          "allocs | Pareto |")
        a("|---|---:|---:|---:|---:|---:|---:|---:|:--:|")
        for cand, m in rows:
            a(
                f"| `{cand}` | {us(m['frame_ns_p50'])} | {us(m['frame_ns_p95'])} | "
                f"{us(m['frame_ns_p99'])} | {us(m['frame_ns_max'])} | {mb(m['peak_bytes'])} | "
                f"{m['bytes_per_entity']:.1f} | {m['alloc_count']} | "
                f"{'yes' if cand in front else ''} |"
            )
        a("")

    a("## Pareto fronts")
    a("")
    a("Objectives, all minimised: " + ", ".join(results["objectives"]) + ".")
    a("")
    a("| workload | non-dominated |")
    a("|---|---|")
    for w in workloads:
        p = results.get("pareto", {}).get(w["name"])
        if p:
            a(f"| `{w['name']}` | " + ", ".join(f"`{c}`" for c in p["front"]) + " |")
    a("")

    a("## Public versus held-out standing")
    a("")
    a("Mean rank by p99 frame time, 0 is best. A positive gap means the candidate ranks "
      "worse on workloads it was not designed against.")
    a("")
    a("| candidate | public | hidden | gap |")
    a("|---|---:|---:|---:|")
    for name, g in sorted(results.get("generalization", {}).items(),
                          key=lambda kv: kv[1]["generalization_gap"]):
        a(f"| `{name}` | {g['mean_p99_rank_public']} | {g['mean_p99_rank_hidden']} | "
          f"{g['generalization_gap']:+.2f} |")
    a("")

    other = [n for n in results.get("notes", []) if "checksum disagreement" not in n]
    if other:
        a("## Notes")
        a("")
        for n in other:
            a(f"- {n}")
        a("")

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(L) + "\n")
