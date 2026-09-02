# sparse_set — observed

Run `20260902T092528Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -march=native`, 5 repetitions, median repetition reported.

## The prediction was falsified

`hypothesis.md` predicted that `sparse_set` would win `h05_sparse_component`,
where `Health` is held by about 2% of 200000 entities and both queries name it.
Driving iteration from the rarest set should have made the query cost about
4000 entities instead of 200000.

It came third:

| candidate | h05_sparse_component p50 |
|---|---:|
| `archetype` | 367.3 us |
| `soa` | 1143.6 us |
| `sparse_set` | 1317.1 us |
| `aos` | 1545.6 us |

The driving-set choice works; it is not where the frame goes. Two costs the
hypothesis did not account for:

1. `integrate` walks the `Velocity` dense array — about 190000 entities, since
   `p_velocity` is 0.95 — and for each one reaches `Position` through the
   sparse array. That is one dependent random load per entity into a 200000-
   entry index plus one into the position values. The other three candidates
   walk position and velocity in lockstep.
2. The multi-component query probes the non-driving sets in the driving set's
   dense order, which is unrelated to how those sets are stored. Saving the
   scan buys back less than the scattered probes cost.

Both are properties of the plain sparse set, not of the implementation. They
are what EnTT-style owning groups exist to remove, by keeping the dense arrays
of a group aligned so that a multi-component walk stays sequential. That is not
implemented here, so this result describes the plain sparse set and says
nothing about grouped sparse sets.

## Where it does hold up

`h01_zipf_hotspot` (2193.6 us) — second, ahead of both `aos` and `soa`. A heavy
head means the hot entities' dense positions stay resident, which is the case
the packed array is good at.

## Next test

Implement the aligned-group variant as a child candidate and re-run
`h05_sparse_component`. If it closes the gap to `archetype`, the falsification
above is about the absence of grouping rather than about packed arrays.
