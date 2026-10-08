# query_memo

## Claim under test

About the harness, not the structure: **a candidate cannot be fast by computing
what a query's digest would have been without finding the matching entities.**

`query(required)` returns `sum(digest_entity(required, v))` over the matching
entities, mod 2^64. Every term depends only on that entity, and the sum is the
same every time it is asked for until something changes. So it can be kept as a
running total: subtract an entity's term before a mutation, add it back after.
A structure that does this answers every query in constant time and never
iterates.

`query_memo` is `soa` plus exactly that, for the masks that have been asked for.
`integrate` changes every position at once, so it marks the masks containing
Position stale and the next query recomputes them with an ordinary pass.

## Falsifiable prediction

If the hole is real:

1. it passes verification on every ECS workload, because every answer it gives
   is correct;
2. on `w04_random_access`, the one workload that queries without integrating,
   it is faster than `soa` by the share of the frame the query pass takes;
3. on every workload that integrates, it is no faster than `soa`.

## What would falsify it

Being rejected by the gate, which would mean the gate already checks something
about how an answer is reached. Or being no faster on `w04`, which would mean
the query pass is too small a share of the frame for the hole to matter.
