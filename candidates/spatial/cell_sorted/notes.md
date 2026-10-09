# cell_sorted — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s.

## Four of five held; the design argument survived

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | below `morton_sorted` on all ten original workloads | 0.16x to 0.41x | held |
| P2 | below `uniform_grid` where everything moves | 0.66x to 0.91x | held |
| P3 | at least 1.5x slower than `uniform_grid` on `hs02` | 4.47x | held |
| P4 | `spatial_move` exponent at least 0.8, both regimes | 1.011 fixed density, **0.585 fixed world** | **falsified** |
| P5 | `s04_teleport` within 5% of its own `s01` | 1.045 | held (within noise) |

P2 was the falsifying case named before measurement. It held on all four
workloads: `s01` 0.91x, `s03` 0.66x, `s04` 0.79x, `hs04` 0.85x of
`uniform_grid`. A contiguous array rebuilt by counting sort every tick beats the
incrementally maintained linked grid wherever every entity moves every tick.
`morton_sorted`'s notes said its layout argument had not been given its best
case; this is that case, and the argument holds once the sort is a counting sort
and the lookup is a directory.

## P4: the directory is a constant the population does not touch

At fixed density the move family's exponent is 1.011: the rebuild is linear in
the population, as claimed. At fixed world it is 0.585. The rebuild clears and
sums a directory of one entry per cell, 162712 of them in that world, and with
the world held still that cost does not grow at all; at small populations it is
most of the rebuild, and the curve is flat at the bottom. The implementer
recorded this risk in `hypothesis.md` before any measurement and left the
threshold where it was. The claim was about the rebuild's growth with
population, and the directory is the one part of the rebuild that does not grow
with it.

## Where it stands

On the Pareto front of four workloads. It loses wherever little moves:
`hs02_rewind_few_moving` at 4.5x the grid and `hs05_spawn_churn` at 1.21x. Its
child `cell_rows`, which reads each row of a query box as one run, is faster on
eleven of the thirteen workloads, by under 1% on two of them (`hs01`, `s03`);
see that candidate's notes.

The comparison stays clean because of a review finding. The first version read
each row of the box as one run, which `uniform_grid` cannot do, and the reviewer
measured that change as about half of the margin over the grid on `s01`. The
walk was put back to one range per cell, as the grid reads its list heads, and
the row walk became its own candidate.

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said EXACT_REDISCOVERY (high confidence);
the skeptic upheld it.

The counting-sort build with a cells+1 directory, the reverse placement pass and
the rebuild-every-frame motivation are all in Lagae and Dutré (2008), and the
move from a Morton or radix-sorted key to a counting sort on the exact bin is in
Hoetzlein (2014). What this repository adds is the measurement against an
incremental linked grid under rollback and churn.

Closest known work, as cited (sources the agents report having read):

- **Compact grid (counting-sort grid with a cells+1 offset array).** Ares Lagae
  and Philip Dutré, "Compact, Fast and Robust Grids for Ray Tracing", Computer
  Graphics Forum 27(4):1235-1244 (Eurographics Symposium on Rendering 2008),
  doi:10.1111/j.1467-8659.2008.01262.x; text read from
  https://cgweb.informatik.uni-freiburg.de/intern/seminar/dataStructures_Lagae%20-%20Grids%20-%202008.pdf
- **Sort-based uniform grid for particles (CUDA 'particles' sample).** Simon
  Green, "Particle Simulation using CUDA", NVIDIA CUDA SDK whitepaper, v1.0 Sept
  2007, v1.3 May 2010;
  https://developer.download.nvidia.com/assets/cuda/files/particles.pdf
- **Fluids v3 counting-sort fixed-radius neighbour search.** Rama C. Hoetzlein
  (NVIDIA), "Fast Fixed-Radius Nearest Neighbors: Interactive Million-Particle
  Fluids", talk slides, GPU Technology Conference 2014;
  https://ramakarl.com/pdfs/2014_Hoetzlein_Fast_Neighbors.pdf ; open-source
  implementation Fluids v3, http://fluids3.com (zlib licence)
- **Index sort (and its Z-index sort refinement).** Markus Ihmsen, Nadir Akinci,
  Markus Becker, Matthias Teschner, "A Parallel SPH Implementation on Multi-Core
  CPUs", Computer Graphics Forum 30(1):99-112, 2011;
  https://cg.informatik.uni-freiburg.de/publications/2011_CGF_dataStructuresSPH.pdf
- **Fluids v3 source: insertParticles + prefix sum + countingSortFull deep
  copy** (found by the skeptic). Rama C. Hoetzlein, Fluids v3.0 (2012) source,
  github.com/ramakarl/fluids3, files fluids3.0/fluids/fluid_system_kern.cu
  (kernels insertParticles, countingSortFull) and HISTORY.txt
- **Weak counting-sort reordering of positions into slabs for CPU cache locality
  (molecular dynamics)** (found by the skeptic). Zhenhua Yao, Jian-Sheng Wang,
  Gui-Rong Liu, Min Cheng, "Improved neighbor list algorithm in molecular
  simulations using cell decomposition and data sorting method", Computer
  Physics Communications 161(1-2):27-35, 2004; arXiv:physics/0311055 (Algorithm
  1)

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
