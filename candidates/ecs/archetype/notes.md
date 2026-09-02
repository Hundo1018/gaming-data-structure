# archetype — observed

Run `20260902T092528Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -march=native`, 5 repetitions, median repetition reported.

## The prediction was falsified

`hypothesis.md` predicted that `archetype` would lose the structural-churn
workload to `sparse_set`, because add and remove copy the whole entity into
another group while a sparse set touches one component's arrays.

It won it:

| candidate | w03_structural_churn p50 |
|---|---:|
| `archetype` | 278.0 us |
| `sparse_set` | 355.6 us |

`w03` runs 3000 ops per frame at `w_add: 4.0, w_remove: 4.0` against 30000 to
60000 entities, so structural change is the dominant operation and the workload
is doing what it was built to do. The copy is real; it is simply cheaper than
expected at this component width. An entity here is at most 36 bytes of
component data across four columns, and the destination row is the end of a
column that iteration has just walked. `sparse_set` avoids the copy but pays a
swap-erase in one dense array plus two sparse writes, and it pays them into
memory that nothing else in the frame has touched.

The prediction should be re-tested at a component width where the copy cannot
stay in cache. That is a workload the suite does not currently contain: the
component set is fixed at four small types.

## Where it stands

Fastest median frame time on 9 of 10 workloads. The exception is
`w05_small_world` (2000 entities, cache-resident), where `aos` wins at 65.7 us
against 79.5 us — at that size no layout is paying for memory traffic and the
remaining difference is bookkeeping, of which `archetype` has the most.

## Caveat that limits the result

Group lookup is a sixteen-entry direct table because there are four component
types. A production archetype store hashes an unbounded component set. This
implementation therefore understates the lookup cost, and the margin above
should not be read as a measurement of archetype ECS libraries.
