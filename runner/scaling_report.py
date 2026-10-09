"""Renders the scaling results, and puts each measured exponent next to the claim.

A parameter sweep has no exponent and no claim. Its section gives each objective
at each value and, for every pair of candidates, the two measured values between
which their order changes.
"""

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
    if results["sweeps"]:
        a(f"- populations: {', '.join(str(s) for s in results['sizes'])} "
          f"(base {results['base_size']})")
    a(f"- {results['repeats']} repetitions per point plus {results['warmup']} warmup, "
      "median repetition")
    a("")
    # A run of parameter sweeps alone has no population curve, and the
    # population sections would be headings over nothing.
    if results["sweeps"]:
        _write_population_families(results, a)
    _write_vary_families(results, a)

    notes = results.get("notes", [])
    if notes:
        a("## Notes")
        a("")
        for n in notes:
            a(f"- {n}")
        a("")

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(L) + "\n")


def _write_population_families(results, a):
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
            # Joined line by line rather than by replacing an indented newline:
            # from Python 3.13 the compiler strips a docstring's indentation, and
            # the paragraph would otherwise be printed with its line breaks.
            para = _write_move_correction.__doc__.split("\n\n", 1)[1]
            a(" ".join(line.strip() for line in para.splitlines()).strip())
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


# The three objectives of the Pareto front, which are what a choice between two
# candidates is made on. Each gets its own table and its own crossings, because
# a pair can change order on one and not on another.
_VARY_METRICS = [
    ("p50", "step_ns_p50", "Median tick, microseconds", 1000.0, "{:.1f}"),
    ("p99", "step_ns_p99", "p99 tick, microseconds", 1000.0, "{:.1f}"),
    ("peak bytes", "peak_bytes", "Peak allocated bytes, MB", 1048576.0, "{:.2f}"),
]


def crossings(values, a_by, b_by):
    """Where two curves change order, as pairs of measured values around it.

    Only values at which both were measured and differ count as ends. Nothing is
    interpolated: the pair says the order changed somewhere between two values
    that were measured, and no measurement says where in between. A value
    inside the pair is one at which the two were equal or one was not
    measured; it is returned with the pair, tagged, so the report can name it
    rather than present the interval as two adjacent values.
    """
    out = []
    prev = None  # (index, value, a above b)
    for i, v in enumerate(values):
        if v not in a_by or v not in b_by or a_by[v] == b_by[v]:
            continue
        above = a_by[v] > b_by[v]
        if prev is not None and above != prev[2]:
            inside = [(u, "equal" if u in a_by and u in b_by else "not measured")
                      for u in values[prev[0] + 1:i]]
            out.append((prev[1], v, inside))
        prev = (i, v, above)
    return out


def _interval(lo, hi, inside):
    text = f"{lo} and {hi}"
    equal = [str(u) for u, why in inside if why == "equal"]
    missing = [str(u) for u, why in inside if why != "equal"]
    notes = []
    if equal:
        notes.append(f"equal at {', '.join(equal)}")
    if missing:
        notes.append(f"{', '.join(missing)} not measured for both")
    return text + (f" ({'; '.join(notes)})" if notes else "")


_RANK_WORDS = ["costliest", "second costliest", "third costliest", "fourth costliest",
               "fifth costliest", "sixth costliest", "seventh costliest",
               "eighth costliest", "ninth costliest", "tenth costliest"]


def _rank_word(rank):
    if 1 <= rank <= len(_RANK_WORDS):
        return _RANK_WORDS[rank - 1]
    suffix = "th" if 10 <= rank % 100 <= 20 else {1: "st", 2: "nd", 3: "rd"}.get(rank % 10, "th")
    return f"{rank}{suffix} costliest"


def p99_rank(ticks):
    """Which tick, counted from the costliest, the harness's p99 is.

    substrate/include/gds/measure.hpp interpolates the sorted tick times at
    0.99 * (ticks - 1). Returns the rank from the top of the tick that carries
    most of the weight, and the rank of the other one when the weight is split
    closely enough that neither describes it.
    """
    idx = 0.99 * (ticks - 1)
    lo = int(idx)
    frac = idx - lo
    if frac < 0.1:
        return ticks - lo, None
    if frac > 0.9:
        return ticks - lo - 1, None
    return ticks - lo, ticks - lo - 1


def _p99_sentence(key, values, rows, fixed, rewinding):
    ticks = {}
    for _, row in rows:
        for v, t in zip(row["value"], row.get("ticks") or []):
            if t:
                ticks.setdefault(v, t)
    if not ticks:
        fixed_ticks = dict(fixed).get("ticks")
        if key == "ticks":
            ticks = {v: int(v) for v in values}
        elif fixed_ticks:
            ticks = {v: int(fixed_ticks) for v in values}
    if not ticks:
        return None
    phrases = []
    for t in sorted(set(ticks.values())):
        rank, other = p99_rank(t)
        where = (_rank_word(rank) if other is None
                 else f"between the {_rank_word(rank)} and the {_rank_word(other)}")
        phrases.append(f"the {where} of {t}")
    text = (f"The p99 column is the harness's 99th percentile over every tick of the "
            f"median repetition: {', '.join(phrases)}. Tick 0, which inserts the whole "
            "population, is one of them.")
    if rewinding:
        text += (" So is every rewind, and a p99 value is a rewind tick only where that "
                 "candidate's rewinds cost more than its ordinary ticks. The load tick "
                 "and any tick slowed by the machine rank among the costliest too, and "
                 "each one that ranks above the rewinds moves the p99 one place down "
                 "them, or off them.")
    return text


