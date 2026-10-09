# grouped_sparse_set — observed

Run `20261009T042136Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 5 repetitions. Verdicts are
`benchmarks/predictions.md`'s; step times there are the median across
repetitions.

## The test sparse_set asked for: grouping closes the gap

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | closes at least half the `h05` gap from `sparse_set` to `archetype` | 0.61 of it | held |
| P2 | `w01` and `w02` at least 15% faster than `sparse_set` | 0.760x, **0.879x** | **falsified** |
| P3 | `w03` within 15% of `sparse_set` either way | **0.612x** | **falsified** |

`sparse_set/notes.md` ended on a question: was its loss on
`h05_sparse_component` about packed arrays, or about the missing alignment that
EnTT-style owning groups add? With an owning Position+Velocity group and nothing
else changed, the median frame on `h05` went from 996.7 us to 665.6 us, against
`archetype`'s 452.6 us: 61% of the gap closed (medians across repetitions). P1 was the falsifying case and it
held, so most of what `sparse_set` lost there was alignment.

## The two falsified predictions went in opposite directions

P2 held on `w01` by a wide margin (0.76x) and missed on `w02` (0.88x against
0.85). `w02`'s queries are Position, Position+Velocity, and
Position+Velocity+Health. The group can only serve the second and integrate: the
first names one component and has nothing to align, and the third, under the
smallest-set rule, can drive from Health and probe the rest. That is a reading of
the code against the mix, not a per-query measurement.

P3 was falsified by being much faster than predicted: 296.9 us on
`w03_structural_churn` against `sparse_set`'s 487.8 us. The prediction expected
the swaps into and out of the group prefix to cost something on a workload
dominated by add and remove. They cost less than what the group saves: `w03`
integrates and queries Position+Velocity every frame, and those now walk two
aligned prefixes. A churn workload that never iterates would separate the two
effects; none exists.

## Where it stands

On the Pareto front of `w03_structural_churn` and `w05_small_world`. Ahead of
`sparse_set` on every ECS workload, behind `archetype` and `bitset_soa` on most.
Its generalization gap is -1.71 ranks, the largest in the ECS track in the good
direction: it does better on what it could not see.
