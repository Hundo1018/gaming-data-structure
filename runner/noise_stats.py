#!/usr/bin/env python3
"""Statistics over a runner/noise_probe.py JSON that decide how to measure.

Two questions, per binary, workload and condition:

1. Is the noise slow or fast? The lag-1 autocorrelation of consecutive
   repetitions inside one invocation, and of consecutive invocations of one
   condition, says whether a slow repetition predicts a slow next one. Slow
   drift is what interleaving two candidates (runner/build_ab.py) cancels;
   independent noise from one repetition to the next it does not cancel, and
   only more repetitions shrink it.
2. How many repetitions does a claim need? From the pooled repetitions, a
   bootstrap gives the 95% interval of the median for n repetitions, as a
   fraction of the median, for several n.

    python3 runner/noise_stats.py /tmp/noise.json --condition plain
"""

import argparse
import json
import random
import statistics
from pathlib import Path


def lag1(xs):
    if len(xs) < 3:
        return None
    m = statistics.fmean(xs)
    num = sum((xs[i] - m) * (xs[i + 1] - m) for i in range(len(xs) - 1))
    den = sum((x - m) ** 2 for x in xs)
    return num / den if den else None


def bootstrap_ci(pool, n, draws, rng, stat):
    """Half-width of the 95% bootstrap interval of stat over n draws, over stat(pool)."""
    vals = sorted(stat(rng.choices(pool, k=n)) for _ in range(draws))
    lo, hi = vals[int(0.025 * draws)], vals[int(0.975 * draws) - 1]
    return (hi - lo) / 2 / stat(pool)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("probe_json")
    ap.add_argument("--condition", default="plain")
    ap.add_argument("--metric", default="total", choices=["total", "p50"])
    ap.add_argument("--n", nargs="+", type=int, default=[1, 5, 15, 45])
    ap.add_argument("--draws", type=int, default=4000)
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    rng = random.Random(1)
    run = json.loads(Path(args.probe_json).read_text())
    rows = []
    for c in run["cells"]:
        if c["condition"] != args.condition:
            continue
        inv = c["raw"]
        within = [lag1(i[args.metric]) for i in inv]
        within = [w for w in within if w is not None]
        meds = [statistics.median(i[args.metric]) for i in inv]
        pool = [x for i in inv for x in i[args.metric]]
        # Repetitions normalised by their own invocation's median: what is left
        # once each invocation's level is taken out.
        resid = [x / statistics.median(i[args.metric]) for i in inv for x in i[args.metric]]
        rows.append({
            "binary": c["binary"], "workload": c["workload"],
            "lag1_within_mean": statistics.fmean(within) if within else None,
            "lag1_between": lag1(meds),
            "cv_pooled": statistics.stdev(pool) / statistics.fmean(pool),
            "cv_within": statistics.stdev(resid),
            "ci95_halfwidth": {n: bootstrap_ci(pool, n, args.draws, rng, statistics.median)
                               for n in args.n},
            # The minimum, for comparison: if the disturbances only ever slow a
            # run down, the fastest of n is the steadier estimate of the
            # undisturbed cost.
            "min_ci95_halfwidth": {n: bootstrap_ci(pool, n, args.draws, rng, min)
                                   for n in args.n},
            "skew": (statistics.fmean(pool) - statistics.median(pool)) / statistics.stdev(pool),
        })
    head = " ".join(f"{'n=' + str(n):>7s}" for n in args.n)
    print(f"{'binary':26s} {'workload':20s} {'lag1 rep':>8s} {'lag1 inv':>8s} "
          f"{'cv all':>7s} {'cv in':>6s} {'skew':>5s}  median CI95 +-: {head}   min CI95 +-: {head}")
    for r in rows:
        ci = " ".join(f"{r['ci95_halfwidth'][n]:7.1%}" for n in args.n)
        cm = " ".join(f"{r['min_ci95_halfwidth'][n]:7.1%}" for n in args.n)
        l1 = f"{r['lag1_within_mean']:8.2f}" if r["lag1_within_mean"] is not None else f"{'-':>8s}"
        l2 = f"{r['lag1_between']:8.2f}" if r["lag1_between"] is not None else f"{'-':>8s}"
        print(f"{r['binary']:26s} {r['workload']:20s} {l1} {l2} {r['cv_pooled']:7.1%} "
              f"{r['cv_within']:6.1%} {r['skew']:5.2f}  {' ' * 16}{ci}   {' ' * 13}{cm}")
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1) + "\n")


if __name__ == "__main__":
    main()
