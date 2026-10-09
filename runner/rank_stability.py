#!/usr/bin/env python3
"""Do two builds rank the candidates the same way?

`runner/build_ab.py` gives, for every binary and workload, per-round
measurements from two builds. A ratio between the builds says how much one
candidate moved; what a comparative study cares about is whether the
candidates' order moved. This reads a build_ab JSON and, for each workload,
ranks the candidates by their median in each build, reports Kendall's tau-b
between the two orders, and lists every pair whose order differs between the
builds, with the margin by which each build separates them.

A flipped pair is reported as *robust* when, in both builds, the two
candidates' per-round ranges do not overlap: each build is consistent with
itself about the order, and the two builds disagree. A flip inside overlapping
ranges is a tie that either build could have broken either way.

    python3 runner/rank_stability.py benchmarks/port_timing.json \\
        --exclude gds_ecs_reference gds_spatial_brute_force gds_ecs_broken_recycle \\
        --metric step_ns_p50 --md benchmarks/port_ranking.md
"""

import argparse
import itertools
import json
import statistics
from pathlib import Path


def values(pair, side, metric, primary):
    if metric == primary:
        return [v for v in pair[side] if v]
    return [v for v in pair.get("also", {}).get(metric, {}).get(side, []) if v]


def kendall_tau_b(xs, ys):
    """tau-b between two score lists over the same items (ties allowed)."""
    conc = disc = tx = ty = 0
    for i, j in itertools.combinations(range(len(xs)), 2):
        dx = (xs[i] > xs[j]) - (xs[i] < xs[j])
        dy = (ys[i] > ys[j]) - (ys[i] < ys[j])
        if dx == 0 and dy == 0:
            continue
        if dx == 0:
            tx += 1
        elif dy == 0:
            ty += 1
        elif dx == dy:
            conc += 1
        else:
            disc += 1
    denom = ((conc + disc + tx) * (conc + disc + ty)) ** 0.5
    return (conc - disc) / denom if denom else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_ab_json")
    ap.add_argument("--metric", default="", help="default: the run's primary metric")
    ap.add_argument("--exclude", nargs="*", default=[], help="binaries left out of the ranking")
    ap.add_argument("--a-label", default="a")
    ap.add_argument("--b-label", default="b")
    ap.add_argument("--md", default="")
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    run = json.loads(Path(args.build_ab_json).read_text())
    primary = run["metric"]
    metric = args.metric or primary
    by_workload = {}
    for p in run["pairs"]:
        if p["status"] != "ok" or p["binary"] in args.exclude:
            continue
        va, vb = values(p, "a", metric, primary), values(p, "b", metric, primary)
        if not va or not vb:
            continue
        by_workload.setdefault(p["workload"], []).append(
            {"binary": p["binary"], "a": va, "b": vb,
             "a_med": statistics.median(va), "b_med": statistics.median(vb)})

    def separated(x, y, side):
        return max(x[side]) < min(y[side]) or max(y[side]) < min(x[side])

    out = {"metric": metric, "workloads": {}}
    # How much the test can see: a pair is resolved by a build when that build's
    # per-round ranges for the two candidates do not overlap. Only pairs both
    # builds resolve can show a disagreement that is not a tie.
    resolved = {"a": 0, "b": 0, "both_agree": 0, "both_disagree": 0}
    for w, cands in sorted(by_workload.items()):
        for x, y in itertools.combinations(cands, 2):
            ra, rb = separated(x, y, "a"), separated(x, y, "b")
            resolved["a"] += ra
            resolved["b"] += rb
            if ra and rb:
                same = (x["a_med"] < y["a_med"]) == (x["b_med"] < y["b_med"])
                resolved["both_agree" if same else "both_disagree"] += 1
    for w, cands in sorted(by_workload.items()):
        tau = kendall_tau_b([c["a_med"] for c in cands], [c["b_med"] for c in cands])
        flips = []
        for x, y in itertools.combinations(cands, 2):
            if (x["a_med"] < y["a_med"]) == (x["b_med"] < y["b_med"]):
                continue
            robust = ((max(x["a"]) < min(y["a"]) or max(y["a"]) < min(x["a"])) and
                      (max(x["b"]) < min(y["b"]) or max(y["b"]) < min(x["b"])))
            flips.append({"pair": [x["binary"], y["binary"]],
                          "a_ratio": x["a_med"] / y["a_med"],
                          "b_ratio": x["b_med"] / y["b_med"], "robust": robust})
        out["workloads"][w] = {"n": len(cands), "tau_b": tau, "flips": flips}

    # Run-to-run noise: per candidate and workload, (max - min) / median over
    # the rounds. It bounds the smallest difference any ordering here can show.
    def spread(xs):
        med = statistics.median(xs)
        return (max(xs) - min(xs)) / med if med else 0.0
    noise = {}
    for side in ("a", "b"):
        sp = sorted(spread(c[side]) for cands in by_workload.values() for c in cands)
        noise[side] = {"p25": sp[len(sp) // 4], "median": statistics.median(sp),
                       "p75": sp[3 * len(sp) // 4]} if sp else None
    out["noise"] = noise

    taus = [v["tau_b"] for v in out["workloads"].values() if v["tau_b"] is not None]
    all_flips = [f for v in out["workloads"].values() for f in v["flips"]]
    out["summary"] = {
        "workloads": len(out["workloads"]),
        "median_tau_b": statistics.median(taus) if taus else None,
        "min_tau_b": min(taus) if taus else None,
        "flipped_pairs": len(all_flips),
        "robust_flips": sum(1 for f in all_flips if f["robust"]),
        "pairs_compared": sum(v["n"] * (v["n"] - 1) // 2 for v in out["workloads"].values()),
        "resolved_by_a": resolved["a"],
        "resolved_by_b": resolved["b"],
        "resolved_by_both_agree": resolved["both_agree"],
        "resolved_by_both_disagree": resolved["both_disagree"],
    }
    s = out["summary"]
    print(f"metric {metric}: {s['workloads']} workloads, median tau-b {s['median_tau_b']}, "
          f"min {s['min_tau_b']}; {s['flipped_pairs']} of {s['pairs_compared']} pairs flip, "
          f"{s['robust_flips']} robustly")
    print(f"  resolved (ranges do not overlap): {args.a_label} {s['resolved_by_a']}, "
          f"{args.b_label} {s['resolved_by_b']}; by both: {s['resolved_by_both_agree']} agree, "
          f"{s['resolved_by_both_disagree']} disagree")
    for side, label in (("a", args.a_label), ("b", args.b_label)):
        n = noise[side]
        if n:
            print(f"  per-round spread (max-min)/median, {label}: p25 {n['p25']:.1%}, "
                  f"median {n['median']:.1%}, p75 {n['p75']:.1%}")
    for w, v in out["workloads"].items():
        for f in v["flips"]:
            print(f"  {w:26s} {f['pair'][0]} vs {f['pair'][1]}: {args.a_label} {f['a_ratio']:.3f}, "
                  f"{args.b_label} {f['b_ratio']:.3f}{'  ROBUST' if f['robust'] else ''}")

    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1) + "\n")
    if args.md:
        lines = [f"# Ranking stability: {args.b_label} against {args.a_label}", "",
                 f"Metric `{metric}`, medians over the rounds in `{args.build_ab_json}`. "
                 f"Excluded: {', '.join(args.exclude) or 'none'}. A flip is robust when, in "
                 "both builds, the two candidates' per-round ranges do not overlap.", "",
                 f"Median tau-b {s['median_tau_b']:.3f}, lowest {s['min_tau_b']:.3f}; "
                 f"{s['flipped_pairs']} of {s['pairs_compared']} candidate pairs change order, "
                 f"{s['robust_flips']} of them robustly.", "",
                 f"Resolution, the part of the comparison the noise lets through: "
                 f"{args.a_label} separates {s['resolved_by_a']} pairs and {args.b_label} "
                 f"{s['resolved_by_b']} (per-round ranges that do not overlap); of the pairs "
                 f"both separate, {s['resolved_by_both_agree']} are in the same order and "
                 f"{s['resolved_by_both_disagree']} in opposite orders.", "",
                 "Run-to-run spread, (max - min) / median over the rounds per candidate and "
                 "workload: " + "; ".join(
                     f"{lab} median {noise[sd]['median']:.1%} (p25 {noise[sd]['p25']:.1%}, "
                     f"p75 {noise[sd]['p75']:.1%})"
                     for sd, lab in (("a", args.a_label), ("b", args.b_label)) if noise[sd]) + ".",
                 "",
                 "| workload | candidates | tau-b | flipped pairs (a ratio → b ratio) |",
                 "|---|---:|---:|---|"]
        for w, v in out["workloads"].items():
            fl = "; ".join(f"{f['pair'][0].split('_', 2)[2]} vs {f['pair'][1].split('_', 2)[2]} "
                           f"{f['a_ratio']:.3f} → {f['b_ratio']:.3f}"
                           f"{' (robust)' if f['robust'] else ''}" for f in v["flips"]) or "—"
            tau = f"{v['tau_b']:.3f}" if v["tau_b"] is not None else "—"
            lines.append(f"| {w} | {v['n']} | {tau} | {fl} |")
        Path(args.md).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
