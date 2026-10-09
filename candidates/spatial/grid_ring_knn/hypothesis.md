# grid_ring_knn

## Hypothesis

On a grid, what a k-nearest query costs is the cells it walks, not the entities
it examines. `uniform_grid` doubles a box half-width starting at one cell edge;
every round clears what it found, re-walks every cell of the larger box from
scratch, and partially sorts everything gathered. In a sparse world k = 64 takes
several doublings, and the last box is mostly empty cells and entities farther
than the answer needs. Visiting each cell once, nearest first, and stopping as
soon as no unvisited cell can hold anything nearer than the k-th best, should
walk a small fraction of those cells.

This is `uniform_grid` with one variable changed. The class derives from
`UniformGrid`, included by path, and overrides `query_knn` and `name()`; it also
overrides `reported_bytes()`, only to add what the search owns, and its
constructor runs the parent's and then derives a few limits from the grid's
dimensions in constant time. Insertion, removal, movement, both radius queries
and history (`kNativeRewind = false`, so snapshot-and-rebuild) are the parent's
code, so any difference between the two is the k-nearest search.

## Mechanism

**The table.** For an offset `d = (dx, dy, dz)` in cells, the least distance
from any point of the query's cell to any point of the offset cell is
`cell * sqrt(Q)` with `Q = sum over axes of max(0, |d| - 1)^2`. `Q` is an
integer, so the table is sorted exactly with a counting sort and split into
shells of equal `Q`; each shell carries one lower bound, `sqrt(Q)` in cell
edges, which a query scales by the cell edge. Every bound depends on
`|dx|, |dy|, |dz|` only, so the table stores one octant and each entry stands
for its up to eight sign reflections. It holds every offset with `Q < 16^2`:
3134 offsets in 214 shells, stored in two orders described below.

The table depends on nothing but its reach, not on the grid, the cell edge or
the population, so it is computed once, by the compiler, and every instance
shares it. Building it per instance would put it inside every
snapshot-and-rebuild rewind, which constructs a new structure, and a rebuild
would then cost more than the parent's for a reason unrelated to the search.
A grid of fewer than seventeen cells along an axis cannot hold the longest
offsets along it; a step whose component exceeds the grid's extent on its axis
has no reflection inside it and is skipped. The test runs only in shells whose
largest component can exceed the narrowest axis, so on grids at least
seventeen cells along every axis it never runs. So that a skipped step is
rarely even read, the table is stored twice, sharing its shells: within each
shell one copy ascends in `|dz|` and the other in `max(|dx|, |dy|)`. A grid
narrower across than in z walks the second; any other, a flat world among
them, walks the first. Either way the first step of a shell whose leading key
is past the grid ends that shell. The other axes are tested a step at a time,
and a step is read only to be passed over where the grid is short along more
than one axis. A shell that holds nothing the grid can use still costs its
bound test and one step read.

**The search.** From the query point's cell, walk the shells in order, skipping
reflections that fall outside the grid. Keep the best k in a bounded max-heap
ordered by the shared `nearer` (dist2, then id); a new entity enters only if the
heap is short or it is `nearer` than the top. Before each shell: if k are held
and the shell's bound, squared, is strictly greater than the k-th best dist2,
stop. Before walking a cell that holds anything, once k are held: compute the
exact lower bound from the query point to that cell's box, and skip the cell if
that bound is strictly greater than the k-th best. When the table is exhausted:
stop if the table covers every offset the grid can hold, or if k are held and
the bound of the first `Q` past the table exceeds the k-th best; otherwise
answer with the parent's widening search, `UniformGrid::query_knn`, from
scratch. The answer is then the parent's, exact wherever the parent's is. The
parent's stop test squares its box half-width, so once squared distances
overflow float (worlds wider than about `1.8e19`) neither is exact there; this
candidate inherits that limit and does not widen it. A count of entities
examined also stops the walk at the end of a shell once it equals the live
population, which is how a query with k at or above the population ends
without walking empty cells to the table's edge.

**How a step becomes cells.** Per query and per axis, two small lookups are
filled as far as the walk has reached: the change in linear cell index for
moving d cells along the axis (or a large negative sentinel if that leaves the
grid), and the squared gap from the query to that slab of cells, in world
units. A cell's exact bound is the sum of its three slabs' gaps. Each octant
step is expanded into its eight reflections, unrolled; the minus reflection of
a zero component is the plus one again and gets the sentinel too. A reflection
whose summed index is negative or past the grid is rejected by one unsigned
comparison, so the walk branches on the table rather than on where the query
sits, and the head loads of successive cells can overlap. The cost is that
duplicates and cells outside the grid are still visits, a fifth to a third of
them depending on the shell.

**Why the answer is exactly the oracle's sequence.** A skipped cell or an
unwalked shell had a lower bound strictly greater than the k-th best at the time,
and the k-th best only moves nearer afterwards, so everything in it is strictly
farther than the final k-th. Equality is not enough to skip: an unvisited
entity at exactly the k-th best dist2 with a smaller id belongs in the answer in
place of the current k-th, so a cell whose bound equals the k-th distance is
visited. The heap's admission test is `nearer`, so ties at the k-th position are
resolved by id exactly as the oracle's sort resolves them, and the result is
emitted by `sort_heap` under `nearer`.

