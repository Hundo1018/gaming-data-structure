# cell_rows — observed

Run `20261009T042136Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 5 repetitions. Verdicts are
`benchmarks/predictions.md`'s.

## Two of three held

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | no workload slower than `cell_sorted` by over 3% | at most 1.023x (`hs08`) | held |
| P2 | `hs06_mixed_reach` at most 0.9x `cell_sorted` | 0.967x | **falsified** |
| P3 | `hs03_knn_heavy` at most 0.9x `cell_sorted` | 0.715x | held |

These predictions were written after `cell_sorted`'s reviewer had measured the
row walk on public workloads, so they were placed only on held-out ones.

P1 rested on one workload for a while. The harness reports a run's metrics from
the repetition with the median total time, and on `hs07_crowd` that repetition's
own median tick was the highest of five (3444, 3377, 3383, 3487, 3718 us). From
that repetition the candidate looked 1.11x `cell_sorted`; from the median of its
five repetitions it is 1.006x. The judge now uses the latter for every step
percentile, a change that also turned one of `delta_grid`'s predictions from
held to falsified.

## Long rows pay; mixed reach does not make rows long

P3 is where the row walk was expected to matter most, and it did: a k-nearest
search at k = 64 in a sparse world widens its box over many cells of each row,
and reading each row as one run took `hs03` from 5323.7 us to 3832.3 us.

P2 assumed the large queries on `hs06_mixed_reach` would do the same. The cell
edge there is 65, set by the mean reach, so a query of radius 128 spans at most
five cells per row and most queries span one or two. The box is long in cells
only where cells are small against the query, and on `hs06` the cells were sized
to make them comparable.

## Where it stands

On seven Pareto fronts, more than any other spatial candidate except
`grid_ring_knn` and `morton_lbvh`. Its generalization gap is +1.55 ranks: it does
better on the public workloads, mostly because the new held-out crowd and
rollback workloads favour `morton_lbvh` and `delta_grid`, not because of
tuning (no constant was chosen here; the change is the walk alone).

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said EXACT_REDISCOVERY (high confidence);
the skeptic upheld it.

Reading a row of a cell-sorted grid as one range is DualSPHysics's 'simplified
neighbour search' (27 cells as 9 ranges).

Closest known work, as cited (sources the agents report having read):

- **DualSPHysics 'simplified neighbour search': ranges of consecutive cells
  along X.** J.M. Domínguez, A.J.C. Crespo, M. Gómez-Gesteira, 'Optimization
  strategies for CPU and GPU implementations of a smoothed particle
  hydrodynamics method', Computer Physics Communications 184(3):617-627, 2013.
  Preprint: 'Optimization strategies for parallel CPU and GPU implementations of
  a meshfree particle method', arXiv:1110.3711 (Section 4.4 and Fig. 10, read
  via alphaXiv).
- **DualSPHysics source code: JCellSearch_inline.h ParticleRange,
  JSimpleNeigs::NearbyPositions, JCellDivCpuSingle.** DualSPHysics open-source
  SPH solver (LGPL), https://github.com/DualSPHysics/DualSPHysics, files
  src/source/JCellSearch_inline.h, src/source/JSimpleNeigs.{h,cpp},
  src/source/JCellDivCpuSingle.cpp (read directly). Project paper: J.M.
  Domínguez et al., 'DualSPHysics: from fluid dynamics to multiphysics
  problems', Computational Particle Mechanics, 2022, arXiv:2104.00537.
- **Sorted-by-cell grid with a cellStart/cellEnd directory (the parent
  cell_sorted's structure).** S. Green, 'Particle Simulation using CUDA', NVIDIA
  whitepaper in the CUDA SDK, 2010 (cited as ref. [32] in arXiv:1110.3711). R.C.
  Hoetzlein, 'Fast Fixed-Radius Nearest Neighbors: Interactive Million-Particle
  Fluids', NVIDIA GPU Technology Conference 2014, session S4117, slides
  https://ramakarl.com/pdfs/2014_Hoetzlein_Fast_Neighbors.pdf (read: 'Fluids v.3
  (Counting Sort)', 'AtomicAdd for Bin Counts & Indices', 'Bin Prefix Sum',
  'Counting Sort (Prefix sum + Copy)').
- **Runs per query region under a linearisation (database clustering
  analysis).** H.V. Jagadish, 'Linear clustering of objects with multiple
  attributes', ACM SIGMOD 1990, pp. 332-342, doi:10.1145/93597.98742. B. Moon,
  H.V. Jagadish, C. Faloutsos, J.H. Saltz, 'Analysis of the clustering
  properties of the Hilbert space-filling curve', IEEE TKDE 13(1):124-141, 2001.
- **DualSPHysics JSimpleNeigs::NearbyPositions / NearbyPositionsLt (re-verified,
  still the closest match)** (found by the skeptic).
  https://github.com/DualSPHysics/DualSPHysics, src/source/JSimpleNeigs.h and
  JSimpleNeigs.cpp (class added 15-09-2018 per its changelog), read directly
- **Domínguez, Crespo, Gómez-Gesteira, Section 4.4 'Simplifying the neighbor
  search' (re-verified)** (found by the skeptic). arXiv:1110.3711v3, pp. 9-11
  and Figs. 8, 10, 11; published as Computer Physics Communications
  184(3):617-627, 2013

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
