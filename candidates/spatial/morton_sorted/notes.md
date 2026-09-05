# morton_sorted — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## The central prediction was falsified

It predicted that rebuilding into perfect order would beat the linked-list grid
on query-heavy workloads. It loses to `uniform_grid` on all ten, by 1.54x on
the clustered workload and 3.85x on the even baseline.

Worse, on `hs03_knn_heavy` it is beaten by scanning everything:

| candidate | hs03_knn_heavy |
|---|---:|
| `uniform_grid` | 5723.3 us |
| `brute_force` | 20115.4 us |
| `morton_sorted` | 34043.7 us |

The reason is specific and is the flaw in the design as built, not in the idea.
A nearest-neighbour query widens its box until the kth neighbour is inside it,
and this structure pays one binary search per cell in the box. The cells go up
with the cube of the width, so each doubling multiplies the searches by eight,
and the log-n searches swamp the contiguous scanning they were supposed to buy.
The grid indexes an array instead and pays nothing per cell.

## What did hold

**Immunity to teleports.** On the controlled pair, where `s04` differs from
`s01` in one field, it went from 3731.0 us to 3658.5 us, a change of -1.9%,
against the grid's +19.3%. Throwing the index away every tick really does make a
jump indistinguishable from a step.

**Holding up better under clustering.** The grid's lead over it fell from 3.85x
on the even baseline to 1.54x on the clustered one. Nothing is preallocated per
cell here, so a crowded cell is simply a longer contiguous run.

## Where the family might still be

Two costs are separable and only one was tested. The rebuild is `std::sort` on
a nearly-sorted array once per tick; a radix sort, or one that starts from the
previous tick's order, would cut it. The query path's binary search per cell is
the larger problem and needs a different fix — a cell directory, or walking a
Morton range rather than searching each cell. Neither is implemented, so what is
falsified is this implementation, and the honest reading is that the layout
argument has not been given its best case.

## Scaling (run `sweep-20260904T081902Z`)

Query cost is flat — **n^0.073** under constant density, among the
best here — which is the layout argument working. The search was never the
problem.

The cost is the rebuild, and the manifest's `O(1), plus a deferred O(n log n)` is
confirmed: **n^1.056**, from 40.4 ns per move at 1000 entities
to 6624 ns at 128000. One sort per tick is the entire candidate.

## Why it lost the k-nearest workload

The main suite had it beaten by a linear scan on `hs03_knn_heavy`. The sweep
says why, and the answer is density rather than population:

| population (world held fixed) | median tick |
|---:|---:|
| 1000 | 4073.9 us |
| 128000 | 458.0 us |

It gets **8.9x faster as the world fills up**. A k-nearest query
widens its search until it can prove it has the k closest, and in a sparse world
that means many doublings, each multiplying the cells searched by eight and each
cell costing a binary search. `hs03_knn_heavy` runs 25000 entities in a
1024x1024x256 world, which is the sparse end of that curve.

The fix is not a faster sort. It is to stop paying a binary search per cell — a
cell directory, or walking a Morton range instead of searching each cell.
