# bitset_soa — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s; step times there are the
median across repetitions.

## Two of three held, and the falsified one was too cautious

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | `h05_sparse_component` at most 0.6x `soa` | 0.562x | held |
| P2 | `w01`, `w02`, `w04` within 10% of `soa` either way | 0.807x, 0.873x, 0.901x | **falsified** |
| P3 | peak memory within 5% above `soa` everywhere | 1.007x | held |

P2 assumed that with dense masks almost every word is non-zero and the bitset
scan does the same work as `soa`'s mask test. It is faster than that on two of
the three: 19% on `w01` and 13% on `w02`. Few words are skipped there, so the
saving is not skipped work. The likelier cause is a branch: `soa` tests each slot
with a data-dependent branch, and with `p_velocity` at 0.8 about one slot in five
fails a Position+Velocity test at random, which a predictor cannot learn; walking
set bits with countr_zero has no branch per slot. That is a hypothesis about the
cause, not a measurement of it: the branch-miss counter is exactly what
`perf_event_open` would give and this container does not expose it.

## The generalist of the ECS track

On the Pareto front of 11 of the 12 ECS workloads, more than any other candidate
in either track; `soa` is on 10. On `w03_structural_churn` it has the lowest
median of every candidate, 268.1 us against `archetype`'s 343.9 us: structural
change on a flat index space is setting a byte and a bit, where `archetype` copies
the entity into another group. On the iteration-heavy workloads it is level with
`archetype` (1.00x on `w02`, 1.03x on `w01`) at `soa`'s footprint.

It is not where `archetype` is strongest: `h05_sparse_component` is 1.48x
`archetype`. A query naming a component 2% of entities hold still walks 200000
slots' worth of words, 3125 of them, and visits each set bit; `archetype` walks
only the groups that hold the component.

## Scaling

Point operations measure n^0.084 and the realistic frame n^0.681, the lowest
realistic-frame exponent of the ECS track in this run (`archetype` n^0.717). Its
memory exponent is `soa`'s, n^0.856 and n^0.821: five bits a slot do not show.
