# aos — observed

Run `20260902T092528Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -march=native`, 5 repetitions, median repetition reported.

## Half the hypothesis was tested; half was not

The prediction that narrow iteration would punish `aos` held everywhere:
`w02_query_heavy` 4845.1 us against `soa`'s 4004.7 us, at twice the footprint
(10.50 MB against 5.50 MB).

The other half — that `aos` is within noise of a split layout when access is to
whole entities — **was not tested by any workload in the suite**, and the
workload named `w04_random_access` does not test it either. The generator emits
one randomly chosen component per `Get` or `Set`, so a point access reads 12
bytes and `aos` loads a 56-byte record to do it. That is the case `aos` is
worst at, not the case it is designed for, and it duly lost:

| candidate | w04_random_access p50 |
|---|---:|
| `archetype` | 3193.6 us |
| `soa` | 3451.6 us |
| `aos` | 4258.4 us |

This is a gap in the workload set, not evidence against the layout. No workload
here reads several components of one entity in one operation, which is the
access pattern of most gameplay code touching an entity. Until such a workload
exists, `aos` has one untested claim on the record.

## Where it wins

`w05_small_world`, 2000 entities, 65.7 us against `archetype`'s 79.5 us. At
that size nothing is bandwidth-bound and the winner is whichever structure does
the least bookkeeping per operation, which is `aos`.
