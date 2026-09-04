# grid_undo_log — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median tick time unless stated.

## Both halves of the prediction held

This is the pair that isolates the history strategy: the index is
`uniform_grid`'s code, included by path, and the only difference is where
history lives. On the eight workloads that never rewind the two are within noise
of each other and use identical memory (`s01`: 984.4 us and 1.10 MB against
968.0 us and 1.10 MB), which is the check that the pair really does differ in one
thing.

**Few entities moving — the log wins, and wins on every axis.**
`hs02_rewind_few_moving`, two entities in a hundred moving per tick:

| | grid_undo_log | uniform_grid under snapshot-and-rebuild |
|---|---:|---:|
| median tick | 141.1 us | 184.2 us |
| p99 tick | 288.4 us | 871.9 us |
| peak memory | 2.92 MB | 7.12 MB |

It is the sole occupant of that workload's Pareto front. The tail is where the
difference really shows: a rebuild pays for the whole population on the ticks
that rewind, so its p99 is 4.7x its median, while the log pays for what moved
and its p99 is 2.0x its median.

**Every entity moving — the log loses, as predicted.**
`hs01_rewind_all_moving`:

| | grid_undo_log | uniform_grid under snapshot-and-rebuild |
|---|---:|---:|
| median tick | 2297.3 us | 1997.5 us |
| p99 tick | 6063.9 us | 3086.0 us |
| peak memory | 11.17 MB | 7.75 MB |

The log becomes a snapshot with extra bookkeeping — a record per entity per
tick, each carrying an id and a liveness byte the snapshot gets for free — and
unwinding it costs a remove and an insert per record where the rebuild costs one
insert per entity. Worse on all three objectives, and not on the front.

## What this settles

The choice between the two is not a matter of taste, and it is not a property of
the index. It is set by the movement rate, and the crossover is somewhere between
2% and 100% of the population moving per tick. Neither strategy is the right
default without knowing that number for the game in question.

## Next test

Find the crossover. The two workloads here are the endpoints; the experiment
that is missing is a sweep of `move_fraction` at fixed rewind depth, and then
the same sweep of `rewind_depth` at fixed movement, since the log's cost grows
with both and the rebuild's grows with only one.
