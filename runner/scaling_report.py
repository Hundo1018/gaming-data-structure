"""Renders the scaling results, and puts each measured exponent next to the claim."""

import math
import re
from pathlib import Path

# Only two shapes of claim can be checked against a number without guessing what
# the author meant. Everything else is put side by side for a person to judge,
# which is the honest outcome for a free-text field.
_CHECKABLE = [
    (re.compile(r"^O\(1\)$", re.I), 0.0, "constant"),
    (re.compile(r"^O\((n|entities)\)$", re.I), 1.0, "linear"),
]
_TOLERANCE = 0.15


def check_claim(claim, exponent):
    """Returns (verdict, expected) or (None, None) when the claim is not checkable."""
    if claim is None or exponent is None:
        return None, None
    text = str(claim).strip()
    for pattern, expected, _ in _CHECKABLE:
        if pattern.match(text):
            ok = abs(exponent - expected) <= _TOLERANCE
            return ("agrees" if ok else "**disagrees**"), expected
    return None, None


def local_slope(sizes, values, back=3):
    """Slope across the last `back` points: how it behaves at the top of the range.

    A single fitted exponent averages the whole range, and a structure whose
    fixed per-call cost matters at the small end fits lower than it
    asymptotically behaves. Three points rather than two, because one noisy
    measurement at the largest population would otherwise set the number on its
    own. The pairwise slopes printed under each table are the honest version:
    they show where a curve bends instead of summarising it away.
    """
    if len(sizes) < back or len(values) < back:
        return None
    x0, x1 = math.log(sizes[-back]), math.log(sizes[-1])
    y0, y1 = math.log(values[-back]), math.log(values[-1])
    if x1 == x0:
        return None
    return round((y1 - y0) / (x1 - x0), 3)


def pairwise_slopes(sizes, values):
    """Slope of each adjacent pair, so a bend in the curve is visible."""
    out = []
    for i in range(1, len(sizes)):
        dx = math.log(sizes[i]) - math.log(sizes[i - 1])
        dy = math.log(values[i]) - math.log(values[i - 1])
        out.append(dy / dx if dx else 0.0)
    return out


def write_scaling_report(results, path):
    L = []
    a = L.append

    a("# Scaling report")
    a("")
    a(f"- run: `{results['run_id']}`")
    a(f"- commit: `{results['git_commit']}`")
    a(f"- cpu: {results['cpu']}")
    a(f"- compiler: {results['compiler']}")
    a(f"- build flags: `{results['build_flags']}`")
    a(f"- populations: {', '.join(str(s) for s in results['sizes'])} "
      f"(base {results['base_size']})")
    a(f"- {results['repeats']} repetitions per point plus {results['warmup']} warmup, "
      "median repetition")
    a("")
    control = ((results["sweeps"].get("spatial_query_radius::fixed_density", {})
                .get("brute_force", {}) or {}).get("time_fit") or {})
    if control:
        a("`brute_force` is the positive control: it looks at every entity for every "
          f"query, so its query exponent must come out at 1. It measures "
          f"{control['exponent']} with an r2 of {control['r_squared']}. An exponent "
          "elsewhere in this report is only worth reading because that one came out "
          "right.")
        a("")
    a("Every exponent below is the least-squares slope of log(cost) against "
      "log(population). A cost that does not depend on the population gives 0; one "
      "that looks at everything gives 1. `r2` is the fit quality: below about 0.9 the "
      "points are not following a single power law over this range and the exponent "
      "should not be quoted.")
    a("")
    a("The number of operations per step is held constant while the population grows. "
      "Without that, every candidate would measure linear regardless of what it does.")
    a("")

    a("## Regimes")
    a("")
    for rid, text in results["regimes"].items():
        a(f"- **{rid}** — {text}")
    a("")

    for key, per_candidate in results["sweeps"].items():
        family, regime = key.split("::")
        fam = results["families"][family]
        a(f"## `{family}` · {regime}")
        a("")
        a(fam["measures"])
        a("")
        rows = sorted(per_candidate.items(),
                      key=lambda kv: (kv[1]["time_fit"] or {}).get("exponent", 99))
        if not rows:
            a("_no candidate produced a usable curve_")
            a("")
            continue
        sizes = results["sizes"]
        a(f"| candidate | time n^ | r2 | top-end n^ | memory n^ | "
          f"step p50 at {sizes[0]} | at {sizes[-1]} | growth |")
        a("|---|---:|---:|---:|---:|---:|---:|---:|")
        for name, row in rows:
            tf = row["time_fit"] or {}
            mf = row["memory_fit"] or {}
            first = row["step_ns_p50"][0] / 1000.0 if row["step_ns_p50"] else 0
            last = row["step_ns_p50"][-1] / 1000.0 if row["step_ns_p50"] else 0
            factor = (last / first) if first > 0 else 0
            top = local_slope(row["size"], row["step_ns_p50"])
            grew = f"{factor:.1f}x" if factor >= 1 else f"/{1 / factor:.1f}"
            a(f"| `{name}` | {tf.get('exponent', '-')} | {tf.get('r_squared', '-')} | "
              f"{top if top is not None else '-'} | {mf.get('exponent', '-')} | "
              f"{first:.1f} us | {last:.1f} us | {grew} |")
        a("")
        a(f"Population grew {sizes[-1] // sizes[0]}x across this table. `top-end n^` is "
          "the slope across the last three points: where it exceeds the fitted "
          "exponent, a fixed per-call cost is flattening the small end and the larger "
          "number is closer to the asymptotic behaviour.")
        a("")
        a("Median step time in microseconds at each population, and the slope of each "
          "adjacent pair beneath it. A candidate whose pairwise slopes drift is not "
          "following one power law, and its fitted exponent is an average over a "
          "changing shape rather than a description of it.")
        a("")
        a("```")
        a("population   " + "".join(f"{n:>10d}" for n in sizes))
        for name, row in rows:
            by_size = dict(zip(row["size"], row["step_ns_p50"]))
            cells = "".join(
                f"{by_size[n] / 1000.0:>10.1f}" if n in by_size else f"{'-':>10s}"
                for n in sizes)
            a(f"{name:<13s}{cells}")
            slopes = pairwise_slopes(row["size"], row["step_ns_p50"])
            scells = "".join(f"{v:>10.2f}" for v in slopes)
            a(f"{'  slope':<13s}{'':>10s}{scells}")
        a("```")
        a("")

    _write_move_correction(results, a)

    a("## Declared complexity against measured growth")
    a("")
    a("The `complexity:` field of each manifest is a claim written by hand. Until this "
      "report existed nothing read it. Most claims are free text describing what the "
      "cost depends on, and those cannot be turned into a number without guessing what "
      "the author meant, so they are placed beside the measurement for a person to "
      "judge. Only `O(1)` and `O(n)` are checked automatically, against a tolerance of "
      f"{_TOLERANCE} in the exponent.")
    a("")
    a("**A disagreement here has two possible causes and the table cannot tell them "
      "apart.** A complexity claim counts operations; the measurement is time. When "
      "they part company it means either that the claim is wrong about the operations, "
      "or that the claim is right and the machine does not behave the way the model "
      "assumes — most often because the working set has outgrown a level of cache, so "
      "a fixed number of memory accesses stops costing a fixed amount of time. Both are "
      "findings. Neither is a reason to edit the claim to match the number.")
    a("")
    a("| candidate | claim | field | measured (family · regime) | verdict |")
    a("|---|---|---|---:|---|")
    for name, claims in sorted(results["declared"].items()):
        if not isinstance(claims, dict):
            continue
        for key, per_candidate in results["sweeps"].items():
            family, regime = key.split("::")
            if regime != "fixed_density" and results["families"][family]["track"] == "spatial":
                continue  # the regime that isolates search cost from answer size
            row = per_candidate.get(name)
            if not row or not row["time_fit"]:
                continue
            field = _field_for_family(family)
            claim = claims.get(field)
            if claim is None:
                continue
            exponent = row["time_fit"]["exponent"]
            verdict, expected = check_claim(claim, exponent)
            if verdict is None:
                verdict = "not machine-checkable"
            else:
                verdict = f"{verdict} (expected {expected})"
            a(f"| `{name}` | `{claim}` | `{field}` | n^{exponent} | {verdict} |")
    a("")

    notes = results.get("notes", [])
    if notes:
        a("## Notes")
        a("")
        for n in notes:
            a(f"- {n}")
        a("")

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(L) + "\n")


