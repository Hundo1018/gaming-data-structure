#!/usr/bin/env python3
"""Compare two judgments of the same predictions, and a forecast of the second.

`runner/predictions.py` judges every manifest's predictions against one results
file. Run it on an original measurement and on a replication, and this says,
for each prediction listed in a forecast file (or every one judged in both):
the verdict and interval status in each run, whether the verdict replicated,
and whether the forecast of the replication's interval status came true.

    python3 runner/replication_compare.py benchmarks/predictions.json \\
        benchmarks/noise/replication_predictions.json \\
        --forecast benchmarks/noise/replication_prediction.json \\
        --md /tmp/replication.md
"""

import argparse
import json
from pathlib import Path


def index(path):
    run = json.loads(Path(path).read_text())
    return {(c, p["id"]): p for c, ps in run["candidates"].items() for p in ps}


def closest(p):
    """The deciding point: value, interval and bound, for the report."""
    want = "held" if p["verdict"] == "HELD" else "falsified"
    pts = [i for i in p["instances"] if i.get("status") == want]
    if not pts:
        return ""
    # For HELD the weakest point is the one whose interval crosses, if any.
    pts.sort(key=lambda i: (i.get("ci_resolved", True), i["point"]))
    i = pts[0]
    ci = f" [{i['ci95'][0]}, {i['ci95'][1]}]" if "ci95" in i else ""
    return f"{i['point']} {i['value']}{ci} ({i['bound']})"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("original")
    ap.add_argument("replication")
    ap.add_argument("--forecast", default="")
    ap.add_argument("--md", default="")
    args = ap.parse_args()

    a, b = index(args.original), index(args.replication)
    forecast = {}
    if args.forecast:
        for f in json.loads(Path(args.forecast).read_text()):
            forecast[(f["candidate"], f["id"])] = f["predicted"]
    keys = sorted(forecast) if forecast else sorted(set(a) & set(b))

    rows = []
    for k in keys:
        pa, pb = a.get(k), b.get(k)
        if pa is None or pb is None:
            rows.append((k, pa, pb, None, None))
            continue
        replicated = pa["verdict"] == pb["verdict"]
        fc = forecast.get(k)
        hit = None
        if fc:
            resolved = pb.get("ci") == "resolved"
            hit = resolved == (fc == "resolves")
        rows.append((k, pa, pb, replicated, hit))

    lines = ["| prediction | original (5 reps) | replication (30 reps) | replicated | forecast | forecast came true | deciding point in the replication |",
             "|---|---|---|---|---|---|---|"]
    for k, pa, pb, rep, hit in rows:
        if pa is None or pb is None:
            lines.append(f"| `{k[0]}` {k[1]} | {'—' if pa is None else pa['verdict']} | "
                         f"{'—' if pb is None else pb['verdict']} | | | | not judged in both |")
            continue
        lines.append(
            f"| `{k[0]}` {k[1]} | {pa['verdict']}, {pa.get('ci', '')} | {pb['verdict']}, "
            f"{pb.get('ci', '')} | {'yes' if rep else '**no**'} | {forecast.get(k, '')} | "
            f"{'' if hit is None else ('yes' if hit else 'no')} | {closest(pb)} |")
    done = [r for r in rows if r[3] is not None]
    summary = (f"{sum(r[3] for r in done)} of {len(done)} verdicts replicated; "
               f"{sum(1 for r in done if r[2].get('ci') == 'resolved')} resolved in the replication")
    if forecast:
        summary += (f"; the forecast of the interval status came true for "
                    f"{sum(1 for r in done if r[4])} of {len(done)}")
    print("\n".join(lines))
    print(summary + ".")
    if args.md:
        Path(args.md).write_text("\n".join(lines) + "\n\n" + summary + ".\n")


if __name__ == "__main__":
    main()
