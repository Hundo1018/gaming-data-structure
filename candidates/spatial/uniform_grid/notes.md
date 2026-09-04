# uniform_grid — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## Fastest on nine of ten spatial workloads

The exception is `hs02_rewind_few_moving`, lost to its own child
`grid_undo_log` on the history strategy rather than on the index.

Against scanning everything it is 15.9x faster on the evenly spread baseline
(968.0 us against 15339.9 us).

## The clustering prediction held

`s02_dense_clustered` puts 40000 entities into eight clumps of radius 8 and
aims the queries into them. The margin over scanning everything falls from 15.9x
to 3.1x, and the margin over every other candidate collapses with it:

| | s01 (even) | s02 (clustered) |
|---|---:|---:|
| vs `brute_force` | 15.85x | 3.12x |
| vs `axis_sorted` | 3.25x | 1.57x |
| vs `morton_sorted` | 3.85x | 1.54x |
| vs `spatial_hash` | 1.90x | 1.09x |

It still wins, but on that workload a cell holds hundreds of entities and the
walk is no longer short, so the structure has stopped doing what it exists to do
and is winning on constant factors. This is the workload that states the case for
a hierarchy, and no candidate in this population answers it.

Memory moves the wrong way too: 5.84 MB on `s02` against 1.10 MB on `s01`,
because the cell array is sized by the world while the population sits in a
fraction of it.

## The teleport prediction held, and is now quantified

`s04_teleport` is `s01` with one field changed, so the difference is the
teleports and nothing else:

| candidate | s01 | s04 | change |
|---|---:|---:|---:|
| `uniform_grid` | 968.0 us | 1154.3 us | +19.3% |
| `spatial_hash` | 1837.0 us | 2328.1 us | +26.7% |
| `axis_sorted` | 3142.8 us | 3192.5 us | +1.6% |
| `morton_sorted` | 3731.0 us | 3658.5 us | -1.9% |
| `brute_force` | 15339.9 us | 15246.7 us | -0.6% |

The incremental structures pay for jumps and the rebuild-per-tick ones cannot
tell a jump from a step, exactly as predicted. It changes no ranking, because a
19% penalty does not close a 3.9x lead — the effect is real and it is not
decisive.

## Where it is weakest

`hs03_knn_heavy` at 5723.3 us is its worst absolute showing, because a
nearest-neighbour query with k of 64 widens its box until it can prove it has
the k closest, and each widening step multiplies the cells walked. It is still
first there by 2.7x.
