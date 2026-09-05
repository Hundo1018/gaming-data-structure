# spatial_hash — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## The memory prediction was falsified on nine of ten workloads

The hypothesis said it would use less memory than the dense grid wherever
entities occupy a small fraction of the world's cells, and named the
falsification: using more. It uses more on nine of the ten workloads.

| workload | spatial_hash | uniform_grid |
|---|---:|---:|
| `s01_steady_uniform` | 6.55 MB | 1.10 MB |
| `s02_dense_clustered` | **1.29 MB** | **5.84 MB** |
| `s03_wide_radius` | 0.59 MB | 0.50 MB |
| `s05_small_world` | 0.42 MB | 0.08 MB |
| `hs03_knn_heavy` | 6.69 MB | 1.23 MB |
| `hs05_spawn_churn` | 8.20 MB | 2.52 MB |

Two things it did not account for. A slot costs sixteen bytes against a dense
cell's four, so it needs occupancy below a quarter before it is even level. And
a cell that empties keeps its slot, so with every entity moving every tick the
occupied set grows toward every cell in the world within a few hundred ticks —
the sparsity it was counting on is a property of an instant, not of a run.

The one workload where it wins is the one where the population genuinely never
visits most of the world: eight tight clumps, entities moving 0.2 to 1.5 units a
tick, and 4.5x less memory than the grid for it. That is the case the design is
for, and it is narrower than the hypothesis assumed.

## The speed prediction held

Slower than `uniform_grid` on all ten workloads: 1.09x on the clustered
workload where cells are crowded and the probe is amortised over a long walk,
1.90x on the even baseline, and 3.5x on `hs03_knn_heavy` where a widening
nearest-neighbour search turns into thousands of probes and it finishes level
with scanning everything (19890.3 us against `brute_force`'s 20115.4 us).

## What survives

The claim that it needs no world bounds is untested here — every workload has a
finite world because the shared wrap requires one. A workload with an unbounded
or streamed world would be the one to run this against, and the substrate cannot
express it yet.

## Scaling (run `sweep-20260904T081902Z`)

Query cost is flat, **n^0.04** under constant density, matching the
dense grid it is a mutation of. Move cost is flat too, **n^0.086**.
The probe per cell is a constant factor, not a growth term: how it scales was
never its problem.

Its k-nearest behaviour is the sharpest single finding in the sweep:

| population (world held fixed) | median tick |
|---:|---:|
| 1000 | 6841.0 us |
| 128000 | 268.2 us |

**25.5x faster with 128x more entities.** The widening k-nearest
search is driven by density, not by population: in a sparse world it doubles its
radius many times before it can prove it has the k closest, and every doubling
multiplies the cells visited by eight, each one a hash probe. This is the same
effect that left it level with a linear scan on `hs03_knn_heavy`, and it puts
the weakness in the search strategy rather than in hashing.
