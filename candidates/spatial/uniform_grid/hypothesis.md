# uniform_grid

## Hypothesis

With cells the size of a query, a radius query touches twenty-seven cells no
matter how big the world is. Query cost then follows local density rather than
population, and movement is two unlinks and two links with no allocation.

## Falsifiable prediction

- On an evenly spread world it is the fastest candidate here, and its margin
  over scanning everything grows with the population.
- On `s02_dense_clustered`, where the queries are aimed into clumps that put
  hundreds of entities in one cell, the margin collapses: cost is set by how
  many entities are in the cells, and clustering puts them all in a few.
- On `s04_teleport` it pays a relink for every jump, so it should lose ground
  against the candidates that rebuild their form each tick and cannot tell a
  jump from a step.

## What would falsify it

Holding its margin on the clustered workload. That would mean cell occupancy is
not the thing that decides its cost, and the case for a hierarchy over a flat
grid is weaker than assumed.
