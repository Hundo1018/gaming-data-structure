# morton_lbvh

## Hypothesis

A flat structure has no answer to clustering, because one cell size cannot be
right where the world is crowded and where it is empty at once. A hierarchy
over the parent's Morton order can be. Sixteen consecutive entities in Morton
order are a compact region whose size is set by how many entities are near each
other: about two units across inside one of `s02_dense_clustered`'s clumps,
about sixty in `s01_steady_uniform`'s even spread, as the side of a cube of the
same volume (the code's cells have the world's proportions, so a run is on
average about four times longer across than it is tall in those worlds). Boxes
over those runs, and boxes over groups of eight boxes, adapt to density with
nothing allocated per unit of world volume, and a radius query that prunes on
them hands the exact test a thin shell of extra entities instead of a grid
cell's whole content, in contiguous runs a vector loop can scan instead of a
linked list.

The parent kept the order and paid one binary search per cell to use it. This
keeps the order and replaces the search with the hierarchy, and replaces the
comparison sort with a radix sort, so the rebuild is a fixed number of linear
passes.

## What changed from the parent, and why it is structurally different

The assumption that changed is the unit of lookup. `morton_sorted` assumed it
was a fixed cell of the world, so a query had to name every cell it overlapped
and find each one. Here it is a run of sixteen entities, whose extent is
whatever those entities span, so a query names nothing: it descends from one
root and is led to the runs that matter by their boxes. The mutation operator is
`spatial_grouping`: consecutive runs are grouped into nodes, and nodes into
nodes. The 30-bit code here is 10 bits of position per axis, each axis divided
into 1024 steps of its own extent; the parent's was 21 bits of cell index per
axis, over cubic cells the size of the typical query radius. The parent's
members are private, so nothing of it is included.

## Representation

- **Authority.** `pos_` and `live_`, indexed by id, written by every mutation
  and read by `position_of`. A mutation sets a stale flag; the first query after
  it rebuilds everything below.
