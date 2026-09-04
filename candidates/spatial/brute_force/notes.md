# brute_force — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## Slowest on nine of ten, and not on the tenth

The interesting number is where an index stops paying for itself, and
`s05_small_world` was built to find it. At 1500 entities `uniform_grid` is
still 5.0x faster (239.5 us against 1205.7 us), so the crossover is below 1500
entities — lower than the workload was designed to bracket, and the run does not
locate it.

The one place it is competitive is `hs03_knn_heavy`, where nearest-neighbour
queries with k of 64 dominate:

| candidate | hs03_knn_heavy |
|---|---:|
| `uniform_grid` | 5723.3 us |
| `spatial_hash` | 19890.3 us |
| `brute_force` | 20115.4 us |
| `morton_sorted` | 34043.7 us |

It beats one index outright and finishes level with another. A widening search
that has to prove it has the k closest can cost more than looking at everything
once, and two of the four indexes here cross that line.

## Cost profile

Its allocation count is the lowest in the population by two orders of magnitude
on most workloads — a handful of vectors and nothing per query — and its memory
is the smallest or near it everywhere except the workloads that rewind, where
its full-copy history is the same size as anyone else's.

It reaches the Pareto front five times, always on memory, never on speed. That
is the front working: nothing was dropped for losing on the objective someone
happened to care about most.
