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

## Historian

Classified **KNOWN_COMPONENTS_NEW_COMBINATION** after measurement, by a
Historian agent with literature search and a skeptic agent told to find closer
prior work and to check every citation. The Historian said
KNOWN_COMPONENTS_NEW_COMBINATION (medium confidence); the skeptic upheld it.

Each half is known: the history half is mixed incremental and full state saving
from optimistic (Time Warp) simulation, and the index half is a differential
file, a read-optimised base with a write-optimised delta merged by rebuilding.
No documented structure combines the two with the choice made per tick from
counts. The skeptic found that hybrid checkpointing, periodic full saves among
incremental logs, is published (Soliman and Elmaghraby 1998), which is the
mutation this candidate's notes proposed next: it is a known remedy, not a new
one.

Closest known work, as cited (sources the agents report having read):

- **Autonomic incremental / non-incremental log-restore for optimistic (Time
  Warp) simulation.** R. Vitali, A. Pellegrini, F. Quaglia, 'Autonomic
  Log/Restore for Advanced Optimistic Simulation Systems', IEEE MASCOTS 2010,
  pp. 319-327, DOI 10.1109/MASCOTS.2010.40 (record:
  https://art.torvergata.it/handle/2108/323642). Follow-up: A. Pellegrini, R.
  Vitali, F. Quaglia, 'Autonomic State Management for Optimistic Simulation
  Platforms', IEEE TPDS 26(6):1560-1569, 2015. Per-object tuning: same authors,
  'An Evolutionary Algorithm to Optimize Log/Restore Operations within
  Optimistic Simulation Platforms', SIMUTools 2011.
- **Mixed (sparse + incremental) state saving with forward and backward
  recovery.** V. Cortellessa, F. Quaglia, 'A checkpointing-recovery scheme for
  Time Warp parallel simulation', Parallel Computing 27(9):1227-1252, 2001, DOI
  10.1016/S0167-8191(01)00081-3. Background: R. Ronngren, M. Liljenstam, R.
  Ayani, J. Montagnat, 'Transparent Incremental State Saving in Time Warp
  Parallel Discrete Event Simulation', PADS 1996
  (https://hal.archives-ouvertes.fr/hal-00691810).
- **Differential file / main+delta (read-optimised base, write-optimised delta,
  tombstones, merge by rebuild).** D. G. Severance, G. M. Lohman, 'Differential
  Files: Their Application to the Maintenance of Large Databases', ACM TODS
  1(3):256-267, 1976. P. O'Neil, E. Cheng, D. Gawlick, E. O'Neil, 'The
  Log-Structured Merge-Tree (LSM-Tree)', Acta Informatica 33(4), 1996. M.
  Stonebraker et al., 'C-Store: A Column-oriented DBMS', VLDB 2005 (read store +
  write store + tuple mover, deletion marking). J. Krueger et al., 'Fast Updates
  on Read-Optimized Databases Using Multi-Core CPUs', PVLDB 5(1):61-72, 2011
  (SAP HANA main/delta merge).
- **Static-to-dynamic transformation / global rebuilding.** J. L. Bentley, J. B.
  Saxe, 'Decomposable Searching Problems I: Static-to-Dynamic Transformation',
  Journal of Algorithms 1(4), 1980. M. H. Overmars, 'The Design of Dynamic Data
  Structures', LNCS 156, Springer, 1983.
- **Undo code blocks with Instruction/Object Dominance, a threshold switch to
  per-chunk snapshots, and reverse scrubbing from a later checkpoint** (found by
  the skeptic). D. Cingolani, A. Pellegrini, F. Quaglia, 'Transparently Mixing
  Undo Logs and Software Reversibility for State Recovery in Optimistic PDES',
  ACM TOMACS 27(2), article 11, 2017, DOI 10.1145/3077583 (conference version
  SIGSIM-PADS 2015, DOI 10.1145/2769458.2769482). Open postprint:
  iris.uniroma1.it handle 11573/1096541. I read the full text: Sections 2.1-2.3
  and 3.4.
- **Hybrid checkpointing (periodic full saves plus per-event incremental logs;
  restore the nearest checkpoint before or after the target, then apply the
  increments)** (found by the skeptic). H. M. Soliman, A. S. Elmaghraby, 'An
  Analytical Model for Hybrid Checkpointing in Time Warp Distributed
  Simulation', IEEE TPDS 9(10):947-951, 1998 (dblp pid 34/915). Its mechanism is
  as summarised in the related work of A. Mazzucchi, 'Grid Checkpointing', arXiv
  2609.05428 (2026), Section 4.1.

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
