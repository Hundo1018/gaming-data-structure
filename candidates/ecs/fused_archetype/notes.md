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
