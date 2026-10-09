# morton_lbvh — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s, computed by
`runner/predictions.py`; step times there are the median across repetitions.

## All ten predictions held

| | prediction | measured |
|---|---|---:|
| P1a | `s02_dense_clustered` median below `uniform_grid`'s | 0.574x |
| P1b | `s02` peak memory at most half of `uniform_grid`'s | 0.306x |
| P2 | `hs03_knn_heavy` median below `uniform_grid`'s | 0.522x |
| P3 | `s01_steady_uniform` within 1.5x of `uniform_grid` | 1.039x |
| P4 | below `morton_sorted` on all ten original workloads | 0.09x to 0.38x |
| P5a | `spatial_move` time exponent at least 0.8 | 0.931, 0.920 |
| P5b | `spatial_query_radius` fixed-density exponent at most 0.25 | 0.182 |
| P6a-c | fixed-world memory exponent at least 0.9 | 0.999, 0.999, 0.994 |

## The gap the README left open

`s02_dense_clustered` was kept in the suite to state a case no flat structure
answered: eight clumps holding 40000 entities, with queries aimed into them. It
is the first workload this population has been asked to answer with a hierarchy:

| candidate | s02 median tick | peak |
|---|---:|---:|
| `morton_lbvh` | 2812.0 us | 1.79 MB |
| `cell_rows` | 3330.1 us | 6.15 MB |
| `cell_sorted` | 3655.8 us | 6.15 MB |
| `uniform_grid` | 5060.7 us | 5.84 MB |
| `spatial_hash` | 4976.0 us | 1.29 MB |

It is the only candidate on `s02`'s Pareto front for time, and `spatial_hash` is
on it for memory alone. The floor tool measures `s02`'s irreducible tick at
1403.6 us, of which 818.7 us is digesting the roughly 1000 entities in each
answer, so `morton_lbvh` sits at 2.0x the floor where `uniform_grid` sits at
3.6x.

The result carried to two held-out workloads written after it was designed:
`hs07_crowd`, the same clumps travelling at three units a tick (3001.7 us
against `uniform_grid`'s 5211.0 us, 1.79 MB against 5.84 MB), and
`hs08_crowd_rollback`, the crowd with churn and twelve-tick rollbacks under
snapshot-and-rebuild (2560.0 us, the lowest median of every candidate,
`delta_grid` included). Its generalization gap is -0.2: it ranks no worse on
what it could not see.

## Where it loses

A rebuild every tick is linear in the population, which P5a predicted and the
sweep confirms. So it loses wherever little moves or little is asked:
`hs02_rewind_few_moving` at 4.8x `uniform_grid`, `hs05_spawn_churn` at 1.15x.
And `hs04_flat_world` at 1.46x: the world is 1024 by 1024 by 8, the Morton code
spends ten bits on an axis eight units deep, and the 30-bit code's ordering
along the two axes that matter is coarser than it looks. That reading is a
guess from the geometry and has not been tested.

## What it says

Density adaptation is worth having where density is the problem, and nowhere
else. On the even worlds it is level with the grid (1.04x on `s01`) and well
behind `cell_sorted`, which rebuilds the same way into flat cells. The pair
`morton_lbvh` / `cell_sorted` is the cleanest available test of
hierarchy-against-flat at equal maintenance strategy: both rebuild per tick
from a sort, and they part exactly where the clumps are.

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said KNOWN_VARIANT (high confidence); the
skeptic refuted it and proposed EXACT_REDISCOVERY, which is adopted as the
stricter class.

The Historian placed it as a variant of the Morton-packed R-tree (Kamel and
Faloutsos 1993; Qi et al. 2018); the skeptic found a library structure that
matches it on every axis the Historian used to separate them, so the stricter
class is adopted.

Closest known work, as cited (sources the agents report having read):

- **Z-order (Morton) packed R-tree: sort points by Z-order, pack every B
  consecutive points into a leaf, pack every B consecutive nodes into a parent,
  bottom-up.** Jianzhong Qi, Yufei Tao, Yanchuan Chang, Rui Zhang,
  'Theoretically Optimal and Empirically Efficient R-trees with Strong
  Parallelizability', PVLDB 11(5):621-634, 2018
  (https://www.vldb.org/pvldb/vol11/p621-qi.pdf); journal version 'Packing
  R-trees with Space-filling Curves: Theoretical Optimality, Empirical
  Efficiency, and Bulk-loading Parallelizability', ACM TODS 2020
- **Packed Hilbert R-tree (sort-based bottom-up R-tree packing).** Ibrahim
  Kamel, Christos Faloutsos, 'On Packing R-trees', Proc. CIKM 1993, pp. 490-499
  (UMD tech report 'Packed R-trees Using Fractals', 1992); predecessor: N.
  Roussopoulos, D. Leifker, 'Direct Spatial Search on Pictorial Databases Using
  Packed R-trees', SIGMOD 1985
- **Flatbush (static packed Hilbert R-tree in flat arrays).** Vladimir Agafonkin
  (mourner), Flatbush, JavaScript library, https://github.com/mourner/flatbush
  (source read: index.js)
- **packed_spatial_index (Rust crate: packed static Hilbert R-tree for 2D/3D
  AABBs with SoA SIMD search).** Filyus, packed_spatial_index crate,
  https://docs.rs/crate/packed_spatial_index/latest, source
  https://github.com/Filyus/packed_spatial_index (only the docs.rs page was
  read; node size and exact layout not verified)
- **EBGeometry PackedBVH, direct space-filling-curve constructor
  PackedBVH<T,P,K>(primsAndBVs, targetLeafSize), with ChildAABBSoA SIMD node
  boxes and pruneTraverse()** (found by the skeptic). Robert Marskar (SINTEF),
  EBGeometry C++ library, https://github.com/rmrsk/EBGeometry. Read:
  Source/EBGeometry_BVHImplem.hpp (constructor body), Source/EBGeometry_BVH.hpp
  (DefaultBranchingRatio, ChildAABBSoA), Source/EBGeometry_SFC.hpp and
  SFCImplem.hpp (computeBins), Docs/Sphinx/source/ImplemBVH.rst and
  SIMDClasses.rst. Direct builder added in commit 30b8809, 2026-07-11 (PR #106).
  SIMD support in 19cda54, 2026-07-08 (#88). pruneTraverse in 890b78f,
  2026-07-09 (#95). All three predate the candidate (2026-10-08).
- **EBGeometry TreeBVH::bottomUpSortAndPartition<SFC::Morton>()** (found by the
  skeptic). Robert Marskar, EBGeometry, commit 7446d4c 'Add SFC-based bottom-up
  construction of BVHs (#67)', 2024-08-26 (read via git show)

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
