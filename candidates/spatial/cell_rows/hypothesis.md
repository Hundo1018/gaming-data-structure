# cell_rows

## Hypothesis

In a cell-sorted array the cells of one row of a query box are adjacent, so the
row is one contiguous run that two directory reads find. Reading it as one run
instead of cell by cell removes a directory read and a block tail per cell and
lets the vector loop run over the whole row. The longer the rows of a query's
box, in cells, the more this saves.

`cell_sorted` reads each cell on its own on purpose, so that its comparison
with `uniform_grid` is about the key and the directory and nothing else. Its
reviewer measured the row walk as a scratch variant on public workloads and
found it accounted for about half of `cell_sorted`'s margin over the grid on
`s01_steady_uniform` and about 70% on `s05_small_world`. This candidate makes
that variant a member of the population, so the effect is measured by the
suite rather than reported from a scratch build, and attributed to its own
variable.

## Falsifiable prediction

Written after the reviewer's public-workload numbers were known, so every
threshold below is on a workload neither of them measured.

- **P1.** On no spatial workload slower than `cell_sorted` by more than 3%. The
  row walk visits the same entities as the cell walk with fewer reads.
- **P2.** On `hs06_mixed_reach`, at most 0.9x `cell_sorted`'s median tick. The
  cell edge there is 65 and queries reach up to 128, so the large ones span up
  to five cells per row.
- **P3.** On `hs03_knn_heavy`, at most 0.9x `cell_sorted`'s median tick. The
  widening k-nearest box at k = 64 spans many cells per row in a sparse world,
  and that is the longest row any workload here produces.

## What would falsify it

Failing P2 and P3 together: then the run length is not where a cell-sorted
query's time goes, and the reviewer's scratch measurement on public workloads
did not generalise to the held-out ones.
