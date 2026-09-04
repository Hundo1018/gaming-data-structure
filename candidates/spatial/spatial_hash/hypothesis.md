# spatial_hash

## Hypothesis

The dense cell array is mostly empty and its size is set by the world rather
than by the population. Hashing cell coordinates makes memory follow occupancy
and removes the need for world bounds, for the cost of a probe per cell visited.

## Falsifiable prediction

Against `uniform_grid`, which it differs from only in how a cell is found:

- On any workload where entities occupy a small fraction of the world's cells,
  it uses less memory.
- It is slower on every workload, because twenty-seven array indexes become
  twenty-seven probes into a table nothing has warmed.
- On `s02_dense_clustered`, where occupied cells are a tiny fraction of the
  world, its memory advantage should be at its largest.

## What would falsify it

Using more memory than the dense grid. That would mean movement spreads
occupancy across enough distinct cells, and the table's doubling leaves enough
slack, that hashing loses on the axis it exists to win.
