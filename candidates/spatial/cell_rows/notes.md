# cell_rows — observed

Run `20261009T042136Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 5 repetitions. Verdicts are
`benchmarks/predictions.md`'s.

## Two of three held

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | no workload slower than `cell_sorted` by over 3% | at most 1.023x (`hs08`) | held |
| P2 | `hs06_mixed_reach` at most 0.9x `cell_sorted` | 0.967x | **falsified** |
| P3 | `hs03_knn_heavy` at most 0.9x `cell_sorted` | 0.715x | held |

These predictions were written after `cell_sorted`'s reviewer had measured the
row walk on public workloads, so they were placed only on held-out ones.

P1 rested on one workload for a while. The harness reports a run's metrics from
the repetition with the median total time, and on `hs07_crowd` that repetition's
own median tick was the highest of five (3444, 3377, 3383, 3487, 3718 us). From
that repetition the candidate looked 1.11x `cell_sorted`; from the median of its
five repetitions it is 1.006x. The judge now uses the latter for every step
percentile, a change that also turned one of `delta_grid`'s predictions from
held to falsified.

## Long rows pay; mixed reach does not make rows long

P3 is where the row walk was expected to matter most, and it did: a k-nearest
search at k = 64 in a sparse world widens its box over many cells of each row,
and reading each row as one run took `hs03` from 5323.7 us to 3832.3 us.

P2 assumed the large queries on `hs06_mixed_reach` would do the same. The cell
edge there is 65, set by the mean reach, so a query of radius 128 spans at most
five cells per row and most queries span one or two. The box is long in cells
only where cells are small against the query, and on `hs06` the cells were sized
to make them comparable.

## Where it stands

On seven Pareto fronts, more than any other spatial candidate except
`grid_ring_knn` and `morton_lbvh`. Its generalization gap is +1.55 ranks: it does
better on the public workloads, mostly because the new held-out crowd and
rollback workloads favour `morton_lbvh` and `delta_grid`, not because of
tuning (no constant was chosen here; the change is the walk alone).
