# delta_grid — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s; step times there are the
median across repetitions. "Rewind tick" is the median of the ticks that open
with a rewind, which the harness now reports separately.

## Six of eight held, including the one that could have sunk it

| | prediction | measured | verdict |
|---|---|---:|---|
| P1a | `hs01` median below snapshot-and-rebuild and the undo log | 0.85x, 0.70x | held |
| P1b | `hs01` p99 below both | 0.86x, 0.47x | held (within noise against snapshot) |
| P2a | `hs02` median within 1.3x of `grid_undo_log` | 1.318x | **falsified** (within noise) |
| P2b | `hs02` p99 within 1.5x of `grid_undo_log` | 1.464x | held (within noise) |
| P2c | `hs02` p99 below snapshot-and-rebuild | 0.48x | held |
| P3 | within 1.2x of `cell_sorted` where everything moves | 1.11x to 1.19x | held |
| P4 | `spatial_move` exponent at most 0.3 | -0.033, 0.086 | held |
| P5 | rollback sweep p99 within 1.2x of the better strategy everywhere | up to 6.5x | **falsified** |

P1 was the falsifying case named before measurement: that keeping history
inside the structure would still lose to snapshotting outside it when everything
moves. It did not lose. On `hs01_rewind_all_moving` the rewind tick is 1081 us,
against 1834 us for `uniform_grid` under snapshot-and-rebuild and 4002 us for
`grid_undo_log`, and the median tick is the lowest in the population. On the
held-out `hs08_crowd_rollback`, written after it, its rewind tick is 2767 us
against 5045 and 13176.

## Where it is not the lower envelope

The rollback sweeps say exactly where, and why.

Against the movement rate, at depth 6, the rewind tick in microseconds:

| move fraction | 0.01 | 0.05 | 0.1 | 0.2 | 0.35 | 0.5 | 1.0 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `grid_undo_log` | 139 | 334 | 565 | 1011 | 1668 | 2276 | 3919 |
| snapshot-and-rebuild | 535 | 583 | 653 | 788 | 970 | 1233 | 1803 |
| `delta_grid` | 185 | 376 | 970 | 880 | 1009 | 1060 | 1040 |

At the ends it is within a few tens of microseconds of the better strategy or
ahead of both. In the middle, from 0.1 to 0.35, it is worse than both. There the
rewind dirties about half the population, the rule `dirty > ALPHA * live` with
ALPHA = 0.5 chooses a full rebuild, and a rewind tick on that path costs 880 to
1009 us, against 653 to 970 us for a snapshot-and-rebuild rewind tick at the
same rates. The cost argument in `hypothesis.md`
set ALPHA by comparing a rebuild with a relink per dirty id, and it was right
about the shape and wrong about the constant: this structure's rebuild clears
and sums a directory of 162712 cells and walks the whole id space, so it is not
cheap enough for the switch to sit at a half.

Against depth, at 10% moving, the rewind tick goes 320, 376, 506, 1070, 1138,
1302 us for depths 1 to 32. The undo log grows linearly to 2035 and the snapshot
stays near 720. Past depth 4 every rewind takes the rebuild path and also
unwinds every frame between, so at depth 32 it is worse than snapshotting. The
frames choose between records and full pre-images by how much changed in each
tick, never by how far back a rewind will go.

## What it says, and the next mutation

Choosing the history representation and the index upkeep per tick from counts
does put one structure on the lower envelope at both ends of the movement axis,
which neither fixed strategy manages: at 1.0 it beats snapshotting by 1.7x and
at 0.01 it is within 50 us of the log. What it does not do is choose well in
between, and the sweep locates the two causes: a rebuild dearer than the cost
argument assumed, and no checkpoint to jump to on a deep rewind.
The mutation the evidence points at is a periodic full checkpoint among the
records, so a deep rewind restores the nearest one and unwinds only the frames
after it, with the rebuild threshold set from the rebuild cost this run
measured rather than from a ratio chosen in advance.
