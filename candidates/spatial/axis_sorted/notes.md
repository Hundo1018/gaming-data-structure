# axis_sorted — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## Two of three predictions held

**Far better than scanning everything.** 3142.8 us against 15339.9 us on the
even baseline, a factor of 4.9.

**Clearly worse than partitioning three axes.** 3.25x slower than
`uniform_grid` there. The predicted mechanism is the right one: the slab is
thin in x and spans the world in y and z, so it examines roughly
`n * 2r / world_size` entities — about 2% of the population at these settings
— where a three-axis structure examines a small constant.

## The flat-world prediction was falsified

It predicted the gap to the grid would narrow on `hs04_flat_world`, where the
third axis is eight units of a thousand and partitioning it should buy the grid
almost nothing.

| workload | axis_sorted | uniform_grid | ratio |
|---|---:|---:|---:|
| `s01_steady_uniform` | 3142.8 us | 968.0 us | 3.25x |
| `hs04_flat_world` | 3186.0 us | 1005.8 us | 3.17x |

The gap did not move. Flattening the world helps both of them and by the same
amount: the grid's query box collapses from twenty-seven cells to nine, and this
structure's slab loses the same proportion of its population. The prediction
assumed the flattening was a cost only the grid was paying, and it was not.

## Where it earns its place

It is on the Pareto front of four workloads, never for speed and always for
memory: 0.98 MB on `hs03_knn_heavy` against `spatial_hash`'s 6.69 MB, with
no cell size to choose and no world bounds to configure. It is the cheapest
thing here that is not a linear scan, and its cost is predictable from two
numbers — the population and the ratio of query radius to world size — which the
structures with cells are not.

## Scaling (run `sweep-20260904T081902Z`)

**The predicted exponent shows up in the curve, not in the fit.**
`hypothesis.md` says the slab holds about `n * 2r / world_size` entities, so
under constant density — where the world grows as the cube root of the
population — query cost should follow n^(2/3) = 0.667. The fitted exponent over
the whole range is 0.538. The slope of each successive doubling is:

```
0.41  0.39  0.39  0.47  0.54  0.55  1.40
```

It climbs from 0.41 toward the predicted 2/3 exactly as the slab term
takes over from the fixed cost of two binary searches, which is most of the work
when a query examines about 47 entities. Then the final doubling jumps to
1.40, which is a discontinuity rather than a trend: at 128000 entities
the three sorted arrays hold about 2.6 MB, which is where this machine's cache
hierarchy changes. That reading is a guess — settling it needs the hardware
counters, and `perf_event_open` is unavailable in this container.

So the prediction is neither confirmed nor refuted by the fitted number. The
curve supports it over the range where a single power law describes the
structure, and stops describing it at the top.

Its move cost is the deferred sort the manifest claims: n^1.099,
from 43.6 ns per move at 1000 entities to 8871 ns at 128000.