**Why float rounding cannot make a bound exceed a distance.** A bound has to be
no larger than the dist2 the substrate will compute for any entity in the cell,
not just no larger than the real squared distance, and four things can push a
naive bound past it:

- The cell an entity is filed in is `floor((v - lo) * inv_cell)` in float, so a
  point can sit slightly outside the cell box it is filed in: by at most about
  `4 * n * 2^-24` cell edges for `n` cells on the axis. The parent floors the cell
  at a 256th of the largest extent, so `n <= 258` and the error is under
  `6.2e-5` of a cell edge. The query's own cell has the same error.
- The outermost cells also hold whatever `axis_index` clamped into them. Their
  outer side is treated as unbounded in the exact per-cell bound; the shell
  bound needs no change, because a point clamped into an edge cell, query or
  entity, lies beyond that cell's box on the outer side, so its distance to
  anything in another cell only grows.
- `dist2` itself can round below the real squared distance by a relative
  `5 * 2^-24` at most, and computing the bound can round up by about as much.
- A bound can overflow. Every bound is a length in cell edges, scaled by the
  cell edge into world units and only then squared. Squaring the cell edge
  first would overflow float once the edge passes `2^64` (about `1.8e19`). The
  bound of every cell but the query's own would then be infinite and would
  prune neighbours a fraction of an edge away, whose `dist2` is finite. Scaled
  first, a bound overflows only when the length it squares is already past
  `sqrt(FLT_MAX)`. The distance it bounds is longer still, by the slack below,
  so the `dist2` of anything it prunes is infinite too. Nothing at infinity is
  pruned while the k-th best is itself infinite, because the tests are strict.

Every bound is therefore shortened by a slack of `1/256` of a cell edge per axis
(per-cell bound) or along the radius (shell bound), and every squared bound is
multiplied by `1 - 2^-16`. Both are more than ten times what the errors above
can reach. They cost almost nothing: a bound short by 0.4% of a cell edge
admits a sliver more than the exact bound would.

## Constants

- **Table reach, 16 cell edges** (`Q < 256`). Coverage against memory. The
  sparsest public configuration that asks for k-nearest is the `spatial_knn`
  sweep at 1000 entities in the fixed world: 0.0064 entities per cubic cell,
  k = 8, so the 8th neighbour lies about 6.7 cell edges away in the interior and
  about 8.4 against the world's top or bottom face, where only half the ball is
  inside. It passes 16 only for queries close to a corner of the world, well
  under one query in a thousand. The table is one octant of four-byte entries:
  3134 of them (12.2 KB) in each of two orders plus 214 twelve-byte shells,
  27 KB in all, one copy per program; a grid walks one order, so 15 KB of it
  is in use. That is under 5% of the 636 KB cell array of the 86 x 86 x 22
  grid of the 1024-wide worlds, and about seven tenths of the 38 KB cell
  array of `s05_small_world`'s 33 x 33 x 9 grid. It is static data, so the
  allocation tracker does not see it; `reported_bytes()` counts all of it
  anyway. The compiler builds it, so neither construction nor a
  snapshot-and-rebuild rewind pays for it. Doubling the reach to 32 would
  multiply its size by about eight for coverage that no public workload uses.
- **Slack, 1/256 of a cell edge**, and **shrink, `1 - 2^-16`**: see the
  rounding argument above. Both are first-principles bounds with a safety factor
  over ten, not tuned.
- **Cell size**: the parent's, unchanged. Changing it would change the radius
  queries and the pair would no longer differ in one thing.

## Falsifiable prediction

All figures are median tick time, against `uniform_grid` on the same machine in
the same run.

- **P1.** On `hs03_knn_heavy` (128 k-nearest queries per tick, k = 64), at most
  0.5x `uniform_grid`'s. The floor tool puts `uniform_grid` at about 12x the
  irreducible tick cost there, the largest gap in the track; with k = 64 the
  parent's box has to double past the 64th neighbour, re-walking every inner
  cell each time, while the table walk stops within about one cell edge of it.
- **P2.** On `s05_small_world`, at most 0.7x `uniform_grid`'s. There the 8th
  neighbour is about 2.2 cell edges away; the parent's rounds of half-width one,
  two and usually four cell edges walk on the order of 600 cells per query, the
  table walk on the order of 180 offsets with fewer head loads than that. The
  rest of the tick (1500 moves, 256 radius queries) is the parent's code, which
  is why the bound is 0.7x and not lower.
- **P3.** On no spatial workload slower than `uniform_grid` by more than 10%.
  Nothing but the k-nearest search changed; on workloads where k-nearest is a
  small share of the tick the two should be within noise. That includes the
  rewinding ones: the table is not rebuilt with the structure, so a
  snapshot-and-rebuild constructs nothing the parent's does not beyond a few
  limits derived from the grid's dimensions.
- **P4.** In the `spatial_knn` sweep, `fixed_world` regime, at population 1000
  (the sparsest point), at least 3x faster than `uniform_grid`: the parent's
  box has to reach half-width eight cell edges and sometimes sixteen, thousands
  of cells, where the table walk examines the cells within about seven. In the
  `fixed_density` regime its time exponent is no higher than `uniform_grid`'s:
  at fixed density and fixed k the k-th neighbour's distance in cell edges is
  constant, so is the walk.

## What would falsify it

Failing P1. That would mean the k-nearest cost on a grid is the entities
examined rather than the cells re-walked, and the search order is not where the
time goes.
