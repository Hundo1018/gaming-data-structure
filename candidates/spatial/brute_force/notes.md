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

## Scaling (run `sweep-20260904T081902Z`)

The positive control for the whole scaling method: it looks at every entity for
every query, so its query exponent has to come out at 1. It measures
**n^1.01, r2 0.9999**. Nothing else in
`benchmarks/scaling.md` is worth reading unless this came out right.

**Its own `O(1)` claim for move is the one the automatic check rejected.** The
manifest says insert, remove and move are O(1), which is true: each is one array
write. Measured time is not.

| population | ns per move |
|---:|---:|
| 1000 | 8.9 |
| 128000 | 66.7 |

n^0.361 after the move family's forcing query is subtracted out. The operation
count is constant and the time is not, because 2000 random writes into an array
growing from 16 KB to 2 MB stop hitting L1 and start reaching main memory.

That is the argument for measuring rather than declaring, and it is why
`complexity:` is now compared against a measurement instead of sitting in the
manifest unread. The claim stays as written: it is a correct statement about
operation counts, and editing it to match a machine would destroy the comparison
that produced the finding.

## Run `20261009T042136Z`

Suite `20261009T042136Z` and sweeps `sweep-20261009T044632Z`, same machine class, GCC 13.3.0, 5 repetitions, under the salted query contract. Step times below are medians across repetitions, the estimator `runner/predictions.py` judges with.

The positive control came out lower this time: query exponent **0.937** at
fixed density (r2 0.9996) and 0.949 at fixed world, against 1.01 in the earlier
run. The pairwise slopes are 1.01, 0.94, 0.97, 0.95, 0.84, 0.91, 0.99: one low
pair, between 16000 and 32000 entities, pulls the fit down. A scan of every
entity is linear in operations, so a fitted 0.94 is the measurement's own error
on this machine in this sitting, and it bounds how far any other exponent in
`benchmarks/scaling.md` should be read: differences of a few hundredths are
not findings.