def _strategies(row):
    return "/".join(sorted({s for s in row.get("rewind_strategy", []) if s})) or "-"


def _write_vary_families(results, a):
    """One section per parameter sweep: each objective per value, and where the
    order of two candidates changes."""
    curves = results.get("curves") or {}
    if not curves:
        return
    a("## Parameter sweeps")
    a("")
    a("A parameter sweep holds the population and the world fixed and gives one "
      "workload key each of a list of values. Nothing is fitted: the axis is not a "
      "size, and no complexity claim is about it. What it answers is which "
      "candidate is cheaper at each value, and between which two values that "
      "changes.")
    a("")
    a("A change of order is stated as the interval between the nearest values on "
      "either side of it at which both candidates were measured and differ, and "
      "never as a point inside that interval: nothing between them was run. A "
      "value inside the interval at which the two were equal, or at which one was "
      "not measured, is named beside it. The last column is the first-named "
      "candidate's value over the second's at the two ends: below 1 at the smaller "
      "value, above 1 at the larger, and how far from 1 says how decisive the "
      "change is. A change whose ratios are both within a few per cent of 1 is "
      "within the noise of a shared machine, and a pair that changes order more "
      "than once on one objective is not separated by this sweep at all.")
    a("")
    a("A p99 change of order is a change in the tail of the tick times, and the "
      "p99 is taken over every tick, the load tick included. It is a change in the "
      "cost of a rewind only where both candidates' rewinds are costlier than "
      "their ordinary ticks, which these tables cannot show. Each family's section "
      "says which tick of its run the p99 is.")
    a("")
    a("`history` is the rewind strategy the binary reported: `native` for a "
      "candidate that keeps its own, `snapshot_rebuild` for one measured inside the "
      "substrate's snapshot-and-rebuild wrapper, `none` on a workload that never "
      "rewinds.")
    a("")
    # Sorted, because the results file is written with sorted keys and a report
    # rendered from it should read the same as one rendered as the run ends.
    for fam_id, curve in sorted(curves.items()):
        fam = results["families"][fam_id]
        key, values = curve["key"], curve["values"]
        rows = sorted(curve["candidates"].items())
        a(f"### `{fam_id}` · varies `{key}`")
        a("")
        a(fam["measures"])
        a("")
        fixed = fam.get("fixed") or []
        if fixed:
            a(f"Held fixed by `{fam['template']}`: "
              + ", ".join(f"`{k}` {v}" for k, v in fixed) + ".")
            a("")
        if not rows:
            a("_no candidate produced a usable curve_")
            a("")
            continue
        rewinds = {}
        for _, row in rows:
            for v, n in zip(row["value"], row.get("rewinds", [])):
                rewinds.setdefault(v, n)
        if len(set(rewinds.values())) == 1:
            counted = f"{next(iter(rewinds.values()))} at every value"
        else:
            counted = ", ".join(f"{v}: {rewinds.get(v, '-')}" for v in values)
        a(f"Every candidate here passed verification against the oracle at every "
          f"value of `{key}` before any was measured. Rewinds scheduled in each run: "
          f"{counted}.")
        a("")
        p99_note = _p99_sentence(key, values, rows, fixed,
                                 any(n for n in rewinds.values() if n))
        if p99_note:
            a(p99_note)
            a("")

        for _, field, title, scale, fmt in _VARY_METRICS:
            a(f"{title}, by `{key}`:")
            a("")
            a("| candidate | history | " + " | ".join(str(v) for v in values) + " |")
            a("|---|---|" + "---:|" * len(values))
            for name, row in rows:
                by_value = dict(zip(row["value"], row[field]))
                cells = " | ".join(fmt.format(by_value[v] / scale) if v in by_value
                                   else "-" for v in values)
                a(f"| `{name}` | {_strategies(row)} | {cells} |")
            a("")

        a(f"Where two candidates change order along `{key}`:")
        a("")
        if len(rows) < 2:
            a(f"- Only `{rows[0][0]}` was measured on this family, so there is no "
              "order to compare.")
            a("")
            continue
        found, repeated, quiet = [], [], []
        for label, field, _, _, _ in _VARY_METRICS:
            any_here = False
            for i, (na, ra) in enumerate(rows):
                for nb, rb in rows[i + 1:]:
                    a_by = dict(zip(ra["value"], ra[field]))
                    b_by = dict(zip(rb["value"], rb[field]))
                    pairs = crossings(values, a_by, b_by)
                    for lo, hi, inside in pairs:
                        first, second = (na, nb) if a_by[lo] < b_by[lo] else (nb, na)
                        by = {na: a_by, nb: b_by}
                        found.append((label, first, second, _interval(lo, hi, inside),
                                      by[first][lo] / by[second][lo],
                                      by[first][hi] / by[second][hi]))
                    if len(pairs) > 1:
                        repeated.append((label, na, nb, len(pairs)))
                    any_here = any_here or bool(pairs)
            if not any_here:
                quiet.append(label)

        if found:
            a("| objective | lower at the smaller value | lower at the larger value | "
              "order changes between | first / second, at each end |")
            a("|---|---|---|---|---:|")
            for label, first, second, between, r_lo, r_hi in found:
                a(f"| {label} | `{first}` | `{second}` | {between} | "
                  f"{r_lo:.2f}, {r_hi:.2f} |")
            a("")
        for label, na, nb, n in repeated:
            a(f"- `{na}` and `{nb}` change order {n} times on {label}: over part of this "
              "range they are within noise of each other, and no one crossing should be "
              "read from it.")
        if quiet:
            a(f"- No two candidates change order on {' or '.join(quiet)} across this "
              "range.")
        if repeated or quiet:
            a("")
