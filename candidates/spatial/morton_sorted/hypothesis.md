# morton_sorted

## Hypothesis

Give up incremental update entirely. Rebuild the searchable form from scratch
whenever anything has changed, and in exchange lay it out perfectly: sorted by
the Morton code of the cell, so that neighbours in the world are neighbours in
memory and a cell is a contiguous run rather than a pointer chase.

## Falsifiable prediction

- On `s04_teleport` it should lose nothing at all relative to its own baseline,
  where the incremental structures pay a relink per jump, so its **ranking**
  should improve there even if its absolute time does not.
- On query-heavy workloads with many entities it should beat the linked-list
  grid, because the grid's cell walk chases pointers and this one does not.
- On `s02_dense_clustered` it should hold up better than the grid, since nothing
  is preallocated per cell and a dense cell is simply a longer contiguous run.

## What would falsify it

Losing to `uniform_grid` on every workload. That would mean one sort per tick
costs more than every pointer chase it removes, and the whole trade of update
speed for layout does not pay at these populations.
