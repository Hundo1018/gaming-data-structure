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

## Scaling (run `sweep-20260904T081902Z`)

Tracks its parent to within noise on every family, which is the check that the
pair still differs only in history: query n^0.078 against `uniform_grid`'s
n^0.083, move n^0.091 against n^0.08,
memory n^0.994
against n^0.994.

The sweep does not exercise rewind — no family in it rewinds — so it says nothing
about the history strategy itself. The experiment that would is a sweep of
`move_fraction` at fixed rewind depth, and then of `rewind_depth` at fixed
movement, which is the open question `hs01` and `hs02` leave at two points.

## Run `20261009T042136Z`

Suite `20261009T042136Z` and sweeps `sweep-20261009T044632Z`, same machine class, GCC 13.3.0, 5 repetitions, under the salted query contract. Step times below are medians across repetitions, the estimator `runner/predictions.py` judges with.

### The crossover these notes asked for

The two endpoints `hs01` and `hs02` left the crossing between this structure and
snapshot-and-rebuild somewhere between 2% and 100% of entities moving. The
parameter sweeps in `benchmarks/scaling.md` measure the cost of the rewind tick
itself, now reported apart from ordinary ticks:

- **Against the movement rate**, at depth 6: the log's rewind tick is below
  `uniform_grid`'s under snapshot-and-rebuild at 0.1 of entities moving (0.87x)
  and above it at 0.2 (1.28x). The crossing is between 10% and 20% moving.
- **Against depth**, at 10% moving: below at depth 4 (0.67x), level at depth 8
  (1.01x). The log's rewind grows linearly with depth (298 us at depth 1, 2035 us
  at 32) and the snapshot's does not move (700 to 745 us).

So the log wins when the product of movement rate and depth is small; somewhere
around a tenth of the population moving for six to eight ticks, it stops
winning. Neither strategy is the right default without both numbers, which is a
sharper form of the conclusion above.

`delta_grid` chooses between records and full pre-images per tick and patches
or rebuilds its index by how much is dirty; it is on the lower envelope at both
ends of the movement axis and above it in the middle. See its notes.

The three predictions encoded from the prose all held: `hs02` tail latency
0.33x and memory 0.41x the snapshot strategy's, and `hs01` tail latency 1.84x.
