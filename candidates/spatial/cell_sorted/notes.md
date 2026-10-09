# cell_sorted — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s.

## Four of five held; the design argument survived

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | below `morton_sorted` on all ten original workloads | 0.16x to 0.41x | held |
| P2 | below `uniform_grid` where everything moves | 0.66x to 0.91x | held |
| P3 | at least 1.5x slower than `uniform_grid` on `hs02` | 4.47x | held |
| P4 | `spatial_move` exponent at least 0.8, both regimes | 1.011 fixed density, **0.585 fixed world** | **falsified** |
| P5 | `s04_teleport` within 5% of its own `s01` | 1.045 | held (within noise) |

P2 was the falsifying case named before measurement. It held on all four
workloads: `s01` 0.91x, `s03` 0.66x, `s04` 0.79x, `hs04` 0.85x of
`uniform_grid`. A contiguous array rebuilt by counting sort every tick beats the
incrementally maintained linked grid wherever every entity moves every tick.
`morton_sorted`'s notes said its layout argument had not been given its best
case; this is that case, and the argument holds once the sort is a counting sort
and the lookup is a directory.

## P4: the directory is a constant the population does not touch

At fixed density the move family's exponent is 1.011: the rebuild is linear in
the population, as claimed. At fixed world it is 0.585. The rebuild clears and
sums a directory of one entry per cell, 162712 of them in that world, and with
the world held still that cost does not grow at all; at small populations it is
most of the rebuild, and the curve is flat at the bottom. The implementer
recorded this risk in `hypothesis.md` before any measurement and left the
threshold where it was. The claim was about the rebuild's growth with
population, and the directory is the one part of the rebuild that does not grow
with it.

## Where it stands

On the Pareto front of four workloads. It loses wherever little moves:
`hs02_rewind_few_moving` at 4.5x the grid and `hs05_spawn_churn` at 1.21x. Its
child `cell_rows`, which reads each row of a query box as one run, is faster on
eleven of the thirteen workloads, by under 1% on two of them (`hs01`, `s03`);
see that candidate's notes.

The comparison stays clean because of a review finding. The first version read
each row of the box as one run, which `uniform_grid` cannot do, and the reviewer
measured that change as about half of the margin over the grid on `s01`. The
walk was put back to one range per cell, as the grid reads its list heads, and
the row walk became its own candidate.
