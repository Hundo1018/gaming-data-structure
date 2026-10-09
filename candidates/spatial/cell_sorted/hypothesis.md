# cell_sorted

## Hypothesis

`morton_sorted` bet that an index thrown away and rebuilt into perfect order
once per tick would beat one kept current incrementally. It lost to
`uniform_grid` on all ten workloads, and its notes name two costs that belong
to the implementation rather than to the bet: a `std::sort` every tick, and one
binary search per cell on every query. They end with "the layout argument has
not been given its best case". This candidate is that best case. It keeps the
bet unchanged and removes both costs by changing the sort key and the lookup,
and nothing else.

**The key.** Entities are ordered by `uniform_grid`'s dense cell index — same
cell edge, same floor, same clamping, same order (x fastest, then y, then z) —
instead of by a Morton code. A dense integer key needs no comparison sort. A
counting sort orders the whole population in two passes over the entities and
two over the cells: clear the per-cell counts, count, turn the counts into
running totals, scatter.

**The lookup.** The running totals the counting sort computes are themselves a
directory. The entities of cell `c` are the range `[start[c], start[c+1])` of
the sorted array, so finding a cell is one array read, as it is in the grid,
where the parent paid a binary search. A query walks the cells overlapping its
box in `uniform_grid`'s order and scans each cell's range: a box of 3x3x3 cells
is 27 directory lookups and up to 27 contiguous ranges, where the grid makes 27
head reads and follows 27 linked lists.

One further change is available and not taken. Because x varies fastest in the
cell index, the cells of one row of a box are consecutive and so are their
ranges, so a row could be read as a single run from two directory reads. The
grid's layout cannot do that, so it is a second variable rather than part of
the directory; it is left out here so that this pair differs only in the key
and the lookup, and is better tested by a descendant against this one.

**The scan.** The sorted copy is stored as four separate arrays — ids, x, y, z —
so a cell's range is read with contiguous loads. The distance test runs over
fixed blocks of eight slots, with the lanes past the end of the range masked
off, which is a loop shape the compiler vectorises at `-O2`; the hits of a
block are then folded into the digest one at a time. The accept test is the
substrate's `dist2(p, c) <= r*r`, called per lane.

**What is unchanged from the parent.** Positions and liveness are kept per id in
flat arrays and are the authority; `position_of` reads them. Insert, remove and
`move_by` write those arrays and mark the index stale. The first query after any
mutation rebuilds the whole index once, so no observation sees a stale index. The
k-nearest search is `uniform_grid`'s box-doubling loop, unaltered, gathering from
each cell's contiguous range; another candidate tests the search strategy
separately.

The rebuild has two terms. The entity passes are linear in the number of ids
the workload issues (`max_entity_id + 1`), live or not, because the
authoritative arrays are indexed by id: under spawn churn that is every id ever
issued, which can be many times the live population. The two directory passes
are linear in the number of cells, which is set by the world and the query
radius, not by the population: 162712 cells for 20000 entities on `s01` (eight
per entity), 1.26 million for 40000 on `s02` where the cell edge is 6 (31 per
entity). At a fixed world size that term is a constant every tick pays.

## Constants

- **Cell edge**: the typical query radius, floored at a 256th of the world's
  largest extent and at 1e-4 — `uniform_grid`'s rule, reproduced exactly so
  that the pair differs in layout and lookup and not in geometry. With the edge
  equal to the typical radius a query box spans three or four cells per axis,
  and the floor caps the grid at 257 cells per axis.
- **Block width, 8 slots**: one 256-bit vector of floats, the width GCC 13
  chooses for this loop with `-march=native` on this machine (confirmed with
  `-fopt-info-vec`, which reports the inner loop vectorised with 32-byte
  vectors). GCC's `-O2` cost model only vectorises a loop whose trip count is a
  known multiple of the vector width, which is why the width is fixed rather
  than the loop running to the end of the range. Not chosen from any workload.
- **Padding, 7 slots** at the end of each sorted array: a block that starts at
  the last entity reads seven slots past it, and the padding keeps those reads
  inside the allocation. Lanes beyond the range are masked by index, so whatever
  the padding holds is never accepted.
- **k-nearest start and limit**: the search starts at a half-width of one cell
  edge and stops widening past twice the largest extent, both inherited from
  `uniform_grid`'s `query_knn` without change.

## Falsifiable predictions

Preregistered before any measurement. All figures are median tick time unless
stated.

- **P1.** It beats `morton_sorted` on all ten spatial workloads. The rebuild is
  a counting sort instead of a comparison sort, and every cell lookup is an
  array read instead of a binary search. The one cost the parent does not pay
  is the pair of directory passes; the prediction is that they cost less than
  the comparison sort they replace even on `s02`, where the directory is
  largest.
- **P2.** It beats `uniform_grid` on `s01_steady_uniform`, `s03_wide_radius`,
  `s04_teleport` and `hs04_flat_world`. On each of these every entity moves
  every tick, and an incrementally maintained grid pays a relink — two unlinks
  and two links, each a dependent random access — for every mover that changes
  cell, and then chases a linked list through scattered memory on every query.
  Here a move is one array write, the rebuild is sequential passes plus one
  scatter, and a query reads contiguous runs.
- **P3.** It loses to `uniform_grid` on `hs02_rewind_few_moving` by at least
  1.5x. Only 2% of entities move per tick, but any move marks the index stale,
  so it rebuilds over all 30000 entities and every cell every tick, where
  `uniform_grid` touches the 2% that moved.
- **P4.** In the `spatial_move` sweep (a fixed number of moves per tick and a
  growing population) its fitted time exponent is at least 0.8 in both regimes,
  against `uniform_grid`'s ~0.1, because the rebuild is linear in the
  population. The risk to this is the directory term: in the `fixed_world`
  regime the cell count does not grow, so the two passes over the cells are a
  constant that flattens the small end of the curve, and the prediction holds
  there only if the entity passes outweigh it across most of the range.
- **P5.** Its `s04_teleport` time is within 5% of its `s01_steady_uniform`
  time. `s04` is `s01` with one field changed, and a rebuild cannot tell a jump
  from a step: the scatter's write pattern is set by id order, which is random
  with respect to position whether entities step or jump.

## What would falsify the design argument

Failing P2. That would mean a contiguous layout rebuilt every tick does not
beat an incrementally maintained linked grid even when everything moves, and
the rebuild-per-tick family has no regime in which it wins.
