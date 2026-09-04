# reference — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median frame time unless stated.

Slowest median frame time on all 10 ECS workloads, by between 1.3x
(`h04_recent_locality`: 1317.2 us against `archetype`'s 998.4 us) and 13.4x
(`h05_sparse_component`: 7547.8 us against 562.2 us).

Allocation count is the clearest separator: 106000 to 320000 allocations per run
against 19 to 700 for the array-based candidates, because every entity is a
separate node.

It reaches the Pareto front once, on `h02_bursty_spawn`, at 20.85 MB against
`archetype`'s 21.88 MB. It is 2.7x slower there and is kept because it is
smaller. That is the front working as intended: nothing was dropped for losing
on the objective someone happened to care about most.
