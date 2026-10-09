# grid_ring_knn — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s.

## Four of five held

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | `hs03_knn_heavy` at most 0.5x `uniform_grid` | 0.386x | held |
| P2 | `s05_small_world` at most 0.7x | 0.655x | held |
| P3 | no original workload slower than `uniform_grid` by over 10% | at most 1.018x | held |
| P4a | `spatial_knn` fixed world, n = 1000, at least 3x faster | 1.61x faster (0.620) | **falsified** |
| P4b | `spatial_knn` fixed-density exponent no higher than the grid's | -0.013 difference | held |

## The search order is where the time went

This candidate is `uniform_grid` with one function replaced, so the falsifying
case for the argument was P1 failing. It did not: on `hs03_knn_heavy` the median
tick fell from 5678.2 us to 2196.9 us, the largest single-variable gain in the
population. The parent's widening search re-walks every cell of each larger box
and partial-sorts everything it gathered; visiting cells in order of their
smallest possible distance and stopping on a proven bound does neither.

The floor tool puts `hs03` at 5.8x the irreducible tick for this candidate,
against 15.1x for its parent in the same run. It also took `hs02_rewind_few_moving`,
a rewind workload, to 0.71x the parent: twelve k-nearest queries a tick there
are a larger share of a tick in which almost nothing moves.

## The sparse end did not move as far as predicted

P4a asked for 3x at the sparsest point of the k-nearest sweep, 1000 entities in
the full world, where the parent doubles its box the most times. It measured
1.61x. The doubling the parent does there is over nearly empty cells, and so is
the ring walk: in a world that sparse, the proven stopping distance is many
cells out whichever order the cells are visited in. What the ring order removes
is the repeated walking, and at that density there is less of it to remove than
the prediction assumed.

## What it leaves

Its generalization gap is +1.3 ranks: it ranks worse on the held-out workloads,
because the new held-out crowd workloads are not k-nearest-heavy and its
gain there is small. That is the expected shape for a candidate that changed one
operation; it is recorded because the report computes it, not because it shows
tuning. A natural next mutation is the same search order in `cell_sorted` or
`morton_lbvh`, whose k-nearest searches still widen a box.

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said EXACT_REDISCOVERY (medium confidence);
the skeptic upheld it.

A precomputed table of cell offsets ordered by their minimum distance, walked
with an early stop once the k-th distance is proven, is Brodu's spherical
indexing (2006) and the cell technique of Bentley, Weide and Yao (1980) before
it.

Closest known work, as cited (sources the agents report having read):

- **Spherical indexing: a precomputed cell-offset table sorted by the minimum
  inter-cell distance, used for k-NN with early cutoff.** Nicolas Brodu,
  "Spherical Indexing for Neighborhood Queries", arXiv:cs/0608108 (cs.DS/cs.CG),
  2006. I found no journal or conference version.
  https://arxiv.org/abs/cs/0608108
- **NEARPT3: a uniform grid with a compile-time-generated, distance-sorted table
  of cells in one symmetric sector, reflected at query time.** W. Randolph
  Franklin, "Nearest Point Query on 184,088,599 Points in E^3 with a Uniform
  Grid", manuscript dated 2006; its header is an IEEE TVCG template, and I could
  not confirm a venue. https://wrfranklin.org/p/105-nearpt3.pdf ; code:
  http://wrfranklin.org/Research/nearpt3/
- **Spiral search (cell technique) for nearest-neighbour searching.** Jon L.
  Bentley, Bruce W. Weide, Andrew C. Yao, "Optimal Expected-Time Algorithms for
  Closest Point Problems", ACM Transactions on Mathematical Software
  6(4):563-580, 1980. Related: John G. Cleary, "Analysis of an Algorithm for
  Finding Nearest Neighbors in Euclidean Space", ACM TOMS 5(2):183-192, 1979.
- **CircularTrip: grid k-NN visiting cells in ascending mindist(cell, q).**
  Muhammad Aamir Cheema, Yidong Yuan, Xuemin Lin, "CircularTrip: An Effective
  Algorithm for Continuous kNN Queries", DASFAA 2007, LNCS 4443, pp. 863-869,
  Springer. https://cgi.cse.unsw.edu.au/~lxue/paper/DASFAA07.pdf
- **Elias's algorithm (bucket best-match search in increasing distance order
  with stop when the distance index exceeds the best found)** (found by the
  skeptic). R. L. Rivest, "On the Optimality of Elias's Algorithm for Performing
  Best-Match Searches", Information Processing 74 (IFIP Congress, Stockholm),
  North-Holland, pp. 678-681, 1974.
- **CircularTrip, grid k-NN in ascending mindist(c,q)** (found by the skeptic).
  M. A. Cheema, Y. Yuan, X. Lin, "CircularTrip: An Effective Algorithm for
  Continuous kNN Queries", DASFAA 2007, LNCS 4443 (PDF at
  https://cgi.cse.unsw.edu.au/~lxue/paper/DASFAA07.pdf; I read its text).

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
