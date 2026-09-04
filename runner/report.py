"""Turns a results file into something a person can read and argue with."""

from pathlib import Path


def us(ns):
    return f"{ns / 1000.0:.1f}"


def mb(b):
    return f"{b / 1048576.0:.2f}"


def write_report(results, workloads, path):  # noqa: C901
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
    a("Compare within this report, not against an earlier one. Every number here was "
      "taken on one machine in one sitting under one set of flags, and the machine is "
      "shared. Absolute times move between runs; the orderings and the ratios inside a "
      "single workload are what the run establishes.")
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
    a("Timing is per step, where a step is a frame in the ECS track and a tick in the "
      "spatial track. Step 0 carries the initial population load, which is why the `max` "
      "column sits far above `p99` on the larger workloads: that column is almost always "
      "the load step, not steady state.")
    a("")
    a("`bytes/entity` is the allocated footprint standing at the end of the run divided by "
      "the live population at the end of the run. `peak` is the high-water mark of live "
      "allocated bytes during the run, which on a bursty workload occurs at a different "
      "moment and a different population.")
    a("")
    seen_tracks = set()
    for w in workloads:
        name = w["name"]
        rows = []
        for cand, per_w in results["measurements"].items():
            m = per_w.get(name)
            if m and m.get("status") == "ok":
                rows.append((cand, m))
        if not rows:
            continue
        rows.sort(key=lambda r: r[1]["step_ns_p50"])
        front = set(results.get("pareto", {}).get(name, {}).get("front", []))
        if w["track"] not in seen_tracks:
            seen_tracks.add(w["track"])
            a(f"### Track: {w['track']}")
            a("")
        a(f"#### `{name}` ({w['visibility']})")
        a("")
        note = w["spec"].get("note")
        if note:
            a(f"{note}")
            a("")
        sp = w["spec"]
        if w["track"] == "spatial":
            bits = [f"{sp.get('initial_entities')} entities",
                    f"world {sp.get('world_size')}x{sp.get('world_size')}x"
                    f"{sp.get('world_height')}",
                    f"{sp.get('ticks')} ticks",
                    f"{sp.get('move_fraction', '1.0')} of them moving per tick at speed "
                    f"{sp.get('speed_min', 0)}-{sp.get('speed_max', 0)}",
                    f"query radius {sp.get('query_radius_min')}-{sp.get('query_radius_max')}",
                    f"placement {sp.get('placement', 'uniform')}"]
            if float(sp.get("teleport_ratio", 0) or 0) > 0:
                bits.append(f"teleport ratio {sp['teleport_ratio']}")
            if int(sp.get("rewind_every", 0) or 0) > 0:
                bits.append(f"rewind every {sp['rewind_every']} ticks by "
                            f"{sp.get('rewind_depth')}")
            a(", ".join(bits))
        else:
            a(f"entities {sp.get('initial_entities')} initial / "
              f"{sp.get('max_entities')} cap, {sp.get('frames')} frames, "
              f"{sp.get('ops_per_frame')} ops per frame, "
              f"access {sp.get('access', 'uniform')}")
        a("")
        unit = rows[0][1].get("step_label", "step")
        temporal = any(r[1].get("rewind_strategy", "none") not in ("none", None)
                       for r in rows)
        head = f"| candidate | {unit} p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs |"
        rule = "|---|---:|---:|---:|---:|---:|---:|---:|"
        if temporal:
            head += " rewind |"
            rule += "---|"
        a(head + " Pareto |")
        a(rule + ":--:|")
        for cand, m in rows:
            line = (
                f"| `{cand}` | {us(m['step_ns_p50'])} | {us(m['step_ns_p95'])} | "
                f"{us(m['step_ns_p99'])} | {us(m['step_ns_max'])} | {mb(m['peak_bytes'])} | "
                f"{m['bytes_per_entity']:.1f} | {m['alloc_count']} |"
            )
            if temporal:
                line += f" {m.get('rewind_strategy', '-')} |"
            a(line + f" {'yes' if cand in front else ''} |")
        a("")

    a("## Pareto fronts")
    a("")
    a("Objectives, all minimised: " + ", ".join(results["objectives"]) + ".")
    a("")
    a("| track | workload | non-dominated |")
    a("|---|---|---|")
    for w in workloads:
        p = results.get("pareto", {}).get(w["name"])
        if p:
            a(f"| {w['track']} | `{w['name']}` | " +
              ", ".join(f"`{c}`" for c in p["front"]) + " |")
    a("")

    a("## Public versus held-out standing")
    a("")
    a("Mean rank by p99 step time, 0 is best. A positive gap means the candidate ranks "
      "worse on workloads it was not designed against.")
    a("")
    a("Ranks are computed within a track: the two tracks ask different questions "
      "of different structures and a rank across both would mean nothing.")
    a("")
    a("| track | candidate | public | hidden | gap |")
    a("|---|---|---:|---:|---:|")
    for name, g in sorted(results.get("generalization", {}).items(),
                          key=lambda kv: (kv[1].get("track", ""),
                                          kv[1]["generalization_gap"])):
        a(f"| {g.get('track', '')} | `{name}` | {g['mean_p99_rank_public']} | "
          f"{g['mean_p99_rank_hidden']} | {g['generalization_gap']:+.2f} |")
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
