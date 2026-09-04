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
