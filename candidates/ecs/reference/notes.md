# reference — observed

Run `20260902T092528Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -march=native`, 5 repetitions, median repetition reported.

Slowest median frame time on all 10 workloads, by between 1.5x
(`h04_recent_locality`: 954 us against `archetype`'s 580 us) and 18x
(`h05_sparse_component`: 6576 us against 367 us). Allocation count is the
clearest separator: 106000 to 320000 allocations per run against 19 to 700 for
the array-based candidates, because every entity is a separate node.

It reaches the Pareto front once, on `h02_bursty_spawn`, at 20.85 MB against
`archetype`'s 21.88 MB. It is 3.6x slower there and is kept because it is
smaller. That is the front working as intended: nothing was allowed to drop out
because it lost on the objective someone happened to care about most.
