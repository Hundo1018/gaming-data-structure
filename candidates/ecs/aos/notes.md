# aos — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median frame time unless stated.

## The claim this candidate exists to test is still untested

`aos` is built for code that touches several components of one entity at once.
No workload in the suite does that: the generator names one randomly chosen
component per `Get` or `Set`, so a point access reads 12 bytes and `aos`
loads a 56-byte record to do it.

`w04_random_access` is the workload that sounds like the test and is not. All
four real candidates finish within 2.7% of each other there
(`soa` 6400.8 us, `aos` 6489.6 us, `archetype` 6570.6 us,
`sparse_set` 7341.2 us), which is inside the repetition spread of 7.3% to 7.8%
on that workload — it separates nothing.

This is a gap in the workload set, not evidence about the layout, and it is
recorded in `workloads/README.md` rather than left implicit.

## Where it wins

`w05_small_world`, 2000 entities: 101.6 us, first, ahead of `sparse_set` at
104.2 us and `archetype` at 113.3 us. At that size nothing is bandwidth-bound
and the winner is whichever structure does the least bookkeeping per operation.

## What it pays

Twice the footprint of `soa` on every workload, for the reason the layout
implies: space for all four components per slot whether or not the entity holds
them. That difference is stable across runs; the speed difference between the
two is not — see `candidates/ecs/soa/notes.md`.

## Scaling (run `sweep-20260904T081902Z`)

Its `O(1)` claim for get and set is confirmed once the linear passes are taken
out of the frame: **n^0.114** with integrate and the per-frame query switched
off, against n^0.794 with them on. Both numbers cover the same operations; the
difference is entirely the two full passes over the population that a normal
frame also contains.

That gap is why the point-operation family exists. The first version of this
sweep measured only the realistic frame, and every ECS candidate came out
between n^0.70 and n^0.81 — which describes the passes, not the structure.
