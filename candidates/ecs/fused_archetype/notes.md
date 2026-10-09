# fused_archetype — observed

Run `20261009T042136Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 5 repetitions. Verdicts are
`benchmarks/predictions.md`'s; step times there are the median across
repetitions.

## The second pass is not a material share of the frame

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | `w02_query_heavy` at most 0.95x `archetype` | 0.983x | **falsified** |
| P2 | `w01_steady_uniform` at most 0.95x `archetype` | 0.995x | **falsified** (within noise) |
| P3 | no original ECS workload over 1.05x `archetype` | at most 0.996x | held |

Failing both P1 and P2 was the stated falsification: the pass over the Position
and Velocity columns that fusing removes is not a material share of these
frames. It is now measured from both sides. The reviewer, with the candidate's
own code and the mechanism switched off, measured the fusion alone at 1.00x on
`w01` and `w02` over 15 to 21 interleaved rounds, and the review led to the
reproduction being made the parent's code verbatim wherever the mechanism does
not need a change. The suite run agrees: 0.98x to 1.00x.

A likely reason, not measured: a frame's query digests every matching entity
under a fresh salt, several hash mixes per component per entity, while the
integrate pass it would share a walk with is three multiply-adds. Removing a pass
over memory saves little if the pass that remains is bound by that arithmetic
rather than by memory. The cache and cycle counters that would settle it are
unavailable in this container.

## Where it stands

On eight Pareto fronts, usually as a near-tie with `archetype`: 0.93x to 1.00x
on the ten original workloads, the widest gap 0.93x on `h05_sparse_component`.
On `w06_point_narrow`, which never integrates, it is 1.02x `archetype`: there the
mechanism is never engaged and only the pending-step test in each point
operation differs.

## Historian

Classified **KNOWN_VARIANT** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said KNOWN_COMPONENTS_NEW_COMBINATION
(medium confidence); the skeptic refuted it and proposed KNOWN_VARIANT, which is
adopted as the stricter class.

Deferred execution resolved on observation, with the deferred step fused into
the next pass over the same data, is lazy evaluation with loop fusion
(LazyTensor, Weld, Bohrium); the skeptic found ECS system fusion in an existing
library, which makes it a known variant rather than a new combination.

Closest known work, as cited (sources the agents report having read):

- **Clock Scan (Crescando storage engine).** P. Unterbrunner, G. Giannikis, G.
  Alonso, D. Fauser, D. Kossmann, "Predictable Performance for Unpredictable
  Workloads", PVLDB 2(1):706-717, 2009. DOI 10.14778/1687627.1687707.
  https://vldb.org/pvldb/vol2/vldb09-323.pdf
- **LazyTensor (deferred execution until observation).** A. Suhan, D. Libenzi,
  A. Zhang, P. Schuh, B. Saeta, J. Y. Sohn, D. Shabalin, "LazyTensor: combining
  eager execution with domain-specific compilers", arXiv:2102.13267, 2021.
- **Weld (lazy evaluation across library calls with loop fusion).** S. Palkar,
  J. Thomas, A. Shanbhag, D. Narayanan, H. Pirk, M. Schwarzkopf, S. Amarasinghe,
  M. Zaharia, "Weld: A Common Runtime for High Performance Data Analytics", CIDR
  2017. https://commit.csail.mit.edu/papers/2017/cidr_weld.pdf
- **Halide producer-consumer fusion at tile granularity (compute_at).** J.
  Ragan-Kelley, C. Barnes, A. Adams, S. Paris, F. Durand, S. Amarasinghe,
  "Halide: A Language and Compiler for Optimizing Parallelism, Locality, and
  Recomputation in Image Processing Pipelines", PLDI 2013, pp. 519-530.
  https://people.csail.mit.edu/jrk/halide-pldi13.pdf
- **SubzeroECS v2 system fusion (runFused: one pass per partition, per-partition
  subset dispatch, Tiled<N> executor)** (found by the skeptic). C. Hutchinson,
  SubzeroECS (Sub0ECS), GitHub https://github.com/CraigHutchinson/Sub0ECS. PR
  #4, "SubzeroECS v2: query-partition store, system fusion, new project layout"
  (commits from 2026-09-30, merged 2026-10-05). Design notes
  docs/research/fusion.md and docs/research/fusion-extension-points.md,
  docs/FINDINGS.md section "Small systems and fusion", code
  include/sub0ecs/store/world.hpp (runFused, dispatchSubset, fusedLoop) and
  include/sub0ecs/fusion/executors/tiled.hpp.
- **SubzeroECS split-phase executor design: lazy write-back with host access
  paths that sync first** (found by the skeptic). C. Hutchinson, SubzeroECS,
  docs/research/executor-async.md, sections 3, 4 and 6 (question E3), in the
  same repository; it cites Kokkos DualView (modify/sync/need_sync) as its
  closest prior art.

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