- **Code.** Each live entity's position is quantised to 1024 steps per axis over
  the world bounds: on each axis the step is `(v - min) * (1024 / extent)`
  truncated and clamped to 0..1023, and an axis of no extent maps every entity
  to step 0. The three 10-bit indices are interleaved into a 30-bit Morton code,
  so every axis contributes all ten of its bits. A code cell, and so on average
  the region a run of sixteen covers, has the world's proportions rather than a
  cube's: in the public worlds, which are four times wider than tall, a cell is
  four times longer in x and y than in z. A box of those proportions has a mean
  width (a+b+c)/2 about a fifth larger than a cube of the same volume (1.79
  against 1.5 times the cube's side), and the extra entities a sphere's edge
  admits grow with the mean width of the boxes it cuts, so this costs some
  over-admission against cubic cells, more in a flatter world. The code decides
  only the order: boxes are computed from the positions themselves, so no
  rounding in the quantisation can change an answer.
- **Sort.** Words `(code << 32) | id`, built in ascending id order, sorted by an
  LSD radix sort over the code's three 10-bit digits. Each pass is stable, so
  equal codes stay in id order and the result is deterministic. No comparison
  sort anywhere.
- **Columns.** The sorted ids and positions are gathered into four
  structure-of-arrays columns, x, y, z and id, each a plain array in sorted
  order whose start is 64-byte aligned, so slots `16j .. 16j+15` of every
  column, leaf j, are one cache line each. The last leaf's unused slots hold
  NaN coordinates, which fail every `<=` test, and the id `kNoEntity`.
- **Hierarchy.** Level 0 holds one box per leaf; each level above holds one
  box per eight boxes of the level below, up to a single root. Boxes are stored
  in groups of eight siblings, one row of eight floats per bound, so the
  children of node j at level l are exactly group j of level l-1. A lane past
  the end of a level holds the empty box (+inf to -inf), so a parent is the union
  of all eight lanes without asking which are real. A node's entity range,
  `[j * 16 * 8^l, (j+1) * 16 * 8^l)` clipped to the population, follows from its
  index and is not stored.

## Queries

- **Radius.** Depth first from the root. Each expanded node's eight child boxes
  are tested at once, and a child is pruned when its lower bound on squared
  distance exceeds `r*r`. Every entity of every leaf reached is tested with
  `dist2(p, c) <= r*r` using the substrate's `dist2`, including in leaves whose
  box lies wholly inside the sphere: there is no shortcut that skips the test.
  The digest is `RadiusDigest(c, r)`, which takes its salt from the query, and
  the salt is used for nothing else. `query_radius_of` subtracts the centre's
  own term when, and only when, the same accept test admits the centre.
- **The lower bound cannot over-estimate.** Per axis the gap is
  `max(lo - c, c - hi, 0)`, and the squared gaps are summed in the order `dist2`
  sums its terms. For any entity p inside a box, `lo <= p <= hi`; IEEE rounding is
  monotonic, so each rounded gap is no larger than the rounded `|p - c|` that
  `dist2` computes, each square is no larger, and each partial sum is no larger.
  The bound is therefore at most `dist2(p, c)` exactly as the substrate computes
  it, and pruning on it can never drop an entity the accept test would keep.
  The whole project is built with `-ffp-contract=off`, so neither side is fused.
  The order matters only as association: `(a+b)+c` with its terms commuted is
  the same float, but `a+(b+c)` can round above `(a+b)+c` and prune an entity
  lying exactly on the sphere.
- **k nearest.** Best first: a min-heap of nodes keyed by their lower bound, and
  a max-heap of at most k results ordered by `nearer`, so its front is the kth
  best. A node is pruned, and the search stops, only when its bound is strictly
  greater than the kth best's distance with k results held: a node at exactly
  that distance may hold an entity at the same distance with a smaller id, and
  is expanded. The answer is the oracle's `(dist2, id)` sequence, ties included.

## Constants, and why

| constant | value | reason |
|---|---|---|
| leaf size L | 16 | Sixteen floats are one 64-byte cache line, so a leaf is exactly one aligned line of each of the four columns and the distance loop over it is two 256-bit vectors per axis with no remainder. Scanning a leaf is about the same vector work as testing one node's eight child boxes (six rows of eight against three rows of sixteen), so a smaller leaf would add a level whose test costs what it saves, and a larger one would scan more entities outside the sphere. |
| fan-out F | 8 | Eight floats are one 256-bit vector, the width GCC uses for this machine under `-march=native` (its vectoriser reports 32-byte vectors for both loops). One node's eight child boxes are one 192-byte group of three lines, tested in one pass. The depth is then five levels at 20 000 entities and six at 128 000. |
| quantisation | 1024 steps per axis over the world bounds, 30-bit code | As specified. In the public worlds of 1024 x 1024 x 256 a step is one unit in x and y and a quarter unit in z, finer than a leaf even at the centre of an `s02` clump, where a unit cube holds on the order of ten entities. |
| radix digits | three of 10 bits | A 1024-bucket histogram is 4 KB, so all three fit in a 48 KB L1 together and are filled in the same pass that computes the codes. A digit every key shares is skipped. |
| traversal stack | 8 x 16 entries | Each expanded node leaves at most seven siblings waiting per level, and no tree exceeds eleven levels (2^32 ids over leaves of 16, divided by 8 per level). |

No constant was chosen by measuring a workload.

## Falsifiable predictions

Median tick time unless stated. Each is a comparison inside one run.

- **P1.** On `s02_dense_clustered`: a lower median tick than `uniform_grid`,
  **and** peak memory at most half of `uniform_grid`'s. Arithmetic, not
  measurement, puts its index at about 1.9 MB for 40 000 entities (positions
  and liveness 0.52 MB, sort words 0.64 MB, sorted columns 0.64 MB, boxes 0.07 MB),
  against the grid's 5.84 MB in the last report.
- **P2.** On `hs03_knn_heavy`: a lower median tick than `uniform_grid`, because a
  best-first search goes to the nearest boxes wherever they are, where the
  grid's widening box walks every empty cell in a sparse world.
- **P3.** On `s01_steady_uniform`: a median tick within 1.5x of `uniform_grid`'s.
  It may lose there: on even density a grid indexes its cell directly where
  this descends five levels and rebuilds everything every tick. Losing by more
  than 1.5x falsifies "competitive on even density".
- **P4.** A lower median tick than `morton_sorted` on all ten spatial workloads,
  public and hidden, including the two rewinding ones, where both are measured
  under snapshot-and-rebuild.
- **P5.** In the `spatial_move` sweep, in each regime, a time exponent of at
  least 0.8: the rebuild every tick is linear and outgrows the fixed 2000 moves.
  In `spatial_query_radius`, `fixed_density` regime, a time exponent of at most
  0.25: the depth grows as log n and the answer does not grow.
- **P6.** In the `fixed_world` regime of every sweep family (`spatial_move`,
  `spatial_query_radius`, `spatial_knn`), a memory exponent of at least 0.9:
  nothing it allocates is proportional to world volume.

## What would falsify it

Failing P1 on time. That would mean the clustered workload's cost is the answer
itself plus the grid's over-admission, and that adapting to density buys
nothing a flat grid lacks. Failing P1 on memory alone would falsify the claim
that memory follows the population, not the hierarchy argument; failing P3
alone would bound where the hierarchy is worth having, not refute it.