def _write_move_correction(results, a):
    """Removes the forcing query from the move family, and says why it was there.

    The move family carries one radius query per tick so that a candidate which
    rebuilds lazily actually does its rebuild. For a candidate whose query is
    itself expensive that query dominates the tick at large populations, and the
    family would report its query cost as its move cost. Subtracting one query's
    worth of the query family's measurement, at the same population and regime,
    separates them. It is an estimate: the two families place entities
    differently, so the per-query cost is close but not identical.
    """
    query_per_tick = 128  # radius_queries_per_tick in spatial_query_radius.template
    moves_per_tick = 2000  # moves_per_tick in spatial_move.template
    wrote_header = False
    for regime in ("fixed_world", "fixed_density"):
        move_key = f"spatial_move::{regime}"
        query_key = f"spatial_query_radius::{regime}"
        if move_key not in results["sweeps"] or query_key not in results["sweeps"]:
            continue
        if not wrote_header:
            a("## Cost of a move, with the forcing query removed")
            a("")
            a(_write_move_correction.__doc__.split("\n\n", 1)[1].replace("\n    ", " ")
              .strip())
            a("")
            wrote_header = True
        a(f"### {regime}")
        a("")
        a("| candidate | move-family n^ | moves-only n^ | ns per move at "
          "smallest | at largest |")
        a("|---|---:|---:|---:|---:|")
        rows = []
        for name, mrow in results["sweeps"][move_key].items():
            qrow = results["sweeps"][query_key].get(name)
            if not qrow:
                continue
            sizes, corrected = [], []
            for i, n in enumerate(mrow["size"]):
                if n not in qrow["size"]:
                    continue
                per_query = qrow["step_ns_p50"][qrow["size"].index(n)] / query_per_tick
                value = mrow["step_ns_p50"][i] - per_query
                if value <= 0:
                    continue
                sizes.append(n)
                corrected.append(value)
            fit = None
            if len(sizes) >= 3:
                from sweep import fit_power_law
                fit = fit_power_law(sizes, corrected)
            raw = (mrow["time_fit"] or {}).get("exponent", "-")
            first = corrected[0] / moves_per_tick if corrected else 0
            last = corrected[-1] / moves_per_tick if corrected else 0
            rows.append((name, raw, fit["exponent"] if fit else "-", first, last))
        for name, raw, corr, first, last in sorted(rows, key=lambda r: r[3]):
            a(f"| `{name}` | {raw} | {corr} | {first:.1f} | {last:.1f} |")
        a("")


def _field_for_family(family):
    """Which line of a manifest's complexity field a family is testing."""
    return {
        "spatial_query_radius": "query_radius",
        "spatial_knn": "query_knn",
        "spatial_move": "insert_remove_move",
        "ecs_point_ops": "get_set",
        "ecs_mixed": "query",
    }.get(family, family)
