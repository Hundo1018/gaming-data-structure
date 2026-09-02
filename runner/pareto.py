"""Multi-objective selection.

A single weighted score would decide in advance which trade-off matters. The
archive keeps every candidate that nothing else beats on all objectives at
once, and lets the trade-off stay visible.
"""


def dominates(a, b):
    """True when a is at least as good as b everywhere and strictly better somewhere.

    All objectives are minimised.
    """
    not_worse = all(x <= y for x, y in zip(a, b))
    strictly_better = any(x < y for x, y in zip(a, b))
    return not_worse and strictly_better


def front(points):
    """points: {name: (objective, ...)} -> sorted list of non-dominated names."""
    names = list(points)
    keep = []
    for n in names:
        if not any(dominates(points[m], points[n]) for m in names if m != n):
            keep.append(n)
    return sorted(keep)


def ranks(points, index):
    """Rank names by one objective, best (lowest) first. Returns {name: rank}."""
    order = sorted(points, key=lambda n: points[n][index])
    return {n: i for i, n in enumerate(order)}
