# sparse_set — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median frame time unless stated.

## The prediction was falsified, less badly than before

`hypothesis.md` predicted `sparse_set` would win `h05_sparse_component`,
where `Health` is held by about 2% of 200000 entities and both queries name
it. Driving iteration from the rarest set should have made the query cost about
4000 entities instead of 200000.

It came second:

| candidate | h05_sparse_component |
|---|---:|
| `archetype` | 562.2 us |
| `sparse_set` | 1242.7 us |
| `soa` | 1604.3 us |
| `aos` | 1825.8 us |

The driving-set choice works; it is not where the frame goes. Two costs the
hypothesis did not account for:

1. `integrate` walks the `Velocity` dense array — about 190000 entities,
   since `p_velocity` is 0.95 — and reaches `Position` through the sparse
   array for each one. That is a dependent random load per entity into a 200000
   entry index. The other three candidates walk position and velocity in step.
2. The multi-component query probes the non-driving sets in the driving set's
   dense order, which is unrelated to how those sets are stored.

Both are properties of the plain sparse set, not of the implementation, and both
are what EnTT-style owning groups exist to remove by keeping a group's dense
arrays aligned. That is not implemented here, so this result describes the plain
sparse set and says nothing about grouped sparse sets.

## Where it holds up

`h01_zipf_hotspot` at 3415.0 us, second behind `archetype` and ahead of both
`aos` and `soa`. A heavy head keeps the hot entities' dense positions
resident, which is what a packed array is good at.

## Next test

Implement the aligned-group variant as a child candidate and re-run
`h05_sparse_component`. If it closes the gap to `archetype`, the
falsification above is about the absence of grouping rather than about packed
arrays.
