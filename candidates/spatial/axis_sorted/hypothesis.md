# axis_sorted

## Hypothesis

Partitioning one axis is most of the benefit for a fraction of the machinery. A
query is one binary search and one sequential run, with no cell size to choose,
no bounds to configure and no pointer chasing.

## Falsifiable prediction

The slab is thin in x and spans the world in y and z, so the entities examined
per query should be about `n * 2r / world_size` — roughly 2% of the population
at the baseline settings — against a three-axis structure's small constant.

- Far better than scanning everything.
- Clearly worse than `uniform_grid` on an evenly spread world, by roughly the
  ratio between the slab's population and a query box's.
- On `hs04_flat_world`, where the third axis is eight units of a thousand, the
  gap to the grid should narrow, because partitioning z was buying the grid
  almost nothing there.

## What would falsify it

Matching the grid on the evenly spread baseline. That would mean the extra
entities in the slab are cheap enough, being contiguous, that partitioning two
more axes buys nothing at this population.
