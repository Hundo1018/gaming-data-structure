# archetype — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median frame time unless stated.

## The prediction was falsified, and stays falsified

`hypothesis.md` predicted `archetype` would lose the structural-churn
workload to `sparse_set`, because add and remove copy the whole entity into
another group while a sparse set touches one component's arrays.

It won it again, by a wider margin than in the earlier run:

| candidate | w03_structural_churn |
|---|---:|
| `archetype` | 398.0 us |
| `aos` | 546.7 us |
| `soa` | 560.0 us |
| `sparse_set` | 587.5 us |

`w03` runs 3000 operations per frame at `w_add: 4.0, w_remove: 4.0` against
30000 to 60000 entities, so structural change is the dominant operation and the
workload is doing what it was built to do. The copy is real; it is simply
cheaper than expected at this component width. An entity here is at most 36
bytes across four columns, and the destination row is the end of a column that
iteration has just walked.

The prediction should be re-tested at a component width where the copy cannot
stay in cache. That workload does not exist: the component set is fixed at four
small types.

## Where it stands

Fastest median frame time on 8 of the 10 ECS workloads. The exceptions are
`w04_random_access`, where all four candidates are within 2.7% and the
workload separates nothing, and `w05_small_world` at 2000 entities, where it
is last of the four real candidates at 113.3 us against `aos`'s 101.6 us: at
that size no layout is paying for memory traffic and `archetype` has the most
bookkeeping per operation.

Its clearest win is `h05_sparse_component` at 562.2 us against
`sparse_set`'s 1242.7 us and `aos`'s 1825.8 us — a query naming a component
2% of entities hold, answered by walking only the groups that carry it.

## Caveat that limits the result

Group lookup is a sixteen-entry direct table because there are four component
types. A production archetype store hashes an unbounded component set. This
implementation understates lookup cost, and the margins above should not be read
as a measurement of archetype ECS libraries.
